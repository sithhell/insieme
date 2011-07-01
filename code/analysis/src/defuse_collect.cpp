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

#include "insieme/analysis/defuse_collect.h"
#include <insieme/core/ast_visitor.h>

#include "insieme/core/analysis/ir_utils.h"

#include <stack>

namespace {

using namespace insieme::analysis;

std::string refTypeToStr(const Ref::RefType& type) { 
	switch(type) {
	case Ref::VAR:		return "SCALAR"; 
	case Ref::ARRAY:	return "ARRAY"; 
	case Ref::MEMBER:	return "MEMBER"; 
	case Ref::CALL: 	return "CALL"; 
	default:  			assert(false);
	}
}

std::string usageTypeToStr(const Ref::UseType& usage) {
	switch (usage) {
	case Ref::DEF: 		return "DEF"; 
	case Ref::USE:		return "USE"; 
	case Ref::UNKNOWN:	return "UNKNOWN"; 
	default:			assert(false);
	}
}

} // end anonymous namespace 
namespace insieme {
namespace analysis {

//===== Ref =========================================================================================

Ref::Ref(const RefType& type, const core::ExpressionPtr& var, const UseType& usage) : 
	 baseExpr(var), type(type), usage(usage) 
{ 
	assert(var->getType()->getNodeType() == core::NT_RefType && "TYpe of base expression must be of RefType"); 
}

std::ostream& Ref::printTo(std::ostream& out) const {
	return out << "[" << usageTypeToStr(getUsage()) << "] - " << refTypeToStr(getType());
}

//===== ScalarRef =======================================================================================

ScalarRef::ScalarRef(const core::VariablePtr& var, const Ref::UseType& usage) : Ref(Ref::VAR, var, usage) { }

const core::VariablePtr& ScalarRef::getVariable() const { 
	return core::static_pointer_cast<const core::Variable>(baseExpr);
}

std::ostream& ScalarRef::printTo(std::ostream& out) const {
	Ref::printTo(out);
	return out << "(" << *baseExpr << ")";
}

//===== ArrayRef =======================================================================================

std::ostream& ArrayRef::printTo(std::ostream& out) const {
	Ref::printTo(out);
	out << "(" << *baseExpr << ")";
	out << " IDX: {" << 
		join("; ", idxExpr, [&](std::ostream& jout, const core::ExpressionPtr& cur){ jout << *cur; } ) << "}";
	if (!idxExpr.empty())
		out << "\n\tSurrounding expr: " << *exprPtr;
	return out;
}

//===== MemberRef =====================================================================================

MemberRef::MemberRef(const core::ExpressionPtr& memberAcc, const UseType& usage) : Ref(Ref::MEMBER, memberAcc, usage) { 
	assert (memberAcc->getNodeType() == core::NT_CallExpr);

	core::NodeManager& mgr = memberAcc->getNodeManager();
	assert (core::analysis::isCallOf(memberAcc, mgr.basic.getCompositeMemberAccess()) || 
		core::analysis::isCallOf(memberAcc, mgr.basic.getCompositeRefElem() ) );

	// initialize the value of the literal
	const core::CallExprPtr& callExpr = core::static_pointer_cast<const core::CallExpr>(memberAcc);
	identifier = core::static_pointer_cast<const core::Literal>(callExpr->getArgument(1));

	// initialize the value of the named composite type 
	const core::TypePtr& refType = callExpr->getArgument(0)->getType();
	assert(refType->getNodeType() == core::NT_RefType);

	type = core::static_pointer_cast<const core::NamedCompositeType>(
			core::static_pointer_cast<const core::RefType>(refType)->getElementType() );
}

std::ostream& MemberRef::printTo(std::ostream& out) const {
	Ref::printTo(out);
	return out << "(" << *type << "." << *identifier << ")";
}

//===== CallRef =====================================================================================
CallRef::CallRef(const core::CallExprPtr& callExpr, const UseType& usage) : Ref::Ref(Ref::CALL, callExpr, usage) { }

const core::CallExprPtr& CallRef::getCallExpr() const { return core::static_pointer_cast<const core::CallExpr>(baseExpr); }

std::ostream& CallRef::printTo(std::ostream& out) const {
	Ref::printTo(out);
	return out << " (" << *core::static_pointer_cast<const core::CallExpr>(baseExpr)->getFunctionExpr() << "(...)" << ")";
}

namespace {

/***
 * DefUseCollect finds occurences of variables (or, more in general, expressions) which can appear
 * in expressions. The main goal of this class is to determine whether a variable is used or
 * defined. In case of arrays references also a pointer to the expressions utilized to index each
 * array dimension needs to be stored. 
 */
class DefUseCollect : public core::ASTVisitor<> {
	
	RefList& refSet;
	Ref::UseType usage;

	typedef std::vector<core::ExpressionPtr> ExpressionList;
	typedef std::pair<core::ExpressionPtr, ExpressionList> SubscriptContext;
	std::stack<SubscriptContext> idxStack;

	const core::StatementSet& skipStmts;

