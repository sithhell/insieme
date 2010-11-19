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

#include <sstream>

#include <gtest/gtest.h>

#include "insieme/utils/string_utils.h"

#include "insieme/core/statements.h"
#include "insieme/core/expressions.h"
#include "insieme/core/ast_builder.h"

#include "ast_node_test.cc"

using namespace insieme::core;
using namespace insieme::core::lang;

TEST(StatementsTest, Management) {
	NodeManager manager;
	NodeManager manager2;

	BreakStmtPtr bS = BreakStmt::get(manager);
	CompoundStmtPtr nS = CompoundStmt::get(manager);

	CompoundStmtPtr bSC = CompoundStmt::get(manager, bS);
	CompoundStmtPtr nSC = CompoundStmt::get(manager, nS);

	vector<StatementPtr> stmtVec;
	stmtVec.push_back(bS);
	stmtVec.push_back(nSC);
	stmtVec.push_back(nS);
	stmtVec.push_back(bSC);
	CompoundStmtPtr bSCVec = CompoundStmt::get(manager, stmtVec);

	EXPECT_EQ (5, manager.size());
	EXPECT_EQ (0, manager2.size());

	CompoundStmtPtr bSCVec2 = CompoundStmt::get(manager2, stmtVec);

	EXPECT_EQ (5, manager.size());
	EXPECT_EQ (5, manager2.size());

	// TODO:
//	DepthFirstVisitor<NodePtr> stmt2check([&](const NodePtr& cur) {
//		EXPECT_TRUE(manager2.addressesLocal(cur));
//	});
//	stmt2check.visit(bSCVec2);

	EXPECT_FALSE(manager.addressesLocal(bSCVec2));
	EXPECT_TRUE(manager.contains(bSCVec2));
}

TEST(StatementsTest, CreationAndIdentity) {
	NodeManager manager;

	BreakStmtPtr bS = BreakStmt::get(manager);
	EXPECT_EQ(bS, BreakStmt::get(manager));

	CompoundStmtPtr nS = CompoundStmt::get(manager);
	EXPECT_NE(*bS, *nS);
}

TEST(StatementsTest, CompoundStmt) {
	NodeManager stmtMan;
	BreakStmtPtr bS = BreakStmt::get(stmtMan);
	ContinueStmtPtr cS = ContinueStmt::get(stmtMan);

	CompoundStmtPtr empty = CompoundStmt::get(stmtMan);
	CompoundStmtPtr bSC = CompoundStmt::get(stmtMan, bS);
	vector<StatementPtr> stmtVec;
	stmtVec.push_back(bS);
	CompoundStmtPtr bSCVec = CompoundStmt::get(stmtMan, stmtVec);
	EXPECT_EQ(bSC , bSCVec);
	EXPECT_EQ(*bSC , *bSCVec);
	stmtVec.push_back(cS);
	CompoundStmtPtr bScSCVec = CompoundStmt::get(stmtMan, stmtVec);
	EXPECT_NE(bSC , bScSCVec);
	EXPECT_NE(bSC->hash() , bScSCVec->hash());
	EXPECT_EQ((*bSC)[0], (*bScSCVec)[0]);
	EXPECT_EQ("{break; continue;}", toString(*bScSCVec));
}

TEST(StatementsTest, IntLiterals) {
	ASTBuilder builder;

	{
		LiteralPtr intLit = builder.literal(builder.getNodeManager().basic.getInt2(), "-10");
		int val = intLit->getValueAs<int>();
		EXPECT_EQ(val, -10);
	}
	{
		LiteralPtr intLit = builder.literal(builder.getNodeManager().basic.getInt2(), "0x10");
		unsigned long val = intLit->getValueAs<unsigned long>();
		EXPECT_EQ(static_cast<unsigned>(16), val);
	}
	{
		LiteralPtr intLit = builder.literal(builder.getNodeManager().basic.getInt2(), "-0x10");
		short val = intLit->getValueAs<short>();
		EXPECT_EQ(-16, val);
	}
	{
		LiteralPtr intLit = builder.literal(builder.getNodeManager().basic.getInt2(), "010");
		unsigned short val = intLit->getValueAs<unsigned short>();
		EXPECT_EQ(static_cast<unsigned>(8), val);
	}
}

