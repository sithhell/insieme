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

#include "insieme/backend/preprocessor.h"

#include "insieme/backend/ir_extensions.h"
#include "insieme/backend/function_manager.h"

#include "insieme/core/ir_node.h"
#include "insieme/core/ir_builder.h"
#include "insieme/core/ir_visitor.h"
#include "insieme/core/ir_address.h"
#include "insieme/core/ir_class_info.h"

#include "insieme/core/lang/basic.h"
#include "insieme/core/lang/static_vars.h"
#include "insieme/core/analysis/ir_utils.h"
#include "insieme/core/analysis/attributes.h"
#include "insieme/core/types/type_variable_deduction.h"
#include "insieme/core/transform/node_replacer.h"
#include "insieme/core/transform/manipulation.h"
#include "insieme/core/transform/manipulation_utils.h"
#include "insieme/core/transform/node_mapper_utils.h"
#include "insieme/transform/ir_cleanup.h"

#include "insieme/utils/logging.h"

namespace insieme {
namespace backend {


	PreProcessorPtr getBasicPreProcessorSequence(BasicPreprocessorFlags options) {
		vector<PreProcessorPtr> steps;
		if (!(options & SKIP_POINTWISE_EXPANSION)) {
			steps.push_back(makePreProcessor<InlinePointwise>());
		}
		steps.push_back(makePreProcessor<InitGlobals>());
		steps.push_back(makePreProcessor<InlineExprWithCleanups>());
		steps.push_back(makePreProcessor<MakeVectorArrayCastsExplicit>());
		// steps.push_back(makePreProcessor<RedundancyElimination>());		// optional - disabled for performance reasons
		steps.push_back(makePreProcessor<CorrectRecVariableUsage>());
		return makePreProcessor<PreProcessingSequence>(steps);
	}


	core::NodePtr PreProcessingSequence::process(const Converter& converter, const core::NodePtr& code) {
		auto& manager = converter.getNodeManager();

		// start by copying code to given target manager
		core::NodePtr res = manager.get(code);

		// apply sequence of pre-processing steps
		for_each(preprocessor, [&](const PreProcessorPtr& cur) {
			res = cur->process(converter, res);
		});

		// return final result
		return res;
	}


	// ------- concrete pre-processing step implementations ---------

	core::NodePtr NoPreProcessing::process(const Converter& converter, const core::NodePtr& code) {
		// just copy to target manager
		return converter.getNodeManager().get(code);
	}




	// --------------------------------------------------------------------------------------------------------------
	//      PreProcessor InlinePointwise => replaces invocations of pointwise operators with in-lined code
	// --------------------------------------------------------------------------------------------------------------

	class PointwiseReplacer : public core::transform::CachedNodeMapping {

		core::NodeManager& manager;
		const core::lang::BasicGenerator& basic;

	public:

		PointwiseReplacer(core::NodeManager& manager) : manager(manager), basic(manager.getLangBasic()) {};

		const core::NodePtr resolveElement(const core::NodePtr& ptr) {

			// check types => abort
			if (ptr->getNodeCategory() == core::NC_Type) {
				return ptr;
			}

			// look for call expressions
			if (ptr->getNodeType() == core::NT_CallExpr) {
				// extract the call
				core::CallExprPtr call = static_pointer_cast<const core::CallExpr>(ptr);

				// only care about calls to pointwise operations
				if (core::analysis::isCallOf(call->getFunctionExpr(), basic.getVectorPointwise())) {

					// get argument and result types!
					assert(call->getType()->getNodeType() == core::NT_VectorType && "Result should be a vector!");
					assert(call->getArgument(0)->getType()->getNodeType() == core::NT_VectorType && "Argument should be a vector!");

					core::VectorTypePtr argType = static_pointer_cast<const core::VectorType>(call->getArgument(0)->getType());
					core::VectorTypePtr resType = static_pointer_cast<const core::VectorType>(call->getType());

					// extract generic parameter types
					core::TypePtr in = argType->getElementType();
					core::TypePtr out = resType->getElementType();

					assert(resType->getSize()->getNodeType() == core::NT_ConcreteIntTypeParam && "Result should be of fixed size!");
					core::ConcreteIntTypeParamPtr size = static_pointer_cast<const core::ConcreteIntTypeParam>(resType->getSize());

					// extract operator
					core::ExpressionPtr op = static_pointer_cast<const core::CallExpr>(call->getFunctionExpr())->getArgument(0);

					// create new lambda, realizing the point-wise operation
					core::IRBuilder builder(manager);

					core::FunctionTypePtr funType = builder.functionType(toVector<core::TypePtr>(argType, argType), resType);

					core::VariablePtr v1 = builder.variable(argType);
					core::VariablePtr v2 = builder.variable(argType);
					core::VariablePtr res = builder.variable(resType);

					// create vector init expression
					vector<core::ExpressionPtr> fields;

					// unroll the pointwise operation
					core::TypePtr unitType = basic.getUnit();
					core::TypePtr longType = basic.getUInt8();
					core::ExpressionPtr vectorSubscript = basic.getVectorSubscript();
					for(std::size_t i=0; i<size->getValue(); i++) {
						core::LiteralPtr index = builder.literal(longType, boost::lexical_cast<std::string>(i));

						core::ExpressionPtr a = builder.callExpr(in, vectorSubscript, v1, index);
						core::ExpressionPtr b = builder.callExpr(in, vectorSubscript, v2, index);

						fields.push_back(builder.callExpr(out, op, a, b));
					}

					// return result
					core::StatementPtr body = builder.returnStmt(builder.vectorExpr(resType, fields));

					// construct substitute ...
					core::LambdaExprPtr substitute = builder.lambdaExpr(funType, toVector(v1,v2), body);
					return builder.callExpr(resType, substitute, call->getArguments());
				}
			}

			// decent recursively
			return ptr->substitute(manager, *this);
		}

	};