	void addVariable(const core::ExpressionPtr& var, const Ref::RefType& refType=Ref::VAR) {
		const core::TypePtr& type = var->getType(); 
		
		// If the variable is not a ref we are not interested in its usage 
		if (type->getNodeType() != core::NT_RefType) { return; }
	
		const core::TypePtr& subType = core::static_pointer_cast<const core::RefType>(type)->getElementType();
		if (subType->getNodeType() == core::NT_ArrayType || subType->getNodeType() == core::NT_VectorType) { 
			// In the case the sub type is a vector type, it means this is an array reference 
			SubscriptContext& subCtx = idxStack.top();
			refSet.push_back( 
				std::make_shared<ArrayRef>(var, 
					// copy the index expressions in reverse order
					ExpressionList(subCtx.second.rbegin(), subCtx.second.rend()),
					subCtx.first,
					usage) 
				);
			idxStack.top() = SubscriptContext();	// reset the expresion list 
			return ;
		} 

		if (refType == Ref::MEMBER ) {
			refSet.push_back( std::make_shared<MemberRef>(var, usage) );
			return;
		}
		if ( refType == Ref::CALL ) {
			assert(var->getNodeType() == core::NT_CallExpr && "Expected call expression");
			refSet.push_back( std::make_shared<CallRef>(core::static_pointer_cast<const core::CallExpr>(var), usage) );
			return;
		}

		assert(var->getNodeType() == core::NT_Variable && "Expected scalar variable");
		refSet.push_back( std::make_shared<ScalarRef>(core::static_pointer_cast<const core::Variable>(var), usage) );
	}

public:
	DefUseCollect(RefList& refSet, const core::StatementSet& skipStmts) : 
		core::ASTVisitor<>(false), refSet(refSet), usage(Ref::USE), skipStmts(skipStmts) 
	{ 
			idxStack.push( SubscriptContext() ); // initialize the stack of array index expressions
	}

	void visitDeclarationStmt(const core::DeclarationStmtPtr& declStmt) {
		const core::VariablePtr& var = declStmt->getVariable();
		usage = Ref::DEF;
		addVariable(var);
		usage = Ref::USE;
		visit( declStmt->getInitialization() );
	}

	void visitVariable(const core::VariablePtr& var) { addVariable(var); }

	void visitCallExpr(const core::CallExprPtr& callExpr) {
		const core::NodeManager& mgr = callExpr->getNodeManager();

		// save the usage before the entering of this callexpression
		Ref::UseType saveUsage = usage;

		if (core::analysis::isCallOf(callExpr, mgr.basic.getRefAssign())) {
			assert( usage != Ref::DEF && "Nested assignment operations" );
			usage = Ref::DEF;
			visit( callExpr->getArgument(0) );
			usage = Ref::USE;
			visit( callExpr->getArgument(1) );
			usage = saveUsage; // restore the previous usage
			return;
		}

		if (core::analysis::isCallOf(callExpr, mgr.basic.getCompositeMemberAccess()) || 
			core::analysis::isCallOf(callExpr, mgr.basic.getCompositeRefElem() ) ) {

			addVariable(callExpr, Ref::MEMBER);

			// recur in the case the accessed member is an array (or struct)
			visit(callExpr->getArgument(0));
			return;
		}

		if (core::analysis::isCallOf(callExpr, mgr.basic.getArraySubscript1D()) ||
			core::analysis::isCallOf(callExpr, mgr.basic.getArrayRefElem1D()) || 
			core::analysis::isCallOf(callExpr, mgr.basic.getVectorRefElem()) || 
			core::analysis::isCallOf(callExpr, mgr.basic.getVectorSubscript()) ) 
		{
			usage = Ref::USE;
			assert(callExpr->getArguments().size() == 2 && "Malformed expression");
			
			// Visit the index expression
			idxStack.push( SubscriptContext() );
			visit( callExpr->getArgument(1) );
			idxStack.pop();
	
			usage = saveUsage; // restore the previous usage 
			assert(idxStack.size() > 0);
			SubscriptContext& subCtx = idxStack.top();
			// if the start of the subscript expression is not set, this is the start
			if (!subCtx.first) { subCtx.first = callExpr; }
			subCtx.second.push_back( callExpr->getArgument(1) );

			visit( callExpr->getArgument(0) );
			return;
		}

		// List the IR literals which do not alterate the usage of a variable  
		if (core::analysis::isCallOf(callExpr, mgr.basic.getRefDeref())) {
			usage = Ref::USE;
			visit( callExpr->getArgument(0) );
			usage = saveUsage;
			return;
		}

		// List the IR literals which do not alterate the usage of a variable and therefore are used
		// to convert a ref into another ref 
		if (core::analysis::isCallOf(callExpr, mgr.basic.getRefVectorToRefArray()) ||
			core::analysis::isCallOf(callExpr, mgr.basic.getStringToCharPointer()) ) 
		{
			visit( callExpr->getArgument(0) );
			return;
		}

		// This call expression could return a reference to a variable which can be either used or
		// defined. Therefore we have to add this usage to the list of usages 
		addVariable(callExpr, Ref::CALL);

		usage = Ref::UNKNOWN;
		visitNode(callExpr);
		usage = saveUsage;
	}

	void visitLambda(const core::LambdaPtr& lambda) {

		Ref::UseType saveUsage = usage;

		// the parameters has to be treated as definitions for the variable
		const core::Lambda::ParamList& params = lambda->getParameterList();
		std::for_each(params.begin(), params.end(), 
			[&](const core::VariablePtr& cur) { 
				usage = Ref::DEF;	
				addVariable(cur); 
			}
		);
		usage = Ref::USE;
		visit(lambda->getBody());
		usage = saveUsage;
	}

	void visitStatement(const core::StatementPtr& stmt) {
		if ( skipStmts.find(stmt) == skipStmts.end() ) {
			visitNode(stmt);
		}
	}

	// Generic method which recursively visit IR nodes 
	void visitNode(const core::NodePtr& node) {
		const core::Node::ChildList& cl = node->getChildList();
		std::for_each( cl.begin(), cl.end(), [&](const core::NodePtr& cur) { 
				this->visit(cur); 
		} );
	}
};

} // end anonymous namespace 

RefList collectDefUse(const core::NodePtr& root, const core::StatementSet& skipStmt) { 
	RefList ret;
	DefUseCollect duCollVis(ret, skipStmt);
	duCollVis.visit(root);
	return ret;
}

} // end namespace analyis 
} // end namespace insieme 
