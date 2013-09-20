/**
 * Copyright (c) 2002-2013 Distributed and Parallel Systems Group,
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

#include "insieme/analysis/cba/framework/call_site_manager.h"

#include <boost/optional.hpp>

#include "insieme/analysis/cba/utils/cba_utils.h"
#include "insieme/core/ir_visitor.h"

#include "insieme/utils/functional_chain.h"

namespace insieme {
namespace analysis {
namespace cba {

	namespace {

		typedef vector<Caller> CallerList;
		typedef boost::optional<CallerList> OptCallerList;

		OptCallerList getStaticUses(const Callee& callee);
		NodeAddress tryObtainingFunction(const core::ExpressionAddress& expr);

	}


	CallSiteManager::CallSiteManager(const core::StatementAddress& root)
		: freeCallees(), freeCallers(), dynamicCalls() {

		// collect free callers and callees
		visitDepthFirst(root,
			insieme::utils::chain(
				[&](const ExpressionAddress& cur) {

					// only interested in lambdas, bind and literals
					auto kind = cur->getNodeType();
					if (kind != NT_LambdaExpr && kind != NT_BindExpr && kind != NT_Literal) return;

					// for literals: check whether it is a function
					FunctionTypePtr type = cur.as<ExpressionPtr>()->getType().isa<FunctionTypePtr>();
					if (!type) return;	// might be the case for literals

					// check whether it is free (not used in a direct call or as a full expression)
					if(cur.isRoot()) return;
					auto parent = cur.getParentNode();
					auto parentKind = cur.getParentNode()->getNodeType();
					if (parentKind == NT_CompoundStmt) return;

					if (parentKind == NT_CallExpr) {
						if (parent.as<CallExprPtr>()->getFunctionExpr() == cur.as<ExpressionPtr>()) return;
					}

					// if all uses of the function can be determined statically => done
					// TODO: result could be immediately saved within map (since it is required later on)
					if (getStaticUses(cur.as<NodeAddress>())) return;

					// found a free function => register it
					Callee callee = (kind == NT_LambdaExpr) ? Callee(cur.as<LambdaExprAddress>()->getLambda()) :
									(kind == NT_BindExpr)   ? Callee(cur.as<BindExprAddress>()) : Callee(cur.as<LiteralAddress>());
					freeCallees[type->getParameterTypes()->size()].push_back(callee);
				},
				[&](const ExpressionAddress& cur) {

					auto call = cur.isa<CallExprAddress>();
					if (!call) return;

					// check whether target is fixed
					auto fun = call->getFunctionExpr();
					auto kind = fun->getNodeType();

					// check whether it is a direct call
					if (kind == NT_LambdaExpr || kind == NT_BindExpr || kind == NT_Literal) return;

					// at this point it is a dynamic call - but not yet a free call
					dynamicCalls.insert(call);

					// TODO: check whether this should be re-enabled
					// check whether the target is statically known
//					if (kind == NT_Variable) {
//						if (tryObtainingFunction(fun)) return;
//					}

					// this is one
					freeCallers[call->size()].push_back(call);
				}
			)
		);
	}


	const vector<Callee>& CallSiteManager::getCallee(const Caller& caller) {

		// check cache
		auto pos = forward.find(caller);
		if (pos != forward.end()) {
			return pos->second;
		}

		const vector<Callee>& res = forward[caller] = computeCallee(caller);

		// cross-check result - the assertion has to be split up due to a gcc limitation
//		assert_decl(bool bedirectional = all(computeCallee(caller), [&](const Callee& cur)->bool {
//			return contains(this->getCaller(cur), caller);
//		}));
//		assert_true(bedirectional);

		return res;

//		// compute callee, cache result and return it
//		return forward[caller] = computeCallee(caller);
	}

	const vector<Caller>& CallSiteManager::getCaller(const Callee& callee) {

		// check cache
		auto pos = backward.find(callee);
		if (pos != backward.end()) {
			return pos->second;
		}

		const vector<Caller>& res = backward[callee] = computeCaller(callee);

		// cross-check result - the assertion has to be split up due to a gcc limitation
//		assert_decl(bool bedirectional = all(computeCaller(callee), [&](const Caller& cur)->bool {
//			return contains(this->getCallee(cur), callee);
//		}));
//		assert_true(bedirectional);

		return res;

//		// compute caller, cache result and return it
//		return backward[callee] = computeCaller(callee);
	}


	// -------------------------------------------------------------------------------------------
	//   										Callee => Caller
	// -------------------------------------------------------------------------------------------

	namespace {

		typedef vector<Caller> CallerList;
		typedef boost::optional<CallerList> OptCallerList;

		bool collectUsesOfVariable(const VariableAddress& var, vector<Caller>& res) {
			assert(getDefinitionPoint(var) == var);

			NodeAddress root;
			if (auto decl = var.getParentAddress().isa<DeclarationStmtAddress>()) {
				root = decl.getParentAddress();
			} else if (auto params = var.getParentAddress().isa<ParametersAddress>()) {
				if (auto lambda = params.getParentAddress().isa<LambdaAddress>()) {
					root = lambda->getBody();
				} else if (auto bind = params.getParentAddress().isa<BindExprAddress>()) {
					root = bind->getCall();
				} else {
					assert_fail() << "Unknown parent type for Parameters: " << params.getParentAddress()->getNodeType();
				}
			} else {
				assert_fail() << "Unknown parent of variable definition: " << var.getParentAddress()->getNodeType();
			}

			// there should be a root now
			assert(root);

			bool allFine = true;
			visitDepthFirstPrunable(root, [&](const ExpressionAddress& cur) {
				// stop if already failed
				if (!allFine) return true;

				// only process local scope
				if (cur->getNodeType() == NT_LambdaExpr) return true;

				// for the rest, only interested in variables
				if (*cur != *var) return false;

				// if variable is used as a function => found a call
				auto call = cur.getParentAddress().isa<CallExprAddress>();

				// if it is not a call, we don't care
				if (!call) return false;

				// check out whether it is a call to the function or passed as an argument
				if (call->getFunctionExpr() == cur) {
					// it is the target function => collect this one
					res.push_back(call);
				} else {
					// it is an argument
					if (auto fun = call->getFunctionExpr().isa<LambdaExprAddress>()) {
						assert(call[cur.getIndex()-2] == cur);
						// ok - it is a static call => we may follow the parameter
						allFine = allFine && collectUsesOfVariable(fun->getParameterList()[cur.getIndex()-2], res);
					} else {
						// it is passed by argument to a non-static call => could go anywhere
						allFine = false;
						return true;
					}
				}

				return false;

			});

			return allFine;

		}

		OptCallerList getUsesOfVariable(const VariableAddress& def) {
			static const OptCallerList fail;
			vector<Caller> res;
			bool success = collectUsesOfVariable(def, res);
			return success ? res : fail;
		}

		OptCallerList getStaticUses(const Callee& callee) {
			static const OptCallerList unknown;

			// get function definition
			auto function = callee.getDefiningExpr();

			// there is nothing we can do for the root
			if (function.isRoot()) return unknown;

			OptCallerList res = CallerList();

			// check whether defining expression is actually addressing represented callee
			if (!callee.isLambda() || function.as<LambdaExprAddress>()->getLambda() == callee.getDefinition()) {

				// option A: the lambda is created as an argument of a call expression
				auto parent = function.getParentAddress();
				if (auto call = parent.isa<CallExprAddress>()) {

					// check for a direct call
					if (call->getFunctionExpr() == function) {
						res = toVector(Caller(call));

					// function is an argument of the call => check whether target function is fixed
					} else if (auto fun = call->getFunctionExpr().isa<LambdaExprAddress>()) {
						// collect all uses of corresponding function parameter
						assert(call[function.getIndex()-2] == function);
						res = getUsesOfVariable(fun->getParameterList()[function.getIndex()-2]);

					} else {
						return unknown;
					}

				// option B: the lambda is created as the init value of a declaration
				} else if (auto decl = parent.isa<DeclarationStmtAddress>()) {
					// simply collect all uses of the variable
					res = getUsesOfVariable(decl->getVariable());
				}

			}

			// rest is only required if successful so far and callee is recursive
			if (!res || !callee.isRecursive()) return res;

			// add recursive calls
			auto def = function.as<LambdaExprAddress>()->getDefinition();
			auto var = callee.getDefinition().getParentAddress().as<LambdaBindingPtr>()->getVariable();
			for(auto cur : def->getRecursiveCallsOf(var)) {

				// compute absolute position of the variable
				auto var = concat(def, cur);

				// get call from recursive variable reference
				CallExprAddress call = var.getParentAddress().as<CallExprAddress>();
				assert_true(call->getFunctionExpr() == var) << "Recursive variables should be called directly!";

				// add call to result list
				res->push_back(call);
			}

			// done
			return res;
		}

	}


	vector<Caller> CallSiteManager::computeCaller(const Callee& callee) const {
		static const vector<Caller> empty;

		// general case: collect all uses
		auto uses = getStaticUses(callee);

		// check whether uses could be determined statically
		if (uses) {
			// take those
			return *uses;
		}

		// no static limit on uses => might be used everywhere
		return getFreeCallers(callee.getNumParams());
	}

	// -------------------------------------------------------------------------------------------
	//   										Caller => Callee
	// -------------------------------------------------------------------------------------------

	namespace {

		bool isFunction(const core::ExpressionAddress& expr) {
			auto nodeType = expr->getNodeType();
			return nodeType == NT_LambdaExpr || nodeType == NT_BindExpr || nodeType == NT_Literal;
		}

		NodeAddress tryObtainingFunction(const core::ExpressionAddress& expr) {
			static const ExpressionAddress unknown;

			// check for null
			if (!expr) return unknown;

			// check whether we have a lambda expression
			if (auto lambdaExpr = expr.isa<LambdaExprAddress>()) return lambdaExpr->getLambda();

			// check whether we already have one
			if (isFunction(expr)) return expr;

			// otherwise we are only supporting variables
			auto var = expr.isa<VariableAddress>();
			if (!var) return unknown;

			// get definition of variable
			auto def = getDefinitionPoint(var);

			// if it is a free variable => there is nothing we can do
			if (def.isRoot()) return unknown;
			auto parent = def.getParentAddress();

			// if definition is a lambda binding => it is a recursive call
			if (auto binding = parent.isa<LambdaBindingAddress>()) {
				return binding->getLambda();
			}

			// if variable is declared => consider declaration
			if (auto decl = parent.isa<DeclarationStmtAddress>()) {
				return tryObtainingFunction(decl->getInitialization());
			}

			// if it is an parameter => follow argument of direct call (if possible)
			if (auto param = parent.isa<ParametersAddress>()) {

				if (param.isRoot()) return unknown;

				auto userOffset = param.getParentNode().isa<LambdaPtr>() ? 5 : 2;
				auto user = param.getParentAddress(userOffset);

				// continue with proper argument
				auto call = user.isa<CallExprAddress>();
				if (call) return tryObtainingFunction(call[def.getIndex()]);
			}

			// otherwise there is nothing we can do
			return unknown;
		}

	}

	vector<Callee> CallSiteManager::computeCallee(const Caller& caller) const {
		static const vector<Callee> empty;

		// investigate function expression
		ExpressionAddress fun = caller.getCall()->getFunctionExpr();

		// -- handle direct calls --
		if (auto literal = fun.isa<LiteralAddress>()) {
			// simply a direct call to a literal
			return toVector(Callee(literal));
		}
		if (auto lambda = fun.isa<LambdaExprAddress>()) {
			// this is a direct call to a lambda
			return toVector(Callee(lambda->getLambda()));
		}
		if (auto bind = fun.isa<BindExprAddress>()) {
			// this is a direct call to a local bind
			return toVector(Callee(bind));
		}

		// -- handle indirect calls - calling a variable --
		if (auto var = fun.isa<VariableAddress>()) {

			// try to obtain function represented by variable statically
			NodeAddress trg = tryObtainingFunction(var);

			if (trg) {
				// good => that's the function to be called
				if (auto lambda = trg.isa<LambdaAddress>()) {
					return toVector(Callee(lambda));
				}
				if (auto bind = trg.isa<BindExprAddress>()) {
					return toVector(Callee(bind));
				}
				return toVector(Callee(trg.as<LiteralAddress>()));
			}

			// bad => it may call any function
			// => fall through
		}

		// fall-back: get all accessible callees with the correct number of arguments
		return getFreeCallees(caller.getNumArgs());
	}


	const vector<Callee>& CallSiteManager::getFreeCallees(unsigned numParams) const {
		static const vector<Callee> empty;

		auto pos = freeCallees.find(numParams);
		return (pos == freeCallees.end()) ? empty : pos->second;
	}

	const vector<Caller>& CallSiteManager::getFreeCallers(unsigned numArgs) const {
		static const vector<Caller> empty;

		auto pos = freeCallers.find(numArgs);
		return (pos == freeCallers.end()) ? empty : pos->second;
	}

	bool CallSiteManager::isFree(const Callee& callee) const {
		return contains(getFreeCallees(callee.getNumParams()), callee);
	}


//	namespace {
//
//		bool collectUsesOfVariable(CBA& context, const VariableAddress& var, vector<Label>& res) {
//			assert(getDefinitionPoint(var) == var);
//
//			NodeAddress root;
//			if (auto decl = var.getParentAddress().isa<DeclarationStmtAddress>()) {
//				root = decl.getParentAddress();
//			} else if (auto params = var.getParentAddress().isa<ParametersAddress>()) {
//				if (auto lambda = params.getParentAddress().isa<LambdaAddress>()) {
//					root = lambda->getBody();
//				} else if (auto bind = params.getParentAddress().isa<BindExprAddress>()) {
//					root = bind->getCall();
//				} else {
//					assert_fail() << "Unknown parent type for Parameters: " << params.getParentAddress()->getNodeType();
//				}
//			} else {
//				assert_fail() << "Unknown parent of variable definition: " << var.getParentAddress()->getNodeType();
//			}
//
//			// there should be a root now
//			assert(root);
//
//			bool allFine = true;
//			visitDepthFirstPrunable(root, [&](const ExpressionAddress& cur) {
//				// stop if already failed
//				if (!allFine) return true;
//
//				// only process local scope
//				if (cur->getNodeType() == NT_LambdaExpr) return true;
//
//				// for the rest, only interested in variables
//				if (*cur != *var) return false;
//
//				// if variable is used as a function => found a call
//				auto call = cur.getParentAddress().isa<CallExprAddress>();
//
//				// if it is not a call, we don't care
//				if (!call) return false;
//
//				// check out whether it is a call to the function or passed as an argument
//				if (call->getFunctionExpr() == cur) {
//					// it is the target function => collect this one
//					res.push_back(context.getLabel(call));
//				} else {
//					// it is an argument
//					if (auto fun = call->getFunctionExpr().isa<LambdaExprAddress>()) {
//						assert(call[cur.getIndex()-2] == cur);
//						// ok - it is a static call => we may follow the parameter
//						allFine = allFine && collectUsesOfVariable(context, fun->getParameterList()[cur.getIndex()-2], res);
//					}
//				}
//
//				return false;
//
//			});
//
//			return allFine;
//
//		}
//
//		CBA::OptCallSiteList getUsesOfVariable(CBA& context, const VariableAddress& def) {
//			static const CBA::OptCallSiteList fail;
//			vector<Label> res;
//			bool success = collectUsesOfVariable(context, def, res);
//			return success ? res : fail;
//		}
//
//		CBA::OptCallSiteList getStaticUses(CBA& context, const ExpressionAddress& function) {
//			static const CBA::OptCallSiteList unknown;
//
//			// there is nothing we can do for the root
//			if (function.isRoot()) return unknown;
//
//			// option A: the lambda is created as an argument of a call expression
//			auto parent = function.getParentAddress();
//			if (auto call = parent.isa<CallExprAddress>()) {
//				assert(call->getFunctionExpr() != function);
//
//				// check whether target function is fixed
//				if (auto fun = call->getFunctionExpr().isa<LambdaExprAddress>()) {
//					// collect all uses of corresponding function parameter
//					assert(call[function.getIndex()-2] == function);
//					return getUsesOfVariable(context, fun->getParameterList()[function.getIndex()-2]);
//				} else {
//					return unknown;
//				}
//			}
//
//			// option B: the lambda is created as the value of a definition
//			if (auto decl = parent.isa<DeclarationStmtAddress>()) {
//				// simply collect all uses of the variable
//				return getUsesOfVariable(context, decl->getVariable());
//			}
//
//			return unknown;
//		}
//
//		bool isFunction(const core::ExpressionAddress& expr) {
//			return expr->getNodeType() == NT_LambdaExpr || expr->getNodeType() == NT_BindExpr;
//		}
//
//		ExpressionAddress tryObtainingFunction(const core::ExpressionAddress& expr) {
//			static const ExpressionAddress unknown;
//
//			// check for null
//			if (!expr) return unknown;
//
//			// check whether we already have one
//			if (isFunction(expr)) return expr;
//
//			// otherwise we are only supporting variables
//			auto var = expr.isa<VariableAddress>();
//			if (!var) return unknown;
//
//			// get definition of variable
//			auto def = getDefinitionPoint(var);
//
//			// if it is a free variable => there is nothing we can do
//			if (def.isRoot()) return unknown;
//
//			// if variable is declared => consider declaration
//			auto parent = def.getParentAddress();
//			if (auto decl = parent.isa<DeclarationStmtAddress>()) {
//				return tryObtainingFunction(decl->getInitialization());
//			}
//
//			// if it is an lambda parameter => follow argument
//			if (auto param = parent.isa<ParametersAddress>()) {
//
//				if (param.isRoot()) return unknown;
//
//				auto userOffset = param.getParentNode().isa<LambdaPtr>() ? 5 : 2;
//				auto user = param.getParentAddress(userOffset);
//
//				// continue with proper argument
//				auto call = user.isa<CallExprAddress>();
//				if (call) return tryObtainingFunction(call[param.getIndex()]);
//			}
//
//			// otherwise there is nothing we can do
//			return unknown;
//		}
//
//	}
//
//
//	const CBA::OptCallSiteList& CBA::getAllStaticUses(const core::ExpressionAddress& fun) {
//
//		// check the cache
//		auto pos = callSiteCache.find(fun);
//		if (pos != callSiteCache.end()) {
//			return pos->second;
//		}
//
//		// compute call-site list
//		return callSiteCache[fun] = getStaticUses(*this, fun);
//	}

} // end namespace cba
} // end namespace analysis
} // end namespace insieme