	core::NodePtr InlinePointwise::process(const Converter& converter, const core::NodePtr& code) {
		// the converter does the magic
		return PointwiseReplacer(converter.getNodeManager()).map(code);
	}


	// --------------------------------------------------------------------------------------------------------------
	//      Restore Globals
	// --------------------------------------------------------------------------------------------------------------

	namespace {

		core::CompoundStmtAddress getMainBody(const core::NodePtr& code) {
			static const core::CompoundStmtAddress fail;

			// check for the program - only works on the global level
			if (code->getNodeType() != core::NT_Program) {
				return fail;
			}

			// check whether it is a main program ...
			core::NodeAddress root(code);
			const core::ProgramAddress& program = core::static_address_cast<const core::Program>(root);
			if (!(program->getEntryPoints().size() == static_cast<std::size_t>(1))) {
				return fail;
			}

			// extract body of main
			const core::ExpressionAddress& mainExpr = program->getEntryPoints()[0];
			if (mainExpr->getNodeType() != core::NT_LambdaExpr) {
				return fail;
			}
			const core::LambdaExprAddress& main = core::static_address_cast<const core::LambdaExpr>(mainExpr);
			const core::StatementAddress& bodyStmt = main->getBody();
			if (bodyStmt->getNodeType() != core::NT_CompoundStmt) {
				return fail;
			}
			return core::static_address_cast<const core::CompoundStmt>(bodyStmt);
		}

	}



	// --------------------------------------------------------------------------------------------------------------
	//      Turn initial assignments of global variables into values to be assigned to init values.
	// --------------------------------------------------------------------------------------------------------------

	core::NodePtr InitGlobals::process(const Converter& converter, const core::NodePtr& code) {

		// get body of main
		auto body = getMainBody(code);
		if (!body) return code;			// nothing to do if this is not a full program

		auto& mgr = code->getNodeManager();
		const auto& base = mgr.getLangBasic();

		// search for globals initialized in main body
		std::map<core::LiteralPtr, core::CallExprAddress> inits;
		core::visitDepthFirstOncePrunable(body, [&](const core::StatementAddress& stmt)->bool {

			// check out
			if (auto call = stmt.isa<core::CallExprAddress>()) {

				// check whether it is an assignment
				if (core::analysis::isCallOf(call.as<core::CallExprPtr>(), base.getRefAssign())) {

					// check whether target is a literal
					if (auto trg = call[0].isa<core::LiteralPtr>()) {

						// check whether literal is already known
						auto pos = inits.find(trg);
						if (pos == inits.end()) {
							// found a new one
							inits[trg] = call;
						}
					}
				}
			}

			// decent into nested compound statements
			if (stmt.isa<core::CompoundStmtAddress>()) {
				return false;
			}

			// but nothing else
			return true;

		});

		// check if anything has been found
		if (inits.empty()) return code;

		// prepare a utility to check whether some expression is depending on globals
		auto isDependingOnGlobals = [&](const core::ExpressionPtr& expr) {
			return core::visitDepthFirstOnceInterruptible(expr, [&](const core::LiteralPtr& lit)->bool {
				// every global variable is a literal of a ref-type ... that's how we identify those
				return lit->getType().isa<core::RefTypePtr>();
			});
		};

		// build up replacement map
		const auto& ext = mgr.getLangExtension<IRExtensions>();
		core::IRBuilder builder(mgr);
		std::map<core::NodeAddress, core::NodePtr> replacements;
		for(const auto& cur : inits) {
			// no free variables shell be moved to the global space
			if (core::analysis::hasFreeVariables(cur.second[1])) {
				continue;
			}

			// skip initialization expressions if they are depending on globals
			if (isDependingOnGlobals(cur.second[1])) {
				continue;
			}

			// if it is initializing a vector
			if (cur.second[1].isa<core::VectorExprPtr>()) {
				continue;
			}
			replacements[cur.second] = builder.callExpr(ext.getInitGlobal(), cur.second[0], cur.second[1]);
		}

		// if there is nothing to do => done
		if (replacements.empty()) {
			return code;
		}

		// conduct replacements
		return core::transform::replaceAll(mgr, replacements);
	}


