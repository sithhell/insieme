/**
 * Copyright (c) 2002-2017 Distributed and Parallel Systems Group,
 *                Institute of Computer Science,
 *               University of Innsbruck, Austria
 *
 * This file is part of the INSIEME Compiler and Runtime System.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
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
 */

#include "insieme/frontend/extensions/frontend_cleanup_extension.h"

#include "insieme/core/analysis/ir++_utils.h"
#include "insieme/core/analysis/ir_utils.h"
#include "insieme/core/checks/full_check.h"
#include "insieme/core/ir.h"
#include "insieme/core/ir_visitor.h"
#include "insieme/core/lang/enum.h"
#include "insieme/core/lang/compound_operators.h"
#include "insieme/core/pattern/ir_generator.h"
#include "insieme/core/pattern/ir_pattern.h"
#include "insieme/core/pattern/pattern_utils.h"
#include "insieme/core/pattern/rule.h"
#include "insieme/core/transform/manipulation.h"
#include "insieme/core/transform/manipulation_utils.h"
#include "insieme/core/transform/node_mapper_utils.h"
#include "insieme/core/transform/node_replacer.h"
#include "insieme/core/tu/ir_translation_unit.h"
#include "insieme/core/tu/ir_translation_unit_io.h"

#include "insieme/annotations/c/include.h"
#include "insieme/annotations/data_annotations.h"

#include "insieme/frontend/converter.h"
#include "insieme/frontend/annotations/frontend_annotations.h"
#include "insieme/frontend/utils/conversion_utils.h"
#include "insieme/frontend/utils/frontend_inspire_module.h"
#include "insieme/frontend/utils/memalloc.h"
#include "insieme/frontend/utils/stmt_wrapper.h"

#include "insieme/utils/name_mangling.h"

#include "insieme/utils/assert.h"

#include <functional>

namespace insieme {
namespace frontend {
namespace extensions {

	using namespace core;
	namespace icp = pattern;
	namespace irp = pattern::irp;

	namespace {
		//// TU steps ------------------------------------------------------------------------------------------------------------------------------------

