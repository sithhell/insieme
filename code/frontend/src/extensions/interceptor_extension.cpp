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

#include "insieme/frontend/decl_converter.h"
#include "insieme/frontend/extensions/interceptor_extension.h"
#include "insieme/frontend/utils/name_manager.h"
#include "insieme/frontend/utils/conversion_utils.h"
#include "insieme/core/lang/pointer.h"
#include "insieme/core/transform/manipulation_utils.h"
#include "insieme/core/transform/node_replacer.h"
#include "insieme/core/analysis/ir_utils.h"
#include "insieme/core/types/return_type_deduction.h"
#include "insieme/utils/name_mangling.h"

#include <boost/program_options.hpp>

namespace insieme {
namespace frontend {
namespace extensions {

	InterceptorExtension::InterceptorExtension() {
	}

	boost::optional<std::string> InterceptorExtension::isPrerequisiteMissing(ConversionSetup& setup) const {
		// interceptor needs to be the first extension in the extension list
		if(setup.getExtensions().begin()->get() != this) {
			return boost::optional<std::string>("InterceptorExtension should be the first Extension");
		}
		// prerequisites are met - no prerequisite is missing
		return boost::optional<std::string>();
	}

	namespace {

		core::TypeVariablePtr getTypeVarForTemplateTypeParmType(const core::IRBuilder& builder, const clang::TemplateTypeParmType* parm) {
			return builder.typeVariable(format("T_%d_%d", parm->getDepth(), parm->getIndex()));
		}

		void convertTemplateParameters(clang::TemplateParameterList* tempParamList, const core::IRBuilder& builder,
		                               core::TypeList& templateGenericParams) {
			for(auto tempParam : *tempParamList) {
				if(auto templateParamTypeDecl = llvm::dyn_cast<clang::TemplateTypeParmDecl>(tempParam)) {
					auto canonicalType = llvm::dyn_cast<clang::TemplateTypeParmType>(
							templateParamTypeDecl->getTypeForDecl()->getCanonicalTypeUnqualified().getTypePtr());
					auto typeVar = getTypeVarForTemplateTypeParmType(builder, canonicalType);
					templateGenericParams.push_back(typeVar);
				} else {
					assert_not_implemented() << "NOT ttpt\n";
				}
			}
		}

		std::pair<core::ExpressionPtr, core::TypePtr> generateCallee(conversion::Converter& converter, const clang::Decl* decl) {
			const core::IRBuilder& builder(converter.getIRBuilder());

			auto funDecl = llvm::dyn_cast<clang::FunctionDecl>(decl);
			if(!funDecl) {
				assert_not_implemented();
				return {};
			}

			// convert to literal depending on whether we are dealing with a function or a method
			auto litConverter = [&converter, &builder](const clang::FunctionDecl* funDecl) {
				if(auto methDecl = llvm::dyn_cast<clang::CXXMethodDecl>(funDecl)) {
					return converter.getDeclConverter()->convertMethodDecl(methDecl, builder.parents(), builder.fields(), true).lit;
				}
				return builder.literal(utils::buildNameForFunction(funDecl), converter.convertType(funDecl->getType()));
			};

			core::ExpressionPtr lit = litConverter(funDecl);
			auto retType = lit->getType().as<core::FunctionTypePtr>()->getReturnType();

			// handle non-templated functions
			if(!funDecl->isTemplateInstantiation()) {
				// if defaulted, fix this type to generic type
				auto methDecl = llvm::dyn_cast<clang::CXXMethodDecl>(funDecl);
				if(methDecl && methDecl->isDefaulted()) {
					// check if method of class template specialization
					auto recordDecl = methDecl->getParent();
					if(auto specializedDecl = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(recordDecl)) {
						auto funType = lit->getType().as<core::FunctionTypePtr>();
						auto paramTypes = funType->getParameterTypeList();
						auto genericDecl = specializedDecl->getSpecializedTemplate();
						core::TypeList genericTypeParams;
						convertTemplateParameters(genericDecl->getTemplateParameters(), builder, genericTypeParams);
						auto genericGenType =
							converter.getIRBuilder().genericType(insieme::utils::mangle(specializedDecl->getQualifiedNameAsString()), genericTypeParams);
						auto prevThisType = core::analysis::getReferencedType(paramTypes[0]);
						auto thisType = genericGenType;
						lit = core::transform::replaceAllGen(converter.getNodeManager(), lit, prevThisType, thisType);
					}
				}
				// return the concrete return type but potentially generic literal
				converter.applyHeaderTagging(lit, decl);
				return {lit, retType};
			}


			// first: handle generic template in order to generate generic function
			core::TypeList templateGenericParams, templateConcreteParams;
			auto templateDecl = funDecl->getPrimaryTemplate();
			if(templateDecl) {
				// build map for generic template parameters
				convertTemplateParameters(templateDecl->getTemplateParameters(), builder, templateGenericParams);

				// build list of concrete params for instantiation of this call
				for(auto tempParam: funDecl->getTemplateSpecializationInfo()->TemplateArguments->asArray()) {
					templateConcreteParams.push_back(converter.convertType(tempParam.getAsType()));
				}
			}
			// translate uninstantiated pattern instead of instantiated version
			auto pattern = funDecl->getTemplateInstantiationPattern();
			auto genericFunLit = litConverter(pattern);
			auto genericFunType = genericFunLit->getType().as<core::FunctionTypePtr>();
			genericFunType = builder.functionType(genericFunType->getParameterTypes(), genericFunType->getReturnType(), genericFunType->getKind(),
													builder.types(templateGenericParams));
			auto innerLit = builder.literal(genericFunLit->getValue(), genericFunType);
			converter.applyHeaderTagging(innerLit, decl);

			// cast to concrete type at call site
			auto concreteFunctionType = lit->getType().as<core::FunctionTypePtr>();

			//if we don't need to do any type instantiation
			if(templateConcreteParams.empty()) {
				return {innerLit, concreteFunctionType.getReturnType()};
			}

			concreteFunctionType = builder.functionType(concreteFunctionType->getParameterTypes(), concreteFunctionType->getReturnType(),
				                                        concreteFunctionType->getKind(), builder.types(templateConcreteParams));
			return {builder.callExpr(builder.getLangBasic().getTypeInstantiation(), builder.getTypeLiteral(concreteFunctionType), innerLit),
				    concreteFunctionType->getReturnType()};
		}

