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

#include <functional>

#include <boost/format.hpp>

#include "insieme/frontend/convert.h"

#include "insieme/frontend/analysis/prunable_decl_visitor.h"
#include "insieme/frontend/clang.h"
#include "insieme/frontend/stmt_converter.h"
#include "insieme/frontend/expr_converter.h"
#include "insieme/frontend/type_converter.h"
#include "insieme/frontend/utils/clang_cast.h"
#include "insieme/frontend/utils/name_manager.h"
#include "insieme/frontend/utils/error_report.h"
#include "insieme/frontend/utils/debug.h"
#include "insieme/frontend/utils/header_tagger.h"
#include "insieme/frontend/utils/source_locations.h"
#include "insieme/frontend/utils/macros.h"
#include "insieme/frontend/omp/omp_annotation.h"

#include "insieme/utils/container_utils.h"
#include "insieme/utils/numeric_cast.h"
#include "insieme/utils/logging.h"
#include "insieme/utils/map_utils.h"
#include "insieme/utils/set_utils.h"
#include "insieme/utils/timer.h"
#include "insieme/utils/assert.h"
#include "insieme/utils/functional_utils.h"
#include "insieme/utils/progress_bar.h"

#include "insieme/core/analysis/ir_utils.h"
#include "insieme/core/annotations/naming.h"
#include "insieme/core/annotations/source_location.h"
#include "insieme/core/arithmetic/arithmetic_utils.h"
#include "insieme/core/datapath/datapath.h"
#include "insieme/core/dump/text_dump.h"
#include "insieme/core/encoder/lists.h"
#include "insieme/core/ir_class_info.h"
#include "insieme/core/ir_program.h"
#include "insieme/core/lang/basic.h"
#include "insieme/core/lang/enum_extension.h"
#include "insieme/core/lang/ir++_extension.h"
#include "insieme/core/lang/simd_vector.h"
#include "insieme/core/lang/static_vars.h"
#include "insieme/core/transform/manipulation.h"
#include "insieme/core/transform/manipulation_utils.h"
#include "insieme/core/transform/node_replacer.h"
#include "insieme/core/transform/node_replacer.h"
#include "insieme/core/types/subtyping.h"

#include "insieme/annotations/c/extern.h"
#include "insieme/annotations/c/extern_c.h"
#include "insieme/annotations/c/include.h"

using namespace clang;
using namespace insieme;

namespace insieme {
namespace frontend {
	// ----------- conversion -----------
	tu::IRTranslationUnit convert(core::NodeManager& manager, const path& unit, const ConversionSetup& setup) {
		// just delegate operation to converter
		TranslationUnit tu(manager, unit, setup);
		conversion::Converter c(manager, tu, setup);
		// add them and fire the conversion
		return c.convert();
	}
}
}

namespace insieme {
namespace frontend {
namespace conversion {

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//						CONVERTER
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	Converter::Converter(core::NodeManager& mgr, const TranslationUnit& tu, const ConversionSetup& setup)
		: translationUnit(tu), convSetup(setup), pragmaMap(translationUnit.pragmas_begin(), translationUnit.pragmas_end()),
		  mgr(mgr), builder(mgr), feIR(mgr, getCompiler().isCXX()), irTranslationUnit(mgr) {
		if (translationUnit.isCxx()) {
			typeConvPtr = std::make_shared<CXXTypeConverter>(*this);
			exprConvPtr = std::make_shared<CXXExprConverter>(*this);
			stmtConvPtr = std::make_shared<CXXStmtConverter>(*this);
		}
		else {
			typeConvPtr = std::make_shared<CTypeConverter>(*this);
			exprConvPtr = std::make_shared<CExprConverter>(*this);
			stmtConvPtr = std::make_shared<CStmtConverter>(*this);
		}
		// tag the translation unit with as C++ if case
		irTranslationUnit.setCXX(translationUnit.isCxx());
		assert_true(irTranslationUnit.isEmpty()) << "the ir translation unit is not empty, should be before we start";
	}

	void Converter::printDiagnosis(const clang::SourceLocation& loc) {
		clang::Preprocessor& pp = getPreprocessor();
		// print warnings and errors:
		while(!warnings.empty()) {
			if(!getConversionSetup().hasOption(ConversionSetup::NoWarnings)) {
				if(getSourceManager().isLoadedSourceLocation(loc)) {
					std::cerr << "\n\nloaded location:\n";
					std::cerr << "\t" << *warnings.begin() << std::endl;
				} else {
					std::cerr << "\n\n";
					utils::clangPreprocessorDiag(pp, loc, clang::DiagnosticsEngine::Level::Warning, *warnings.begin());
				}
			}
			warnings.erase(warnings.begin());
		}
	}