TEST(StatementsTest, RealLiterals) {
	ASTBuilder builder;

	{
		LiteralPtr floatLit = builder.literal(builder.getNodeManager().basic.getFloat(), "0.4");
		float val = floatLit->getValueAs<float>();
		EXPECT_FLOAT_EQ (val, 0.4);
	}
	{
		LiteralPtr intLit = builder.literal(builder.getNodeManager().basic.getDouble(), "0.00001");
		double val = intLit->getValueAs<double>();
		EXPECT_DOUBLE_EQ (val, 0.00001);
	}
}

TEST(StatementsTest, DefaultParams) {
	ASTBuilder builder;

	LiteralPtr one = builder.literal(builder.getNodeManager().basic.getIntGen(), "1");
	DeclarationStmtPtr decl = builder.declarationStmt(builder.getNodeManager().basic.getIntGen(), one);
	ForStmtPtr forStmt = builder.forStmt(decl, decl, one, one);

	EXPECT_EQ(one, forStmt->getStep());
}



TEST(StatementsTest, Break) {
	NodeManager manager;

	BreakStmtPtr stmt = BreakStmt::get(manager);

	EXPECT_EQ ("break", toString(*stmt));

	// check hash codes, children and cloning
	basicNodeTests(stmt, Node::ChildList());
}

TEST(StatementsTest, Continue) {
	NodeManager manager;

	ContinueStmtPtr stmt = ContinueStmt::get(manager);

	EXPECT_EQ ("continue", toString(*stmt));

	// check hash codes, children and cloning
	basicNodeTests(stmt, Node::ChildList());
}

TEST(StatementsTest, Return) {
	NodeManager manager;

	LiteralPtr literal = Literal::get(manager, manager.basic.getInt4(), "12");
	ReturnStmtPtr stmt = ReturnStmt::get(manager, literal);

	EXPECT_EQ ("return 12", toString(*stmt));

	// check hash codes, children and cloning
	basicNodeTests(stmt, toVector<NodePtr>(literal));
}

TEST(StatementsTest, Declaration) {
	NodeManager manager;

	LiteralPtr literal = Literal::get(manager, manager.basic.getInt4(), "12");
	DeclarationStmtPtr stmt = DeclarationStmt::get(manager, Variable::get(manager, manager.basic.getInt4(), 1), literal);

	EXPECT_EQ ("int<4> v1 = 12", toString(*stmt));

	// check hash codes, children and cloning
	VariablePtr varExpr = Variable::get(manager, manager.basic.getInt4(), 1);
	basicNodeTests(stmt, toVector<NodePtr>(varExpr, literal));
}

TEST(StatementsTest, Compound) {
	NodeManager manager;

	LiteralPtr literal = Literal::get(manager, manager.basic.getInt4(), "12");
	DeclarationStmtPtr stmt1 = DeclarationStmt::get(manager, Variable::get(manager, manager.basic.getInt4(), 1), literal);
	DeclarationStmtPtr stmt2 = DeclarationStmt::get(manager, Variable::get(manager, manager.basic.getInt4(), 2), literal);
	DeclarationStmtPtr stmt3 = DeclarationStmt::get(manager, Variable::get(manager, manager.basic.getInt4(), 3), literal);

	CompoundStmtPtr cs0 = CompoundStmt::get(manager);
	CompoundStmtPtr cs1 = CompoundStmt::get(manager, toVector<StatementPtr>(stmt1));
	CompoundStmtPtr cs2 = CompoundStmt::get(manager, toVector<StatementPtr>(stmt1, stmt2));
	CompoundStmtPtr cs3 = CompoundStmt::get(manager, toVector<StatementPtr>(stmt1, stmt2, stmt3));

	CompoundStmtPtr all[] = {cs0, cs1, cs2, cs3 };
	for (int i=0; i<4; i++) {
		for (int j=0; j<4; j++) {
			EXPECT_EQ ( i==j, *all[i] == *all[j]);
			EXPECT_EQ ( i==j, all[i]->hash() == all[j]->hash());
		}
	}

	EXPECT_EQ ("{}", toString(*cs0));
	EXPECT_EQ ("{int<4> v1 = 12;}", toString(*cs1));
	EXPECT_EQ ("{int<4> v1 = 12; int<4> v2 = 12;}", toString(*cs2));
	EXPECT_EQ ("{int<4> v1 = 12; int<4> v2 = 12; int<4> v3 = 12;}", toString(*cs3));

	// check hash codes, children and cloning
	basicNodeTests(cs0, toVector<NodePtr>());
	basicNodeTests(cs1, toVector<NodePtr>(stmt1));
	basicNodeTests(cs2, toVector<NodePtr>(stmt1,stmt2));
	basicNodeTests(cs3, toVector<NodePtr>(stmt1,stmt2,stmt3));
}

