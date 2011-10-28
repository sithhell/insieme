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

#pragma once

#include "insieme/core/forward_decls.h"

#include "insieme/core/ir_pointer.h"
#include "insieme/core/ir_node_traits.h"

#include "insieme/core/ir_node.h"
#include "insieme/core/ir_values.h"
#include "insieme/core/ir_int_type_param.h"
#include "insieme/core/ir_types.h"
#include "insieme/core/ir_expressions.h"
#include "insieme/core/ir_statements.h"
#include "insieme/core/ir_program.h"

namespace insieme {
namespace core {


	class IRBuilder {

		/**
		 * The manager used by this builder to create new nodes.
		 */
		NodeManager& manager;

	public:

		/**
		 * A type used within some signatures mapping variables to values.
		 */
		typedef utils::map::PointerMap<VariablePtr, ExpressionPtr> VarValueMapping;

		/**
		 * Creates a new IR builder working with the given node manager.
		 */
		IRBuilder(NodeManager& manager) : manager(manager) { }

		/**
		 * Obtains a reference to the node manager used by this builder.
		 */
		NodeManager& getNodeManager() const {
			return manager;
		}

		/**
		 * Obtains a reference to the basic generator within the node manager.
		 */
		const lang::BasicGenerator& getLangBasic() const;

		template<typename T, typename ... Children>
		Pointer<const T> get(Children ... child) const {
			return T::get(manager, child ...);
		}

		template<typename T>
		Pointer<const T> get(const NodeList& children) const {
			return T::get(manager, children);
		}

		template<
			NodeType type,
			typename Node = typename to_node_type<type>::type
		>
		Pointer<const Node> get(const NodeList& children) const {
			// use factory method of Node implementation
			return Node::get(manager, children);
		}

		NodePtr get(NodeType type, const NodeList& children) const;


		// --- Imported Standard Factory Methods from Node Types ---

		#include "ir_builder.inl"

		// --- Handle value clases ---

		StringValuePtr stringValue(const char* str) const;
		StringValuePtr stringValue(const string& str) const;

		BoolValuePtr boolValue(bool value) const;
		CharValuePtr charValue(char value) const;
		IntValuePtr intValue(int value) const;
		UIntValuePtr uintValue(unsigned value) const;

		// --- Convenience Utilities ---

		GenericTypePtr genericType(const StringValuePtr& name, const TypeList& typeParams, const vector<IntTypeParamPtr>& intTypeParams);

		StructTypePtr structType(const vector<std::pair<StringValuePtr,TypePtr>>& entries) const;
		UnionTypePtr unionType(const vector<std::pair<StringValuePtr,TypePtr>>& entries) const;

		TupleExprPtr tupleExpr(const ExpressionList& values) const;
		VectorExprPtr vectorExpr(const VectorTypePtr& type, const ExpressionList& values) const;
		StructExprPtr structExpr(const vector<std::pair<StringValuePtr, ExpressionPtr>>& values) const;

		// creates a program - empty or based on the given entry points
		ProgramPtr createProgram(const ExpressionList& entryPoints = ExpressionList());

		// Function Types
		FunctionTypePtr toPlainFunctionType(const FunctionTypePtr& funType) const;
		FunctionTypePtr toThickFunctionType(const FunctionTypePtr& funType) const;

		// Literals
		LiteralPtr stringLit(const std::string& str) const;
		LiteralPtr intLit(const int val) const;
		LiteralPtr uintLit(const unsigned int val) const;


		/**
		 * A factory method for intTypeParam literals.
		 */
		LiteralPtr getIntTypeParamLiteral(const IntTypeParamPtr& param) const;

		/**
		 * A factory method for a identifier literal.
		 */
		LiteralPtr getIdentifierLiteral(const StringValuePtr& value) const;

		/**
		 * A factory method for a type literals.
		 */
		LiteralPtr getTypeLiteral(const TypePtr& type) const;

		/**
		 * A method generating a vector init expression form a scalar.
		 */
		ExpressionPtr scalarToVector(const TypePtr& type, const ExpressionPtr& subExpr) const;

		// Values
		// obtains a zero value - recursively resolved for the given type
		ExpressionPtr getZero(const TypePtr& type) const;

		// Referencing
		CallExprPtr deref(const ExpressionPtr& subExpr) const;
		CallExprPtr refVar(const ExpressionPtr& subExpr) const;
		CallExprPtr refNew(const ExpressionPtr& subExpr) const;
		CallExprPtr assign(const ExpressionPtr& target, const ExpressionPtr& value) const;

		ExpressionPtr invertSign(const ExpressionPtr& subExpr) const;
		// Returns the negation of the passed subExpr (which must be of boolean type)
		// 	       (<BOOL> expr) -> <BOOL> !expr
		ExpressionPtr negateExpr(const ExpressionPtr& subExpr) const;

		// Vectors
		CallExprPtr vectorSubscript(const ExpressionPtr& vec, const ExpressionPtr& index) const;
		//CallExprPtr vectorSubscript(const ExpressionPtr& vec, unsigned index) const;

		// Compound Statements
		CompoundStmtPtr compoundStmt(const StatementPtr& s1, const StatementPtr& s2) const;
		CompoundStmtPtr compoundStmt(const StatementPtr& s1, const StatementPtr& s2, const StatementPtr& s3) const;

		// Call Expressions
		CallExprPtr callExpr(const TypePtr& resultType, const ExpressionPtr& functionExpr) const;
		CallExprPtr callExpr(const TypePtr& resultType, const ExpressionPtr& functionExpr, const ExpressionPtr& arg1) const;
		CallExprPtr callExpr(const TypePtr& resultType, const ExpressionPtr& functionExpr, const ExpressionPtr& arg1, const ExpressionPtr& arg2) const;
		CallExprPtr callExpr(const TypePtr& resultType, const ExpressionPtr& functionExpr, const ExpressionPtr& arg1, const ExpressionPtr& arg2, const ExpressionPtr& arg3) const;