	// --------------------------------------------------------------------------------------------------------------
	//      Adding explicit Vector to Array casts
	// --------------------------------------------------------------------------------------------------------------

	class VectorToArrayConverter : public core::transform::CachedNodeMapping {

		core::NodeManager& manager;

	public:

		VectorToArrayConverter(core::NodeManager& manager) : manager(manager) {}

		const core::NodePtr resolveElement(const core::NodePtr& ptr) {

			// do not touch types ...
			if (ptr->getNodeCategory() == core::NC_Type) {
				return ptr;
			}

			// apply recursively - bottom up
			core::NodePtr res = ptr->substitute(ptr->getNodeManager(), *this);

			// handle calls
			if (ptr->getNodeType() == core::NT_CallExpr) {
				res = handleCallExpr(core::static_pointer_cast<const core::CallExpr>(res));
			}

			// handle rest
			return res;
		}

	private:

		/**
		 * This method replaces vector arguments with vector2array conversions whenever necessary.
		 */
		core::CallExprPtr handleCallExpr(core::CallExprPtr call) {

			// extract node manager
			core::IRBuilder builder(manager);
			const core::lang::BasicGenerator& basic = builder.getLangBasic();


			// check whether there is a argument which is a vector but the parameter is not
			const core::TypePtr& type = call->getFunctionExpr()->getType();
			assert(type->getNodeType() == core::NT_FunctionType && "Function should be of a function type!");
			const core::FunctionTypePtr& funType = core::static_pointer_cast<const core::FunctionType>(type);

			const core::TypeList& paramTypes = funType->getParameterTypes()->getElements();
			const core::ExpressionList& args = call->getArguments();

			// check number of arguments
			if (paramTypes.size() != args.size()) {
				// => invalid call, don't touch this
				return call;
			}

			bool conversionRequired = false;
			std::size_t size = args.size();
			for (std::size_t i = 0; !conversionRequired && i < size; i++) {
				conversionRequired = conversionRequired || (args[i]->getType()->getNodeType() == core::NT_VectorType && paramTypes[i]->getNodeType() != core::NT_VectorType);
			}

			// check whether a vector / non-vector argument/parameter pair has been found
			if (!conversionRequired) {
				// => no deduction required
				return call;
			}

			// derive type variable instantiation
			auto instantiation = core::types::getTypeVariableInstantiation(manager, call);
			if (!instantiation) {
				// => invalid call, don't touch this
				return call;
			}

			// obtain argument list
			vector<core::TypePtr> argTypes;
			::transform(args, std::back_inserter(argTypes), [](const core::ExpressionPtr& cur) { return cur->getType(); });

			// apply match on parameter list
			core::TypeList newParamTypes = paramTypes;
			for (std::size_t i=0; i<newParamTypes.size(); i++) {
				newParamTypes[i] = instantiation->applyTo(manager, newParamTypes[i]);
			}

			// generate new argument list
			bool changed = false;
			core::ExpressionList newArgs = call->getArguments();
			for (unsigned i=0; i<size; i++) {

				// ignore identical types
				if (*newParamTypes[i] == *argTypes[i]) {
					continue;
				}

				core::TypePtr argType = argTypes[i];
				core::TypePtr paramType = newParamTypes[i];

				// strip references
				bool ref = false;
				if (argType->getNodeType() == core::NT_RefType && paramType->getNodeType() == core::NT_RefType) {
					ref = true;
					argType = core::static_pointer_cast<const core::RefType>(argType)->getElementType();
					paramType = core::static_pointer_cast<const core::RefType>(paramType)->getElementType();
				}

				// handle vector->array
				if (argType->getNodeType() == core::NT_VectorType && paramType->getNodeType() == core::NT_ArrayType) {
					if (!ref) { assert(ref && "Cannot convert implicitly to array value!"); }
					// conversion needed
					newArgs[i] = builder.callExpr(basic.getRefVectorToRefArray(), newArgs[i]);
					changed = true;
				}
			}
			if (!changed) {
				// return original call
				return call;
			}

			// exchange parameters and done
			return core::CallExpr::get(manager, instantiation->applyTo(call->getType()), call->getFunctionExpr(), newArgs);
		}

	};

