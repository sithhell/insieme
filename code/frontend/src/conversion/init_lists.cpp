/**
 * Copyright (c) 2002-2016 Distributed and Parallel Systems Group,
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

#include "insieme/frontend/conversion/init_lists.h"

#include "insieme/core/ir_builder.h"
#include "insieme/core/analysis/ir_utils.h"
#include "insieme/core/analysis/type_utils.h"
#include "insieme/core/lang/pointer.h"
#include "insieme/core/transform/materialize.h"

#include "insieme/frontend/converter.h"
#include "insieme/frontend/state/function_manager.h"
#include "insieme/frontend/utils/macros.h"


namespace insieme {
namespace frontend {
namespace conversion {

	core::ExpressionPtr convertCxxStdInitializerListExpr(Converter& converter, const clang::CXXStdInitializerListExpr* stdInitListExpr) {
		core::ExpressionPtr retIr;
		auto& builder = converter.getIRBuilder();

		//convert sub expression and types
		auto subEx = converter.convertExpr(stdInitListExpr->getSubExpr());
		auto subExType = stdInitListExpr->getSubExpr()->getType().getTypePtr();
		auto initListIRType = converter.convertType(stdInitListExpr->getType());
		auto recordType = llvm::dyn_cast<clang::RecordType>(stdInitListExpr->getType().getTypePtr()->getUnqualifiedDesugaredType());
		auto recordDecl = recordType->getAsCXXRecordDecl();
		frontend_assert(recordType && recordDecl) << "failed to get the std::initializer_list type declaration.";
		auto thisVar = core::lang::buildRefTemp(initListIRType);

		auto& refExt = converter.getNodeManager().getLangExtension<core::lang::ReferenceExtension>();

		auto buildMemberAccess = [&builder, &refExt](const core::ExpressionPtr& thisVar, const std::string memberName, const core::TypePtr& memberType) {
			return builder.callExpr(refExt.getRefMemberAccess(), builder.deref(thisVar), builder.getIdentifierLiteral(memberName), builder.getTypeLiteral(memberType));
		};

		//extract size of sub expr
		frontend_assert(llvm::isa<clang::ConstantArrayType>(subExType)) << "std::initializer_list sub expression has no constant size array type.";
		auto numElements = llvm::cast<clang::ConstantArrayType>(subExType)->getSize().getSExtValue();

		// The pointer type of our member field. Will be set during ctor creation and used for dtor creation below
		core::TypePtr ptrType;
		//generate and insert mandatory std::initializer_list<T> ctors
		for(auto ctorDecl : recordDecl->ctors()) {
			core::LiteralPtr ctorLiteral = converter.getFunMan()->lookup(ctorDecl);
			core::LambdaExprPtr lam;
			if(ctorDecl->getNumParams() == 2) {
				//ctor decl: constexpr initializer_list<T>(T* x, size_t s)
				//create list of arguments (this type, array sub expression, and size of array)
				core::ExpressionList args { thisVar, core::lang::buildPtrFromArray(subEx),
				builder.numericCast(builder.uintLit(numElements), builder.getLangBasic().getUInt8()) };
				auto funType = ctorLiteral->getType().as<core::FunctionTypePtr>();
				core::VariableList params;
				for(const core::TypePtr& type : funType->getParameterTypes()) {
					params.push_back(builder.variable(core::transform::materialize(type)));
				}

				ptrType = core::analysis::getReferencedType(params[1]);
				auto memberType = core::lang::PointerType(ptrType).getElementType();
				std::map<string, core::NodePtr> symbols {
					{ "_m_array", buildMemberAccess(params[0], "_M_array", ptrType) },
					{ "_m_length", buildMemberAccess(params[0], "_M_len", builder.getLangBasic().getUInt8()) },
					{ "_member_type", memberType },
					{ "_array", builder.deref(params[1]) },
					{ "_length", builder.deref(params[2]) },
				};

				// check whether we need to copy the elements or can simply assign them
				bool copyElements = false;
				auto tuIt = converter.getIRTranslationUnit().getTypes().find(memberType.as<core::GenericTypePtr>());
				if(tuIt != converter.getIRTranslationUnit().getTypes().cend() && !core::analysis::isTrivial(tuIt->second)) {
					copyElements = true;
				}

				auto body = builder.parseStmt(std::string("") + R"(
					{
						var uint<inf> array_len = num_cast(_length,type_lit(uint<inf>));
						_m_array = ptr_const_cast(ptr_from_array(ref_new(type_lit(array<_member_type,#array_len>))),type_lit(t));
						_m_length = _length;
						for(int<8> it = 0l .. num_cast(_length, type_lit(int<8>))) { )" +
						(copyElements ?
								R"( ref_assign(ptr_subscript(ptr_const_cast(*_m_array, type_lit(f)), it), ref_cast(ptr_subscript(_array, it), type_lit(t), type_lit(f), type_lit(cpp_ref))); )" :
								R"( ptr_subscript(ptr_const_cast(*_m_array, type_lit(f)), it) = *ptr_subscript(_array, it); )") + R"(
						}
					}
				)", symbols);

				//add the ctor implementation to the IR TU --> std::initializer_list<T>(T* t, size_t s) { }
				lam = builder.lambdaExpr(funType, params, body);
				//build call to the specific constructor

				retIr = builder.callExpr(funType->getReturnType(), ctorLiteral, args);

			} else if(ctorDecl->isDefaultConstructor()) {
				// ctor decl: constexpr initializer_list<T>()
				auto funType = ctorLiteral->getType().as<core::FunctionTypePtr>();
				core::VariableList params { builder.variable(core::transform::materialize(funType->getParameterTypes()[0])) };

				// create the body of the default ctor
				std::map<string, core::NodePtr> symbols {
					{ "_m_length", buildMemberAccess(params[0], "_M_len", builder.getLangBasic().getUInt8()) },
				};
				auto body = builder.parseStmt(R"({
					_m_length = 0ul;
				})", symbols);

				//create default constructor
				lam = builder.lambdaExpr(funType, params, body);
			}
			if(lam) converter.getIRTranslationUnit().addFunction(ctorLiteral, lam);
		}

		//generate dtor
		frontend_assert(ptrType) << "Pointer type hasn't been set already while creating the ctor bodies";
		auto dtorThisType = builder.refType(initListIRType);
		auto funType = builder.functionType(toVector<core::TypePtr>(dtorThisType), dtorThisType, core::FunctionKind::FK_DESTRUCTOR);
		core::LiteralPtr dtorLiteral = builder.getLiteralForDestructor(funType);
		core::VariableList params { builder.variable(core::transform::materialize(funType->getParameterTypes()[0])) };

		// create the body of the default ctor
		std::map<string, core::NodePtr> symbols {
			{ "_m_array", buildMemberAccess(params[0], "_M_array", ptrType) },
			{ "_m_length", buildMemberAccess(params[0], "_M_len", builder.getLangBasic().getUInt8()) },
		};
		auto body = builder.parseStmt(R"({
			if(*_m_length != 0ul) {
				ref_delete(ptr_to_ref(ptr_const_cast(*_m_array, type_lit(f))));
			}
		})", symbols);

		//create and add destructor
		auto lam = builder.lambdaExpr(funType, params, body);
		converter.getIRTranslationUnit().addFunction(dtorLiteral, lam);

		//now we have to replace the whole type to make sure the dtor is present
		auto key = initListIRType.as<core::GenericTypePtr>();
		const auto& oldRecord = converter.getIRTranslationUnit().getTypes().at(key)->getStruct();
		frontend_assert(oldRecord) << "Record has not been stored in TU previously";
		auto newRecord = builder.structType(oldRecord->getName(), oldRecord->getParents(), oldRecord->getFields(), oldRecord->getConstructors(),
		                                    dtorLiteral, builder.boolValue(false), oldRecord->getMemberFunctions(), oldRecord->getPureVirtualMemberFunctions());
		converter.getIRTranslationUnit().replaceType(key, newRecord);

		//return call to special constructor
		frontend_assert(retIr) << "failed to convert std::initializer_list expression.";
		return retIr;
	}

} // End namespace utils
} // End namespace frontend
} // End namespace insieme