		// For the methods below, the return type is deduced from the functionExpr's function type
		CallExprPtr callExpr(const ExpressionPtr& functionExpr, const vector<ExpressionPtr>& arguments = vector<ExpressionPtr>()) const;
		CallExprPtr callExpr(const ExpressionPtr& functionExpr, const ExpressionPtr& arg1) const;
		CallExprPtr callExpr(const ExpressionPtr& functionExpr, const ExpressionPtr& arg1, const ExpressionPtr& arg2) const;
		CallExprPtr callExpr(const ExpressionPtr& functionExpr, const ExpressionPtr& arg1, const ExpressionPtr& arg2, const ExpressionPtr& arg3) const;


		// Lambda Nodes
		LambdaPtr lambda(const FunctionTypePtr& type, const ParametersPtr& params, const StatementPtr& body) const;
		LambdaPtr lambda(const FunctionTypePtr& type, const VariableList& params, const StatementPtr& body) const;

		// Lambda Expressions
		LambdaExprPtr lambdaExpr(const StatementPtr& body, const ParametersPtr& params) const;
		LambdaExprPtr lambdaExpr(const TypePtr& returnType, const StatementPtr& body, const ParametersPtr& params) const;

		// Direct creation of lambda and bind with capture initialization
		BindExprPtr lambdaExpr(const StatementPtr& body, const VarValueMapping& captureMap, const VariableList& params = VariableList()) const;
		BindExprPtr lambdaExpr(const TypePtr& returnType, const StatementPtr& body, const VarValueMapping& captureMap, const VariableList& params) const;

		BindExprPtr bindExpr(const VariableList& params, const CallExprPtr& call) const;

		// Create a job expression
		JobExprPtr jobExpr(const ExpressionPtr& threadNumRange, const vector<DeclarationStmtPtr>& localDecls, const vector<GuardedExprPtr>& guardedExprs, const ExpressionPtr& defaultExpr) const;

		// Creation of thread number ranges
		CallExprPtr getThreadNumRange(unsigned min) const;
		CallExprPtr getThreadNumRange(unsigned min, unsigned max) const;

		// Direct call expression of getThreadGroup
		CallExprPtr getThreadGroup(ExpressionPtr level = ExpressionPtr()) const;
		CallExprPtr getThreadId(ExpressionPtr level = ExpressionPtr()) const;

		// Direct call expression of barrier
		CallExprPtr barrier(ExpressionPtr threadgroup = ExpressionPtr()) const;

		// Direct call expression of pfor
		CallExprPtr pfor(const ExpressionPtr& body, const ExpressionPtr& start, const ExpressionPtr& end, ExpressionPtr step = ExpressionPtr()) const;

		// Build a Call expression for a pfor that mimics the effect of the given for statement
		CallExprPtr pfor(const ForStmtPtr& initialFor) const;

		/*
		 * creates a function call from a list of expressions
		 */
		ExpressionPtr createCallExprFromBody(StatementPtr body, TypePtr retTy, bool lazy=false) const;

		/**
		 * Creates an expression accessing the corresponding member of the given struct.
		 */
		ExpressionPtr accessMember(ExpressionPtr structExpr, string member) const;

		/**
		 * Creates an expression accessing the corresponding member of the given struct.
		 */
		ExpressionPtr accessMember(ExpressionPtr structExpr, StringValuePtr member) const;

		/**
		 * Creates an expression obtaining a reference to a member of a struct.
		 */
		ExpressionPtr refMember(ExpressionPtr structExpr, StringValuePtr member) const;

		/**
		 * Creates an expression obtaining a reference to a member of a struct.
		 */
		ExpressionPtr refMember(ExpressionPtr structExpr, string member) const;

		/**
		 * Creates an expression accessing the given component of the given tuple value.
		 */
		ExpressionPtr accessComponent(ExpressionPtr tupleExpr, unsigned component) const;
		ExpressionPtr accessComponent(ExpressionPtr tupleExpr, ExpressionPtr component) const;

		/**
		 * Creates an expression accessing the reference to a component of the given tuple value.
		 */
		ExpressionPtr refComponent(ExpressionPtr tupleExpr, unsigned component) const;
		ExpressionPtr refComponent(ExpressionPtr tupleExpr, ExpressionPtr component) const;


		/**
		 * A function obtaining a reference to a NoOp instance.
		 */
		StatementPtr getNoOp() const;

		/**
		 * Tests whether the given node is a no-op.
		 *
		 * @param node the node to be tested
		 * @return true if it is a no-op, false otherwise
		 */
		bool isNoOp(const NodePtr& node) const;

		IfStmtPtr ifStmt(const ExpressionPtr& condition, const StatementPtr& thenBody, const StatementPtr& elseBody = StatementPtr()) const;
		WhileStmtPtr whileStmt(const ExpressionPtr& condition, const StatementPtr& body) const;
		ForStmtPtr forStmt(const DeclarationStmtPtr& var, const ExpressionPtr& end, const ExpressionPtr& step, const StatementPtr& body) const;

		SwitchStmtPtr switchStmt(const ExpressionPtr& switchStmt, const vector<std::pair<ExpressionPtr, StatementPtr>>& cases, const StatementPtr& defaultCase = StatementPtr()) const;

		// Utilities
	private:
		static TypeList extractTypes(const ExpressionList& expressions);
		static TypeList extractParamTypes(const ParametersPtr& params);
		unsigned extractNumberFromExpression(ExpressionPtr& expr) const;
	};

} // namespace core
} // namespace insieme