	core::NodePtr MakeVectorArrayCastsExplicit::process(const Converter& converter, const core::NodePtr& code) {
		// the converter does the magic
		return VectorToArrayConverter(converter.getNodeManager()).map(code);
	}

	core::NodePtr RedundancyElimination::process(const Converter& converter, const core::NodePtr& code) {
		// this pass has been implemented as part of the core manipulation utils
		return transform::eliminateRedundantAssignments(code);
	}

	core::NodePtr CorrectRecVariableUsage::process(const Converter& converter, const core::NodePtr& code) {
		core::NodeManager& manager = converter.getNodeManager();

		// this pass has been implemented as part of the core manipulation utils
		return core::transform::makeCachedLambdaMapper([&](const core::NodePtr& code)->core::NodePtr {
			// only consider lambdas
			if (code->getNodeType() != core::NT_LambdaExpr) return code;
			// use core library utility to fix recursive variable usage
			return core::transform::correctRecursiveLambdaVariableUsage(manager, code.as<core::LambdaExprPtr>());
		}).map(code);
	}


	namespace detail{
		bool isExpressionWithCleanups(const core::ExpressionPtr& expr){
			
			// check that is a lambda
			auto fun = expr.isa<core::LambdaExprPtr>();
			if (!fun) {
				return false;
			}

			// can not inline recursions
			if (fun->isRecursive()) {
				return false;
			}

			// can not inline members
			if (fun->getFunctionType()->isConstructor() || fun->getFunctionType()->isDestructor() || fun->getFunctionType()->isMemberFunction() ){
				return false;
			}

			// can not inline a void return function
			if (fun->getFunctionType()->getReturnType() ==  expr.getNodeManager().getLangBasic().getUnit()){
				return false;
			}

			// an inlineable function returns by value
			if (fun->getFunctionType()->getReturnType().isa<core::RefTypePtr>()){
				return false;
			}

			// is not an empty func (it must have 2 or more stmts)
			// 	- at least one cleanup declaration + the actual expression
			auto body = fun->getBody();
			if (body.size() < 2) {
				return false;
			}

			// last stmt can be a return or another expression (the actual cleanp), and previous to last could be an expression in cases with returned value
			auto lastStmt =body[body.size()-1];
			auto preLastStmt = body.size() > 2? body[body.size()-2]: core::StatementPtr();
			if (!lastStmt.isa<core::ExpressionPtr>() && !lastStmt.isa<core::ReturnStmtPtr>()){
				return false;
			}

			// this two are to check that we do not use static vars inside of the function to inline
			const auto& staticLazy  = expr.getNodeManager().getLangExtension<core::lang::StaticVariableExtension>().getInitStaticLazy();
			const auto& staticConst = expr.getNodeManager().getLangExtension<core::lang::StaticVariableExtension>().getInitStaticConst();

			// only declarations except for the (2) last 
			for (auto stmt : body){
				if (stmt == lastStmt) break;
				if (stmt == preLastStmt && core::analysis::isCallOf(stmt, stmt->getNodeManager().getLangBasic().getRefAssign()) ){
					if(auto retStmt =  lastStmt.isa<core::ReturnStmtPtr>()){
						
						if (core::analysis::isCallOf(retStmt->getReturnExpr(), retStmt->getNodeManager().getLangBasic().getRefDeref())){
							return retStmt->getReturnExpr().as<core::CallExprPtr>()[0].isa<core::VariablePtr>();
						}
						else{
							return false;
						}

					}
				}

				core::DeclarationStmtPtr decl = stmt.isa<core::DeclarationStmtPtr>();
				if(!decl){
					return false;
				}
				else{
					auto varType = decl->getVariable()->getType();

					// make sure no static variables are used here: otherwhise we can not inline
					if(core::analysis::isCallOf(decl->getInitialization(), staticLazy) || core::analysis::isCallOf(decl->getInitialization(), staticConst)){
						return false;
					}
					// no builtin is a cleanup
					else if (expr.getNodeManager().getLangBasic().isBuiltIn(varType)){
						return false;
					}
					// declare variable can not be a cppref or a pointer, it has to be an actual value with the need to be cleaned up
					else if (varType.isa<core::RefTypePtr>() && expr.getNodeManager().getLangBasic().isBuiltIn(varType.as<core::RefTypePtr>()->getElementType())){
						return false;
					}
					// no pointer or array can be a cleanup
					else if (varType.isa<core::RefTypePtr>() && (varType.as<core::RefTypePtr>()->getElementType().isa<core::RefTypePtr>() || 
																 varType.as<core::RefTypePtr>()->getElementType().isa<core::ArrayTypePtr>() ||  
																 varType.as<core::RefTypePtr>()->getElementType().isa<core::VectorTypePtr>() )){
						return false;
					}
				}
			}
			
			// any other case, is suitable for inlining
			return true;
		}