		//////////////////////////////////////////////////////////////////////////
		// Removes member functions from types which are template instantiations and are not called by ourselves. These can't be called in any other way anyways.
		// =======================================================================
		void purgeUncalledTemplateInstantiationMemberFunctions(core::tu::IRTranslationUnit& tu) {
			auto& mgr = tu.getNodeManager();
			core::IRBuilder builder(mgr);
			auto& functions = tu.getFunctions();

			// we need a fixpoint iteration here which applies the transformation until we don't perform any deletions anymore
			while(true) {
				bool changed = false;
				// first we collect all called literals, which are calling template instantiations
				insieme::utils::set::PointerSet<core::LiteralPtr> calledLiterals;
				for(const auto& fun : functions) {
					core::visitDepthFirstOnce(fun.second, [&](const core::CallExprPtr& callExpr) {
						const auto& callee = callExpr.getFunctionExpr().isa<core::LiteralPtr>();
						if(callee && callee.hasAttachedValue<annotations::TemplateInstantiationMarkerAnnotation>()) { calledLiterals.insert(callee); }
					});
				}

				// iterate over all tag types and check their member functions. if they are template instantiations and are not in the list of called functions, remove them
				for(const auto& it : tu.getTypes()) {
					const auto& tagType = it.second;
					core::MemberFunctionList memFuns = tagType->getRecord()->getMemberFunctions()->getElements();
					memFuns.erase(std::remove_if(memFuns.begin(), memFuns.end(), [&](const core::MemberFunctionPtr& memFun) {
						const auto& impl = memFun->getImplementation().isa<core::LiteralPtr>();
						return impl && impl.hasAttachedValue<annotations::TemplateInstantiationMarkerAnnotation>() && calledLiterals.find(impl) == calledLiterals.end();
					}), memFuns.end());

					// if we removed some member function(s)
					if(memFuns.size() != tagType->getRecord()->getMemberFunctions().size()) {
						changed = true;
						// we create a replacement type
						auto newType = core::transform::replaceNode(mgr, core::TagTypeAddress(tagType)->getRecord()->getMemberFunctions(),
						                                            builder.memberFunctions(memFuns)).as<core::TagTypePtr>();
						// and actually replace it in the TU
						tu.replaceType(it.first, newType);
					}
				}

				// remove all uncalled template instantiations from the TU
				for(auto it = functions.begin(); it != functions.end(); ) {
					const auto& impl = it->first;
					if(impl.hasAttachedValue<annotations::TemplateInstantiationMarkerAnnotation>() && calledLiterals.find(impl) == calledLiterals.end()) {
						changed = true;
						it = functions.erase(it);
					} else {
						++it;
					}
				}

				// if we didn't change anything anymore, we are done
				if(!changed) {
					return;
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// Simplify calls which have no side effects if their return value is clearly unused (in compound)
		// =======================================================================
		core::ExpressionPtr simplifyExpressionsInCompoundStatements(const core::ExpressionPtr& ir) {
			auto& mgr = ir->getNodeManager();
			core::IRBuilder builder(mgr);
			auto& feExt = mgr.getLangExtension<utils::FrontendInspireModule>();
			auto& rExt = mgr.getLangExtension<core::lang::ReferenceExtension>();
			auto& cExt = mgr.getLangExtension<core::lang::CompoundOpsExtension>();
			auto& pExt = mgr.getLangExtension<core::lang::PointerExtension>();

			return core::transform::transformBottomUpGen(ir, [&](const core::CompoundStmtPtr& compound) {
				StatementList newStmts;
				for(auto stmt : compound.getStatements()) {
					auto replacement = stmt;
					if(feExt.isCallOfCStyleAssignment(stmt) || feExt.isCallOfCxxStyleAssignment(stmt)) {
						replacement = builder.assign(core::analysis::getArgument(stmt, 0), core::analysis::getArgument(stmt, 1));
					}
					else if(cExt.isCallOfCompPrefixInc(stmt)) {
						replacement = builder.preInc(core::analysis::getArgument(stmt, 0));
					}
					else if(cExt.isCallOfCompPrefixDec(stmt)) {
						replacement = builder.preDec(core::analysis::getArgument(stmt, 0));
					}
					else if(pExt.isCallOfCxxStylePtrPreInc(stmt)) {
						replacement = builder.callExpr(pExt.getPtrPreInc(), core::analysis::getArgument(stmt, 0));
					}
					else if(pExt.isCallOfCxxStylePtrPreDec(stmt)) {
						replacement = builder.callExpr(pExt.getPtrPreDec(), core::analysis::getArgument(stmt, 0));
					}
					else if(rExt.isCallOfRefDeref(stmt)) {
						replacement = core::analysis::getArgument(stmt, 0);
					}
					else if(core::lang::isAnyRefCast(stmt)) {
						replacement = core::analysis::getArgument(stmt, 0);
					}
					else if(feExt.isCallOfBoolToInt(stmt)) {
						replacement = core::analysis::getArgument(stmt, 0);
					}
					else if(feExt.isCallOfCxxPseudoDestructorCall(stmt)) {
						// pseudo destructor calls are no-ops semantically
						continue;
					}
					core::transform::utils::migrateAnnotations(stmt, replacement);
					newStmts.push_back(replacement);
				}
				return builder.compoundStmt(newStmts);
			}, core::transform::globalReplacement);
		}

		//////////////////////////////////////////////////////////////////////////
		// Simplify calls which have no side effects because we cast from Type A to Type B to Type A again
		// =======================================================================
		core::ExpressionPtr simplifyRedundantNestedKindCasts(const core::ExpressionPtr& ir) {
			auto& mgr = ir->getNodeManager();
			const auto& refExt = mgr.getLangExtension<core::lang::ReferenceExtension>();
			return core::transform::transformBottomUpGen(ir, [&](const core::CallExprPtr& call) -> core::ExpressionPtr {
				if(refExt.isCallOfRefKindCast(call)) {
					const auto& middle = call->getArgument(0);
					if(refExt.isCallOfRefKindCast(middle)) {
						// if we have two nested ref_kind_casts here, we check whether the outer type is the same as the object being casted
						const auto& inner = middle.as<core::CallExprPtr>()->getArgument(0);
						if(core::lang::getReferenceKind(call) == core::lang::getReferenceKind(inner)) {
							return inner;
						}
					}
				}
				return call;
			}, core::transform::globalReplacement);
		}


		//////////////////////////////////////////////////////////////////////////
		// Simplify cxx-style increment/decrement if the resulting ref is immediately dereffed
		// =======================================================================
		core::ExpressionPtr simplifyCxxStyleIncDec(const core::ExpressionPtr& ir) {
			auto& mgr = ir->getNodeManager();
			core::IRBuilder builder(mgr);
			auto& rExt = mgr.getLangExtension<core::lang::ReferenceExtension>();
			auto& pExt = mgr.getLangExtension<core::lang::PointerExtension>();

			return core::transform::transformBottomUpGen(ir, [&](const core::CallExprPtr& call) {
				auto replacement = call;
				if(rExt.isCallOfRefDeref(call)) {
					auto arg = call->getArgument(0);
					if(pExt.isCallOfCxxStylePtrPreDec(arg)) replacement = builder.callExpr(pExt.getPtrPreDec(), core::analysis::getArgument(arg, 0));
					if(pExt.isCallOfCxxStylePtrPreInc(arg)) replacement = builder.callExpr(pExt.getPtrPreInc(), core::analysis::getArgument(arg, 0));
				}
				return replacement;
			}, core::transform::globalReplacement);
		}

		//////////////////////////////////////////////////////////////////////////
		// Replace FE ref temps with real ref temps
		// =======================================================================
		core::ExpressionPtr replaceFERefTemp(const core::ExpressionPtr& ir) {
			auto& mgr = ir->getNodeManager();
			auto& refExt = mgr.getLangExtension<core::lang::ReferenceExtension>();
			auto& feExt = mgr.getLangExtension<utils::FrontendInspireModule>();
			return core::transform::transformBottomUpGen(ir, [&](const core::LiteralPtr& lit) -> core::ExpressionPtr {
				if(feExt.isFERefTemp(lit)) return refExt.getRefTemp();
				return lit;
			}, core::transform::globalReplacement);
		}

		//////////////////////////////////////////////////////////////////////////
		// Replace all copies of std::initializer_list with a call to the copy constructor
		// =======================================================================
		core::ExpressionPtr replaceStdInitListCopies(const core::ExpressionPtr& ir) {
			auto mangledName = insieme::utils::mangle("std::initializer_list");
			auto& mgr = ir->getNodeManager();
			auto& refExt = mgr.getLangExtension<core::lang::ReferenceExtension>();
			core::IRBuilder builder(mgr);
			return core::transform::transformBottomUpGen(ir, [&](const core::DeclarationPtr& decl) {
				const auto& type = decl->getType();
				auto expr = decl->getInitialization();

				// add ref_casts to force copy construction of init_lists, since this is implicit in cpp
				if(core::lang::isPlainReference(type) && refExt.isCallOfRefDeref(expr)) {
					auto elementType = expr->getType();
					if(auto tagT = elementType.isa<core::GenericTypePtr>()) {
						if(boost::starts_with(tagT->getName()->getValue(), mangledName)) {
							auto targetType = core::lang::ReferenceType::create(elementType, true, false, core::lang::ReferenceType::Kind::CppReference);
							expr = core::analysis::getArgument(expr, 0);
							return builder.declaration(type, core::lang::buildRefCast(expr, targetType));
						}
					}
				}
				return decl;
			}, core::transform::globalReplacement);
		}

		//// ProgramPtr steps ----------------------------------------------------------------------------------------------------------------------------

		class TypeCanonicalizer : public core::transform::CachedNodeMapping {
			virtual const NodePtr resolveElement(const NodePtr& ptr) override {

				// canonicalize types bottom-up
				auto res = ptr->substitute(ptr->getNodeManager(), *this);
				if (auto tt = res.isa<core::TagTypePtr>()) {
					return core::analysis::getCanonicalType(tt);
				}

				return res;
			}
		};

		//////////////////////////////////////////////////////////////////////
		// Assure return statements for "main" functions typed as int
		// ==========================================================
		//
		// In C, it's allowed for the main function to by typed as () -> int, but not actually contain a "return x".
		// Since in the backend we move that function to a "normal" one, this will generate a warning in the BE compiler.
		// We fix this by generating a "return 0" in that case.
		//
		ProgramPtr mainReturnCorrection(ProgramPtr prog) {
			auto& mgr = prog->getNodeManager();
			auto& basic = mgr.getLangBasic();
			const auto& build = IRBuilder(mgr);

			// if entry point returns int ensure that it has a return statement
			auto eplist = prog->getEntryPoints();
			// we only need to care about C programs which do not have a return because of C semantics on "main"
			if(eplist.size() == 1) {
				auto eP = prog->getEntryPoints()[0];
				if(eP.isa<LambdaExprPtr>()) {
					auto lambdaExp = eP.as<LambdaExprPtr>();
					auto funType = lambdaExp->getFunctionType();
					if(basic.isInt(funType->getReturnType())) {
						auto compound = LambdaExprAddress(lambdaExp)->getBody();

						icp::TreePattern compoundPattern =
							irp::compoundStmt(icp::empty | icp::single(!irp::returnStmt(icp::any)) | (icp::anyList << !irp::returnStmt(icp::any)));
						if(compoundPattern.match(compound)) {
							// outermost compound does not contain return, add it
							auto newRoot = core::transform::append(mgr, compound, toVector<core::StatementPtr>(build.returnStmt(build.intLit(0))))
											   .as<core::ExpressionPtr>();
							prog = Program::remEntryPoint(mgr, prog, eP);
							prog = Program::addEntryPoint(mgr, prog, newRoot);
						}
					}
				}
			}
			return prog;
		}

		//////////////////////////////////////////////////////////////////////////
		// Find and replace zero inits of tag types (which can only occur for global inits)
		// =======================================================================
		ProgramPtr replaceZeroStructInits(ProgramPtr prog) {
			auto& mgr = prog->getNodeManager();
			core::IRBuilder builder(mgr);
			prog = irp::replaceAllAddr(irp::callExpr(mgr.getLangBasic().getZero(), icp::anyList), prog, [&](const NodeAddress& matchingAddress) -> NodePtr {
					   auto call = matchingAddress.getAddressedNode().as<CallExprPtr>();
					   auto t = core::analysis::getRepresentedType(call->getArgument(0));
					   if(t.isa<core::TagTypePtr>()) { return builder.getZero(t); }
					   // else keep call
					   return call;
				   }).as<ProgramPtr>();
			return prog;
		}
	}

	boost::optional<std::string> FrontendCleanupExtension::isPrerequisiteMissing(ConversionSetup& setup) const {
		// last or second-last
		auto it = setup.getExtensions().crbegin();
		if(it->get() != this) {
			std::advance(it, 1);
			if(it->get() != this) { return boost::optional<std::string>("FrontendCleanup needs to be the last or second-to-last Extension"); }
		}

		// prerequisites are met - no prerequisite is missing
		return boost::optional<std::string>();
	}

	core::tu::IRTranslationUnit FrontendCleanupExtension::IRVisit(core::tu::IRTranslationUnit& tu) {
		purgeUncalledTemplateInstantiationMemberFunctions(tu);

		auto ir = core::tu::toIR(tu.getNodeManager(), tu);

		ir = simplifyCxxStyleIncDec(ir);
		ir = simplifyExpressionsInCompoundStatements(ir);
		ir = simplifyRedundantNestedKindCasts(ir);
		ir = replaceFERefTemp(ir);
		ir = replaceStdInitListCopies(ir);

		assert_false(core::analysis::contains(ir, core::IRBuilder(ir->getNodeManager()).genericType(utils::getDummyAutoDeducedTypeName())))
				<< "Found un-deduced auto type!";

		return core::tu::fromIR(ir);
	}

	insieme::core::ProgramPtr FrontendCleanupExtension::IRVisit(insieme::core::ProgramPtr& prog) {

		prog = TypeCanonicalizer().map(prog);
		prog = mainReturnCorrection(prog);
		prog = replaceZeroStructInits(prog);

		return prog;
	}

} // extensions
} // frontend
} // insieme