	tu::IRTranslationUnit Converter::convert() {
		assert_true(getCompiler().getASTContext().getTranslationUnitDecl());
	
		// collect all type definitions
		auto declContext = clang::TranslationUnitDecl::castToDeclContext(getCompiler().getASTContext().getTranslationUnitDecl());
	
		struct TypeVisitor : public analysis::PrunableDeclVisitor<TypeVisitor> {
	
			Converter& converter;
			TypeVisitor(Converter& converter) : converter(converter) {}
		
			Converter& getConverter() {
				return converter;
			}
		
			void VisitRecordDecl(const clang::RecordDecl* typeDecl) {
				if(!typeDecl->isCompleteDefinition()) {
					return;
				}
				if(typeDecl->isDependentType()) {
					return;
				}
			
				// we do not convert templates or partial specialized classes/functions, the full
				// type will be found and converted once the instantiation is found
				converter.trackSourceLocation(typeDecl);
				converter.convertTypeDecl(typeDecl);
				converter.untrackSourceLocation();
			}
			// typedefs and typealias
			void VisitTypedefNameDecl(const clang::TypedefNameDecl* typedefDecl) {
				if(!typedefDecl->getTypeForDecl()) {
					return;
				}
			
				// get contained type
				converter.trackSourceLocation(typedefDecl);
				converter.convertTypeDecl(typedefDecl);
				converter.untrackSourceLocation();
			}
		} typeVisitor(*this);
		typeVisitor.traverseDeclCtx(declContext);
	
		// collect all global declarations
		struct GlobalVisitor : public analysis::PrunableDeclVisitor<GlobalVisitor> {
	
			Converter& converter;
			GlobalVisitor(Converter& converter) : converter(converter) {}
		
			Converter& getConverter() {
				return converter;
			}
		
			void VisitVarDecl(const clang::VarDecl* var) {
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
				//converter.lookUpVariable(var);
				converter.untrackSourceLocation();
			}
		} varVisitor(*this);
		varVisitor.traverseDeclCtx(declContext);
	
		// collect all global declarations
		struct FunctionVisitor : public analysis::PrunableDeclVisitor<FunctionVisitor> {
	
			Converter& converter;

			bool externC;
			FunctionVisitor(Converter& converter, bool Ccode)
				: converter(converter), externC(Ccode) {
			}
		
			Converter& getConverter() {
				return converter;
			}
		
			void VisitLinkageSpec(const clang::LinkageSpecDecl* link) {
				bool isC =  link->getLanguage() == clang::LinkageSpecDecl::lang_c;
				FunctionVisitor vis(converter, isC);
				vis.traverseDeclCtx(llvm::cast<clang::DeclContext>(link));
			}
		
			void VisitFunctionDecl(const clang::FunctionDecl* funcDecl) {
				if(funcDecl->isTemplateDecl() && !funcDecl->isFunctionTemplateSpecialization()) {
					return;
				}
			
				converter.trackSourceLocation(funcDecl);
				core::ExpressionPtr irFunc = converter.convertFunctionDecl(funcDecl);
				converter.untrackSourceLocation();
				if(externC) {
					annotations::c::markAsExternC(irFunc.as<core::LiteralPtr>());
				}
			}
		} funVisitor(*this, false);
		funVisitor.traverseDeclCtx(declContext);
		
		//std::cout << " ==================================== " << std::endl;
		//std::cout << getIRTranslationUnit() << std::endl;
		//std::cout << " ==================================== " << std::endl;
	
		// that's all
		return irTranslationUnit;
	}