TEST(StatementsTest, For) {
	NodeManager manager;

	LiteralPtr start = Literal::get(manager, manager.basic.getInt4(), "1");
	LiteralPtr end   = Literal::get(manager, manager.basic.getInt4(), "9");
	LiteralPtr step  = Literal::get(manager, manager.basic.getInt4(), "2");

	DeclarationStmtPtr decl = DeclarationStmt::get(manager, Variable::get(manager, manager.basic.getInt4(), 1), start);
	StatementPtr body = manager.basic.getNoOp();

	ForStmtPtr stmt = ForStmt::get(manager, decl, body, end, step);

	EXPECT_EQ ("for(int<4> v1 = 1 .. 9 : 2) {}", toString(*stmt));

	// check hash codes, children and cloning
	basicNodeTests(stmt, toVector<NodePtr>(decl, end, step, body));
}

TEST(StatementsTest, While) {
	NodeManager manager;

	LiteralPtr condition = Literal::get(manager, manager.basic.getBool(), "true");
	StatementPtr body = manager.basic.getNoOp();

	WhileStmtPtr stmt = WhileStmt::get(manager, condition, body);

	EXPECT_EQ ("while(true) {}", toString(*stmt));

	// check hash codes, children and cloning
	basicNodeTests(stmt, toVector<NodePtr>(condition, body));
}


TEST(StatementsTest, If) {
	NodeManager manager;

	VariablePtr var = Variable::get(manager, manager.basic.getBool(), 1);
	StatementPtr then = manager.basic.getNoOp();
	StatementPtr other = manager.basic.getNoOp();

	IfStmtPtr stmt = IfStmt::get(manager, var, then, other);

	EXPECT_EQ ("if(v1) {} else {}", toString(*stmt));

	// check hash codes, children and cloning
	basicNodeTests(stmt, toVector<NodePtr>(var, then, other));
}

TEST(StatementsTest, Switch) {
	NodeManager manager;

	VariablePtr var = Variable::get(manager, manager.basic.getInt4(), 1);

	LiteralPtr literalA = Literal::get(manager, manager.basic.getInt4(), "1");
	LiteralPtr literalB = Literal::get(manager, manager.basic.getInt4(), "2");

	StatementPtr caseA = manager.basic.getNoOp();
	StatementPtr caseB = ContinueStmt::get(manager);


	std::vector<SwitchStmt::Case> cases;
	cases.push_back(SwitchStmt::Case(literalA, caseA));
	cases.push_back(SwitchStmt::Case(literalB, caseB));
	StatementPtr other = BreakStmt::get(manager);

	SwitchStmtPtr stmt = SwitchStmt::get(manager, var, cases, other);

	EXPECT_EQ ("switch(v1) [ case 1: {} | case 2: continue | default: break ]", toString(*stmt));

	// check hash codes, children and cloning
	auto list = toVector<NodePtr>(var, literalA, caseA, literalB, caseB);
	list.push_back(other);
	basicNodeTests(stmt, list);
}

//DECLARE_NODE_TYPE(SwitchStmt)