		core::CallExprPtr interceptMethodCall(conversion::Converter& converter, const clang::Decl* decl,
			                                  std::function<core::ExpressionPtr(const core::TypePtr&)> thisArgFactory,
			                                  clang::CallExpr::arg_const_range args) {
			if(converter.getHeaderTagger()->isIntercepted(decl)) {
				auto methDecl = llvm::dyn_cast<clang::CXXMethodDecl>(decl);
				if(methDecl) {
					auto calleePair = generateCallee(converter, decl);
					auto convMethodLit = calleePair.first;
					auto funType = convMethodLit->getType().as<core::FunctionTypePtr>();
					auto retType = calleePair.second;
					auto thisArg = thisArgFactory(retType);
					VLOG(2) << "Interceptor: intercepted clang method/constructor call\n" << dumpClang(decl) << "\n";
					auto retCall = utils::buildCxxMethodCall(converter, retType, convMethodLit, thisArg, args);
					return retCall;
				}
			}
			return nullptr;
		}
	}

	core::ExpressionPtr InterceptorExtension::Visit(const clang::Expr* expr, insieme::frontend::conversion::Converter& converter) {
		const core::IRBuilder& builder = converter.getIRBuilder();
		VLOG(3) << "Intercepting Expression\n";
		// decl refs to intercepted functions
		if(auto dr = llvm::dyn_cast<clang::DeclRefExpr>(expr)) {
			auto decl = dr->getDecl();
			if(converter.getHeaderTagger()->isIntercepted(decl)) {
				// translate functions
				if(llvm::dyn_cast<clang::FunctionDecl>(decl)) {
					core::ExpressionPtr lit = generateCallee(converter, decl).first;
					VLOG(2) << "Interceptor: intercepted clang fun\n" << dumpClang(decl) << " -> converted to literal: " << *lit << " of type "
						    << *lit->getType() << "\n";
					return lit;
				}
				// as well as global variables
				if(llvm::dyn_cast<clang::VarDecl>(decl)) {
					auto lit = builder.literal(insieme::utils::mangle(decl->getQualifiedNameAsString()), converter.convertVarType(expr->getType()));
					converter.applyHeaderTagging(lit, decl);
					VLOG(2) << "Interceptor: intercepted clang lit\n" << dumpClang(decl) << " -> converted to literal: " << *lit << " of type "
						    << *lit->getType() << "\n";
					return lit;
				}
				// and enum constants
				if(llvm::dyn_cast<clang::EnumConstantDecl>(decl)) {
					const clang::EnumType* enumType = llvm::dyn_cast<clang::EnumType>(llvm::cast<clang::TypeDecl>(decl->getDeclContext())->getTypeForDecl());
					core::ExpressionPtr exp =
						builder.literal(insieme::utils::mangle(decl->getQualifiedNameAsString()), converter.convertType(clang::QualType(enumType, 0)));
					converter.applyHeaderTagging(exp, decl);
					exp = builder.numericCast(exp, converter.convertType(expr->getType()));
					VLOG(2) << "Interceptor: intercepted clang enum\n" << dumpClang(decl) << " -> converted to expression: " << *exp << " of type "
						    << *exp->getType() << "\n";
					return exp;
				}
			}
		}
		// member calls and their variants
		if(auto construct = llvm::dyn_cast<clang::CXXConstructExpr>(expr)) {
			auto thisFactory = [&](const core::TypePtr& retType){ return core::lang::buildRefTemp(retType); };
			return interceptMethodCall(converter, construct->getConstructor(), thisFactory, construct->arguments());
		}
		if(auto newExp = llvm::dyn_cast<clang::CXXNewExpr>(expr)) {
			if(auto construct = newExp->getConstructExpr()) {
				auto thisFactory = [&](const core::TypePtr& retType){ return builder.undefinedNew(retType); };
				auto ret = interceptMethodCall(converter, construct->getConstructor(), thisFactory, construct->arguments());
				if(ret) return core::lang::buildPtrFromRef(ret);
			}
		}
		if(auto memberCall = llvm::dyn_cast<clang::CXXMemberCallExpr>(expr)) {
			auto thisFactory = [&](const core::TypePtr& retType){ return converter.convertExpr(memberCall->getImplicitObjectArgument()); };
			return interceptMethodCall(converter, memberCall->getCalleeDecl(), thisFactory, memberCall->arguments());
		}
		if(auto operatorCall = llvm::dyn_cast<clang::CXXOperatorCallExpr>(expr)) {
			auto decl = operatorCall->getCalleeDecl();
			if(decl) {
				if(llvm::dyn_cast<clang::CXXMethodDecl>(decl)) {
					auto argList = operatorCall->arguments();
					auto thisFactory = [&](const core::TypePtr& retType){ return converter.convertExpr(*argList.begin()); };
					decltype(argList) remainder(argList.begin()+1, argList.end());
					return interceptMethodCall(converter, decl, thisFactory, remainder);
				}
			}
		}

		return nullptr;
	}