	core::ExpressionPtr Converter::convertFunctionDecl(const clang::FunctionDecl* funcDecl, bool symbolic) {
		core::TypePtr funType = convertType(funcDecl->getType());
		dumpColor(funType);
		
		return core::ExpressionPtr();
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//						SOURCE LOCATIONS
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	void Converter::trackSourceLocation(const clang::Decl* decl) {
		if(const clang::ClassTemplateSpecializationDecl* spet = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(decl)) {
			lastTrackableLocation.push(spet->getPointOfInstantiation());
		} else {
			lastTrackableLocation.push(decl->getLocation());
		}
	}

	void Converter::trackSourceLocation(const clang::Stmt* stmt) {
		lastTrackableLocation.push(stmt->getLocStart());
	}

	void Converter::untrackSourceLocation() {
		assert_false(lastTrackableLocation.empty());
		lastTrackableLocation.pop();
	}

	std::string Converter::getLastTrackableLocation() const {
		if(!lastTrackableLocation.empty()) {
			return utils::location(lastTrackableLocation.top(), getSourceManager());
		} else {
			return "ERROR: unable to identify last input code location ";
		}
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//						ANNOTATIONS
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	core::ExpressionPtr Converter::attachFuncAnnotations(const core::ExpressionPtr& node, const clang::FunctionDecl* funcDecl) {
		
		// ----------------------------------- Add annotations to this function -------------------------------------------
		pragma::attachPragma(core::NodeList({node.as<core::NodePtr>()}), funcDecl, *this);

		// -------------------------------------------------- C NAME ------------------------------------------------------
		// check for overloaded operator "function" (normal function has kind OO_None)
		if(funcDecl->isOverloadedOperator()) {
			clang::OverloadedOperatorKind operatorKind = funcDecl->getOverloadedOperator();
			// in order to get a real name we have to use the getOperatorSpelling method
			// otherwise we get names like operator30 and this is for instance not
			// suitable when using STL containers (e.g., map needs < operator)...
			string operatorAsString = clang::getOperatorSpelling(operatorKind);
			core::annotations::attachName(node, ("operator" + operatorAsString));
		} else if(!funcDecl->getNameAsString().empty()) {
			// annotate with the C name of the function
			core::annotations::attachName(node, (utils::buildNameForFunction(funcDecl)));
		}
		if(core::annotations::hasAttachedName(node)) { VLOG(2) << "attachedName: " << core::annotations::getAttachedName(node); }

		// ---------------------------------------- SourceLocation Annotation ---------------------------------------------
		// for each entry function being converted we register the location where it was originally defined in the C program
		std::pair<SourceLocation, SourceLocation> loc{funcDecl->getLocStart(), funcDecl->getLocEnd()};
		fe::pragma::PragmaStmtMap::DeclMap::const_iterator fit = pragmaMap.getDeclarationMap().find(funcDecl);

		if(fit != pragmaMap.getDeclarationMap().end()) {
			// the statement has a pragma associated with, when we do the rewriting, the pragma needs to be overwritten
			loc.first = fit->second->getStartLocation();
		}

		utils::attachLocationFromClang(node, getSourceManager(), loc.first, loc.second);

		return node;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//						BASIC CONVERSIONS
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	core::ExpressionPtr Converter::convertExpr(const clang::Expr* expr) const {
		assert_true(expr) << "Calling convertExpr with a NULL pointer";
		return exprConvPtr->Visit(const_cast<Expr*>(expr));
	}

	core::StatementPtr Converter::convertStmt(const clang::Stmt* stmt) const {
		assert_true(stmt) << "Calling convertStmt with a NULL pointer";
		return stmtutils::tryAggregateStmts(builder, stmtConvPtr->Visit(const_cast<Stmt*>(stmt)));
	}

	core::TypePtr Converter::convertType(const clang::QualType& type) {
		return typeConvPtr->convert(type);
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//						TYPE DECLS
	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	void Converter::convertTypeDecl(const clang::TypeDecl* decl) {
		assert_not_implemented();
		//core::TypePtr res = nullptr;
		//for(auto extension : this->getConversionSetup().getExtensions()) {
		//	auto result = extension->Visit(decl, *this).as<core::TypePtr>();
		//}

		//if(!res) {
		//	// trigger the actual conversion
		//	res = convertType(decl->getTypeForDecl()->getCanonicalTypeInternal());
		//}

		//// frequently structs and their type definitions have the same name
		//// in this case symbol == res and should be ignored
		//if(const clang::TypedefDecl* typedefDecl = llvm::dyn_cast<clang::TypedefDecl>(decl)) {
		//	auto symbol = builder.genericType(typedefDecl->getQualifiedNameAsString());
		//	if(res != symbol && res.isa<core::NamedCompositeTypePtr>()) { // also: skip simple type-defs
		//		getIRTranslationUnit().addType(symbol, res);
		//	}
		//}

		//// handle pragmas
		//core::NodeList list({res});
		//list = pragma::attachPragma(list, decl, *this);
		//assert_eq(1, list.size()) << "More than 1 node present";
		//res = list.front().as<core::TypePtr>();

		//for(auto extension : this->getConversionSetup().getExtensions()) {
		//	extension->PostVisit(decl, res, *this);
		//}
	}
	
} // End conversion namespace
} // End frontend namespace
} // End insieme namespace