		core::ExpressionPtr inlineExpressionWithCleanups(const core::ExpressionPtr& expr){
			assert_true(isExpressionWithCleanups(expr)) << "not supposed to use with this: \n" << dumpPretty(expr);

			// check that is a lambda
			auto fun = expr.as<core::LambdaExprPtr>();
			auto body = fun->getBody();


			core::ExpressionPtr res;
			auto lastStmt = body[body.size()-1];
			auto preLastStmt = body.size() > 2? body[body.size()-2]: core::StatementPtr();
			if (preLastStmt && core::analysis::isCallOf(preLastStmt, preLastStmt->getNodeManager().getLangBasic().getRefAssign())){
				res = preLastStmt.as<core::ExpressionPtr>();
			}
			else if (auto returnStmt = lastStmt.isa<core::ReturnStmtPtr>()){
				res = returnStmt->getReturnExpr();
			}
			else{
				res = lastStmt.as<core::ExpressionPtr>();
			}

			core::StatementList list = body->getStatements();

			auto it = list.rbegin()+1;
			auto end = list.rend();
			for (; it != end; ++it){
				// skip the assignment in case of assignment cleanups
				if (*it == res) continue;
				auto decl = (*it).as<core::DeclarationStmtPtr>();
				res =core::transform::replaceAllGen(res->getNodeManager(), res, decl->getVariable(), decl->getInitialization());
			}
			
			return res;
		}
	}// detail namespace

	core::NodePtr InlineExprWithCleanups::process(const Converter& converter, const core::NodePtr& code) {

		auto inliner = core::transform::makeCachedLambdaMapper([&](const core::NodePtr& node)->core::NodePtr {
			if(auto call = node.isa<core::CallExprPtr>()){
				core::ExpressionPtr fun = core::analysis::stripAttributes(call->getFunctionExpr());
				if (detail::isExpressionWithCleanups(fun)){

					auto res =  detail::inlineExpressionWithCleanups(fun);
					core::IRBuilder builder(fun->getNodeManager());

					// inline the new expression 
					core::LambdaExprAddress root(fun.as<core::LambdaExprPtr>());
					auto bodyAddress = root->getBody();
					auto newFun = core::transform::replaceNode (res->getNodeManager(), bodyAddress, builder.compoundStmt(builder.returnStmt(res)));
					res = builder.callExpr(call->getType(), newFun.as<core::ExpressionPtr>(), call->getArguments());
					res = core::transform::tryInlineToExpr (res->getNodeManager(), res.as<core::CallExprPtr>());
					return res;
				}	
			}
			return node;
		});

		auto res = inliner.map(code);

		// this thing modifies the objects, it might be that then we create a new function, 
		// but meta infos will still have a pointer to the original function, ending up into two implementations 
		core::visitDepthFirstOnce(res, [&] (const core::TypePtr& type){
			if (core::hasMetaInfo(type)){
				auto meta = core::getMetaInfo(type);

				vector<core::ExpressionPtr> ctors = meta.getConstructors();
				for (auto& ctor : ctors){
					ctor = inliner.map(ctor).as<core::ExpressionPtr>();
				}
				if (!ctors.empty()) meta.setConstructors(ctors);

				if (meta.hasDestructor()){
					auto dtor = meta.getDestructor();
					dtor = inliner.map(dtor).as<core::ExpressionPtr>();
					meta.setDestructor(dtor);
				}

				vector<core::MemberFunction> members = meta.getMemberFunctions();
				for (core::MemberFunction& member : members){
					
					auto m = inliner.map(member.getImplementation());
					member = core::MemberFunction(member.getName(), m.as<core::ExpressionPtr>(),
												  member.isVirtual(), member.isConst());
				}
				if(!members.empty()) meta.setMemberFunctions(members);
				core::setMetaInfo(type, meta);

			}
		});
		return res;
	};

} // end namespace backend
} // end namespace insieme