	core::TypePtr InterceptorExtension::Visit(const clang::QualType& typeIn, insieme::frontend::conversion::Converter& converter) {
		clang::QualType type = typeIn;
		const core::IRBuilder& builder = converter.getIRBuilder();
		// handle template parameters of intercepted tagtypes
		if(auto injected = llvm::dyn_cast<clang::InjectedClassNameType>(type.getUnqualifiedType())) {
			auto spec = injected->getInjectedSpecializationType()->getUnqualifiedDesugaredType();
			if(auto tempSpecType = llvm::dyn_cast<clang::TemplateSpecializationType>(spec)) {
				auto key = tempSpecType->getCanonicalTypeUnqualified().getTypePtr();
				assert_true(::containsKey(templateSpecializationMapping, key)) << "Template injected specialization type encountered, but no mapping available";
				return templateSpecializationMapping[key];
			}
		}
		// handle template parameters of intercepted template functions
		if(auto ttpt = llvm::dyn_cast<clang::TemplateTypeParmType>(type.getUnqualifiedType())) {
			return getTypeVarForTemplateTypeParmType(builder, ttpt);
		}
		// handle class, struct and union interception
		if(auto tt = llvm::dyn_cast<clang::TagType>(type->getCanonicalTypeUnqualified())) {
			// do not intercept enums, they are simple
			if(tt->isEnumeralType()) return nullptr;
			auto decl = tt->getDecl();
			if(converter.getHeaderTagger()->isIntercepted(decl)) {
				auto genType = converter.getIRBuilder().genericType(utils::getNameForTagDecl(converter, decl).first);

				// for templates: convert template arguments to generic type parameters
				if(auto templateDecl = llvm::dyn_cast<clang::ClassTemplateSpecializationDecl>(decl)) {
					auto genericDecl = templateDecl->getSpecializedTemplate();

					// store class template template type parameters in map
					core::TypeList genericTypeParams;
					convertTemplateParameters(genericDecl->getTemplateParameters(), builder, genericTypeParams);
					auto genericGenType = converter.getIRBuilder().genericType(insieme::utils::mangle(decl->getQualifiedNameAsString()), genericTypeParams);

					// store injected class name in specialization map
					auto injected = genericDecl->getInjectedClassNameSpecialization()->getUnqualifiedDesugaredType();
					if(auto tempSpecType = llvm::dyn_cast<clang::TemplateSpecializationType>(injected)) {
						templateSpecializationMapping[tempSpecType->getCanonicalTypeUnqualified().getTypePtr()] = genericGenType;
					}

					// build concrete genType
					core::TypeList concreteTypeArguments;
					for(auto arg: templateDecl->getTemplateArgs().asArray()) {
						// TODO non-type
						concreteTypeArguments.push_back(converter.convertType(arg.getAsType()));
					}
					genType = converter.getIRBuilder().genericType(insieme::utils::mangle(decl->getQualifiedNameAsString()), concreteTypeArguments);
				}

				converter.applyHeaderTagging(genType, decl);
				VLOG(2) << "Interceptor: intercepted clang type\n" << dumpClang(decl) << " -> converted to generic type: " << *genType << "\n";
				return genType;
			}
		}
		return nullptr;
	}

	bool InterceptorExtension::FuncDeclVisit(const clang::FunctionDecl* decl, insieme::frontend::conversion::Converter& converter) {
		if(converter.getHeaderTagger()->isIntercepted(decl)) {
			return false;
		}
		return true;
	}

	FrontendExtension::flagHandler InterceptorExtension::registerFlag(boost::program_options::options_description& options) {
		// create lambda
		auto lambda = [&](const ConversionJob& job) {
			// check if the default activated plugins have been deactivated manually
			if(job.hasOption(frontend::ConversionJob::NoDefaultExtensions)) { return false; }
			return true;
		};
		return lambda;
	}


} // extensions
} // frontend
} // insieme
