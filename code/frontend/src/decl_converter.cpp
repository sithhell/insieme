/**
 * Copyright (c) 2002-2015 Distributed and Parallel Systems Group,
 *                Institute of Computer Science,
 *               University of Innsbruck, Austria
 *
 * This file is part of the INSIEME Compiler and Runtime System.
 *
 * We provide the software of this file (below described as "INSIEME")
 * under GPL Version 3.0 on an AS IS basis, and do not warrant its
 * validity or performance.  We reserve the right to update, modify,
 * or discontinue this software at any time.  We shall have no
 * obligation to supply such updates or modifications or any other
 * form of support to you.
 *
 * If you require different license terms for your intended use of the
 * software, e.g. for proprietary commercial or industrial use, please
 * contact us at:
 *                   insieme@dps.uibk.ac.at
 *
 * We kindly ask you to acknowledge the use of this software in any
 * publication or other disclosure of results by referring to the
 * following citation:
 *
 * H. Jordan, P. Thoman, J. Durillo, S. Pellegrini, P. Gschwandtner,
 * T. Fahringer, H. Moritsch. A Multi-Objective Auto-Tuning Framework
 * for Parallel Codes, in Proc. of the Intl. Conference for High
 * Performance Computing, Networking, Storage and Analysis (SC 2012),
 * IEEE Computer Society Press, Nov. 2012, Salt Lake City, USA.
 *
 * All copyright notices must be kept intact.
 *
 * INSIEME depends on several third party software packages. Please
 * refer to http://www.dps.uibk.ac.at/insieme/license.html for details
 * regarding third party software licenses.
 */

#include "insieme/frontend/decl_converter.h"

#include "insieme/frontend/converter.h"
#include "insieme/frontend/state/function_manager.h"
#include "insieme/frontend/state/variable_manager.h"
#include "insieme/frontend/utils/name_manager.h"

#include "insieme/core/ir.h"
#include "insieme/core/ir_builder.h"
#include "insieme/core/lang/pointer.h"

#include "insieme/annotations/c/extern.h"
#include "insieme/annotations/c/extern_c.h"

namespace insieme {
namespace frontend {
namespace conversion {

	DeclConverter::DeclConverter(Converter& converter) : converter(converter), builder(converter.getIRBuilder()) {}
	
	// Converters -----------------------------------------------------------------------------------------------------

	DeclConverter::ConvertedVarDecl DeclConverter::convertVarDecl(const clang::VarDecl* varDecl) const {
		auto irType = converter.convertVarType(varDecl->getType());
		auto var = builder.variable(irType);
		if(varDecl->getInit()) {
			return {var, converter.convertExpr(varDecl->getInit())};
		} else {
			return {var, {}};
		}
	}

	core::LambdaExprPtr DeclConverter::convertFunctionDecl(const core::FunctionTypePtr& funType, const clang::FunctionDecl* funcDecl) const {
		core::LiteralPtr funLit = builder.literal(funcDecl->getNameAsString(), funType);

		if(funcDecl->hasBody()) {
			converter.getVarMan()->pushScope(false);
			core::VariableList params;
			for(auto param : funcDecl->parameters()) {
				auto irParam = convertVarDecl(param);
				params.push_back(irParam.first);
				converter.getVarMan()->insert(param, irParam.first);
			}
			auto body = converter.convertStmt(funcDecl->getBody());
			converter.getVarMan()->popScope();
			auto funExp = builder.lambdaExpr(funType, params, body);
			return funExp;
		} else {
			return core::LambdaExprPtr();
		}

		assert_not_implemented();
		return core::LambdaExprPtr();
	}
	
	// Visitors -------------------------------------------------------------------------------------------------------
	
	void DeclConverter::VisitDeclContext(const clang::DeclContext* context) {
		for(auto decl : context->decls()) {
			Visit(decl);
		}
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//							Structs, Unions and Classes 
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	void DeclConverter::VisitRecordDecl(const clang::RecordDecl* typeDecl) {
		if(!typeDecl->isCompleteDefinition()) { return; }
		if(typeDecl->isDependentType()) { return; }

		// we do not convert templates or partial specialized classes/functions, the full
		// type will be found and converted once the instantiation is found
		converter.trackSourceLocation(typeDecl);
		//convertTypeDecl(typeDecl);
		converter.untrackSourceLocation();
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//							Typedefs and type aliases
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	void DeclConverter::VisitTypedefNameDecl(const clang::TypedefNameDecl* typedefDecl) {
		if(!typedefDecl->getTypeForDecl()) { return; }

		// get contained type
		converter.trackSourceLocation(typedefDecl);
		//convertTypeDecl(typedefDecl);
		converter.untrackSourceLocation();
	}
	
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//							Variable declarations
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	void DeclConverter::VisitVarDecl(const clang::VarDecl* var) {
		// variables to be skipped
		if(!var->hasGlobalStorage()) {
			return;
		}
		if(var->hasExternalStorage()) {
			return;
		}
		if(var->isStaticLocal()) {
			return;
		}
			
		converter.trackSourceLocation(var);
		auto converted = convertVarDecl(var);
		auto globalLit = builder.literal(converted.first->getType(), utils::getNameForGlobal(var, converter.getSourceManager()));
		globalLit = pragma::handlePragmas({globalLit}, var, converter).front().as<core::LiteralPtr>();
		converter.getVarMan()->insert(var, globalLit);
		// handle pragmas attached to decls
		converter.untrackSourceLocation();
	}
	
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//					 Linkage spec, e.g. extern "C"
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	void DeclConverter::VisitLinkageSpec(const clang::LinkageSpecDecl* link) {
		bool prevExternC = inExternC;
		inExternC = link->getLanguage() == clang::LinkageSpecDecl::lang_c;
		VisitDeclContext(llvm::cast<clang::DeclContext>(link));
		inExternC = prevExternC;
	}
	
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//					 Function declarations
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	void DeclConverter::VisitFunctionDecl(const clang::FunctionDecl* funcDecl) {
		if(funcDecl->isTemplateDecl() && !funcDecl->isFunctionTemplateSpecialization()) { return; }
		converter.trackSourceLocation(funcDecl);
		auto funType = converter.convertType(funcDecl->getType()).as<core::FunctionTypePtr>();
		core::LiteralPtr irLit = builder.literal(funcDecl->getNameAsString(), funType);
		if(inExternC) { annotations::c::markAsExternC(irLit); }
		// insert first before converting the body
		converter.getFunMan()->insert(funcDecl, irLit);
		core::LambdaExprPtr irFunc = convertFunctionDecl(funType, funcDecl);
		converter.untrackSourceLocation();
		if(irFunc) converter.getIRTranslationUnit().addFunction(irLit, irFunc);
	}

} // End conversion namespace
} // End frontend namespace
} // End insieme namespace