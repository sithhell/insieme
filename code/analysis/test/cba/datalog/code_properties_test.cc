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

#include <fstream>

#include <gtest/gtest.h>

#include "insieme/analysis/cba/datalog/code_properties.h"

#include "insieme/core/dump/json_dump.h"
#include "insieme/core/dump/binary_dump.h"

#include "insieme/core/ir_builder.h"

#include "insieme/driver/integration/tests.h"

namespace insieme {
namespace analysis {
namespace cba {
namespace datalog {

	using namespace core;

	auto quickDump = [](const NodePtr &node, const string &file = "insieme_ir_text_dump.txt") {
		std::ofstream outputFile(file);
		if (outputFile.is_open()) {
			core::dump::json::dumpIR(outputFile, node);
			outputFile.close();
		} else {
			std::cerr << "Could not open file!" << std::endl;
		}
	};

	TEST(CodeProperties, DISABLED_DumpTextToFile) {
		using namespace driver::integration;

		NodeManager mgr;
		IRBuilder builder(mgr);


		auto addresses = builder.parseAddressesStatement(
			"{ var int<4> x = 12; $x$; }"
		);

		auto code = getCase("seq/c/pyramids")->load(mgr);

		ASSERT_EQ(1, addresses.size());

		std::ofstream outputFile("/tmp/insieme_ir_text_dump.txt");
		if (outputFile.is_open()) {
			//core::dump::binary::dumpIR(outputFile, code);
			//dumpText(addresses[0].getRootAddress(), outputFile, true);
			core::dump::json::dumpIR(outputFile, code);
			outputFile.close();
		} else {
			std::cerr << "Could not open file!" << std::endl;
		}
	}

	TEST(CodeProperties, DefinitionPoint_LambdaParameter) {
		NodeManager mgr;
		IRBuilder builder(mgr);
		Context ctxt;

		auto addresses = builder.parseAddressesExpression(
			"( x : int<4> ) -> int<4> { return $x$; }"
		);

		ASSERT_EQ(1, addresses.size());

		auto var = addresses[0].as<CallExprAddress>().getArgument(0).as<VariableAddress>();
		auto param = var.getRootAddress().as<LambdaExprAddress>()->getParameterList()[0];

		std::cout << "Parameter: " << param << "\n";
		std::cout << "Variable:  " << var << "\n";

		quickDump(var.getRootNode());

		EXPECT_EQ(param, getDefinitionPoint(ctxt, var, false));

	}


	TEST(CodeProperties, DefinitionPoint_BindParameter) {
		NodeManager mgr;
		IRBuilder builder(mgr);
		Context ctxt;

		auto addresses = builder.parseAddressesExpression(
			"( x : int<4> ) => $x$"
		);

		ASSERT_EQ(1, addresses.size());

		auto var = addresses[0].as<VariableAddress>();
		auto param = var.getRootAddress().as<BindExprAddress>()->getParameters()[0];

		std::cout << "Parameter: " << param << "\n";
		std::cout << "Variable:  " << var << "\n";

		auto definition = getDefinitionPoint(ctxt, var, false);
		EXPECT_TRUE(definition);
		EXPECT_EQ(param, definition);

	}


	TEST(CodeProperties, DefinitionPoint_BindParameter_2) {
		NodeManager mgr;
		IRBuilder builder(mgr);
		Context ctxt;

		std::map<std::string,core::NodePtr> symbols;
		symbols["z"] = builder.variable(builder.parseType("int<4>"));

		auto addresses = builder.parseAddressesStatement(
			"{ var int<4> y = 4; ( x : int<4>, w : int<4> ) => $x$ + $y$ + $z$ + $w$; }",
			symbols
		);

		ASSERT_EQ(4, addresses.size());

		auto x = addresses[0].as<VariableAddress>();
		auto y = addresses[1].as<VariableAddress>();
		auto z = addresses[2].as<VariableAddress>();
		auto w = addresses[3].as<VariableAddress>();

		auto decl = x.getRootAddress().as<CompoundStmtAddress>()[0].as<DeclarationStmtAddress>();
		auto bind = x.getRootAddress().as<CompoundStmtAddress>()[1].as<BindExprAddress>();

		auto defX = bind->getParameters()[0];
		auto defY = decl->getVariable();
		auto defW = bind->getParameters()[1];

		EXPECT_EQ(defX,getDefinitionPoint(ctxt, x));
		EXPECT_EQ(defY,getDefinitionPoint(ctxt, y));
		EXPECT_FALSE(getDefinitionPoint(ctxt, z));
		EXPECT_EQ(defW,getDefinitionPoint(ctxt, w));

	}

	TEST(CodeProperties, DefinitionPoint_BindParameter_3) {
		NodeManager mgr;
		IRBuilder builder(mgr);
		Context ctxt;

		std::map<std::string,core::NodePtr> symbols;
		symbols["y"] = builder.variable(builder.parseType("bool"));

		auto addresses = builder.parseAddressesExpression(
			"(x:bool)=>($x$ && $y$)",
			symbols
		);

		ASSERT_EQ(2, addresses.size());

		auto x = addresses[0].as<VariableAddress>();
		auto y = addresses[1].as<CallExprAddress>()->getArgument(0).as<VariableAddress>();

		// there is a bug in the parser, marking the wrong y => fix this
		y = y.getParentAddress(10).as<CallExprAddress>().getArgument(0).as<VariableAddress>();

		auto bind = x.getRootAddress().as<BindExprAddress>();
		auto defX = bind->getParameters()[0];

		EXPECT_EQ(defX,getDefinitionPoint(ctxt, x));
		EXPECT_FALSE(getDefinitionPoint(ctxt, y));

	}

	TEST(CodeProperties, DefinitionPoint_BindParameter_4) {
		NodeManager mgr;
		IRBuilder builder(mgr);
		Context ctxt;

		std::map<std::string,core::NodePtr> symbols;
		symbols["x"] = builder.variable(builder.parseType("bool"));

		auto addresses = builder.parseAddressesExpression(
			"(y:bool)=>($x$ && $y$)",
			symbols
		);

		ASSERT_EQ(2, addresses.size());

		auto x = addresses[0].as<VariableAddress>();
		auto y = addresses[1].as<CallExprAddress>()->getArgument(0).as<VariableAddress>();

		// there is a bug in the parser, marking the wrong y => fix this
		y = y.getParentAddress(10).as<CallExprAddress>().getArgument(0).as<VariableAddress>();

		auto bind = x.getRootAddress().as<BindExprAddress>();
		auto defY = bind->getParameters()[0];

		EXPECT_FALSE(getDefinitionPoint(ctxt, x));
		EXPECT_EQ(defY,getDefinitionPoint(ctxt, y));

	}


	TEST(CodeProperties, DefinitionPoint_LocalVariable) {
		NodeManager mgr;
		IRBuilder builder(mgr);
		Context ctxt;

		auto addresses = builder.parseAddressesStatement(
			"{ var int<4> x = 12; $x$; }"
		);

		ASSERT_EQ(1, addresses.size());

		auto var = addresses[0].as<VariableAddress>();
		auto param = var.getRootAddress().as<CompoundStmtAddress>()[0].as<DeclarationStmtAddress>()->getVariable();

		std::cout << "Parameter: " << param << "\n";
		std::cout << "Variable:  " << var << "\n";

		EXPECT_EQ(param, getDefinitionPoint(ctxt, var,false));

	}

	TEST(CodeProperties, ExitPointAnalysis) {
		core::NodeManager mgr;
		IRBuilder builder(mgr);
		Context ctxt;

		auto t = builder.parseExpr(
				"() -> int<4> { "
				"	() -> bool {"
				"		return false; "
				"	};"
				"	return 1; "
				"	return 2; "
				"	() -> int<4> {"
				"		return 1; "
				"	};"
				"}"
		);

		visitDepthFirstOnce(core::NodeAddress(t), [&](const core::LambdaAddress &lambda) {
			auto results = performExitPointAnalysis(ctxt, lambda);

			std::set<core::ReturnStmtAddress> control;
			visitDepthFirstOnce(lambda, [&](const core::ReturnStmtAddress &returnStmt) {
				auto rsa = core::cropRootNode(returnStmt, lambda);

				if (rsa.getDepth() > 3)
					return; /* These are returns of sub-lambdas! */
				control.insert(rsa);
			});

			EXPECT_EQ(results, control);
		});
	}


	TEST(CodeProperties, DISABLED_LargerCode) {
		using namespace driver::integration;

		core::NodeManager mgr;
		IRBuilder builder(mgr);
		Context ctxt;

		for (auto name : {"seq/c/loop_transform", "seq/c/pyramids", "seq/c/pendulum", "seq/c/mqap", "seq/c/loops", "seq/c/transpose"}) {
			auto code = getCase(name)->load(mgr);
			EXPECT_TRUE(code);
			EXPECT_TRUE(getTopLevelNodes(ctxt, code));
		}
	}

	TEST(CodeProperties, IsPolymorph) {

		NodeManager mgr;
		IRBuilder builder(mgr);
		Context ctxt;

		EXPECT_FALSE(isPolymorph(ctxt, builder.parseType("bool")));
		EXPECT_FALSE(isPolymorph(ctxt, builder.parseType("char")));
		EXPECT_FALSE(isPolymorph(ctxt, builder.parseType("int")));
		EXPECT_FALSE(isPolymorph(ctxt, builder.parseType("uint")));
		EXPECT_FALSE(isPolymorph(ctxt, builder.parseType("string")));

		EXPECT_TRUE(isPolymorph(ctxt, builder.parseType("'a")));

		EXPECT_FALSE(isPolymorph(ctxt, builder.parseType("(bool)")));
		EXPECT_FALSE(isPolymorph(ctxt, builder.parseType("(bool,int<4>)")));

		EXPECT_TRUE(isPolymorph(ctxt, builder.parseType("('a)")));
		EXPECT_TRUE(isPolymorph(ctxt, builder.parseType("('a,bool)")));

		EXPECT_FALSE(isPolymorph(ctxt, builder.parseType("int<4>")));
		EXPECT_FALSE(isPolymorph(ctxt, builder.parseType("ref<int<4>>")));
		EXPECT_TRUE(isPolymorph(ctxt, builder.parseType("array<'a,'b>")));
		EXPECT_FALSE(isPolymorph(ctxt, builder.parseType("(int<4>)->bool")));
		EXPECT_FALSE(isPolymorph(ctxt, builder.parseType("(string, int<4>)->uint<4>")));

	}

	TEST(CodeProperties, TopLevelTerm) {
		NodeManager mgr;
		IRBuilder builder(mgr);
		Context ctxt;

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("('a)")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("{var int<4> a = 1; if (a > 0) { a+1; }}")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("int<4>")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("ref<int<4>>")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("struct { x : int<4>; }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("decl struct B; def struct B { x : int<4>; }; B")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("decl struct B; def struct B { x : ref<B>; }; B")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("decl struct data; def struct data { x : ptr<data>; }; data")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("decl struct A; decl struct B; def struct A { x : ref<B>; }; def struct B { x : ref<A>; }; B")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("decl struct A; decl struct B; def struct A { x : ptr<B>; }; def struct B { x : ptr<A>; }; A")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("decl struct A; decl struct B; def struct A { x : ref<A>; }; def struct B { y : ref<A>; }; B")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("def struct A { lambda retA = () -> A { return *this; } }; A")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("{ while(true) { continue; } }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("{ while(true) { break; } }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("{ for(int<4> i = 1 .. 10 ) { continue; } }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("decl f : (int<4>)->int<4>;"
			                         "def f = (x : int<4>)->int<4> {"
			                         "	return (x==0)?1:f(x-1)*x;"
			                         "}; f")));

	}

	TEST(CodeProperties, Types) {
		NodeManager mgr;
		IRBuilder builder(mgr);
		Context ctxt;

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("int<4>")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("someweirdname<>")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("'a")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("'a...")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("'a<>")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("'a...<>")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("vector<int<4>, 4>")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("vector<'a, 4>")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("ref<'a,f,f,plain>")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("ref<'a,f,t,plain>")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("ref<'a,t,f,plain>")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("ref<'a,t,t,plain>")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("ref<'a,f,f,cpp_ref>")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("ref<'a,f,f,cpp_rref>")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("ptr<'a>")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("ptr<'a,f,f>")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("ptr<'a,t,f>")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("ptr<'a,f,t>")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("ptr<'a,t,t>")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("struct { a : int<4>; b : int<5>; }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("struct name { a : int<4>; b : int<5>; }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("let papa = t<11> in struct name : [papa] { a : int<4>; b : int<5>; }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("let papa = t<11> in struct name : [private papa] { a : int<4>; b : int<5>; }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("let papa = t<11> in struct name : [protected papa] { a : int<4>; b : int<5>; }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("let papa = t<11> in struct name : [public papa] { a : int<4>; b : int<5>; }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("let int = int<4> in int")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("rf<int<4>>")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("() -> unit")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("(int<4>) -> int<4>")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("( int<4> , rf<int<4>>) -> int<4>")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("( int<4> , rf<int<4>>) => int<4>")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("(ary<'elem,'n>, vector<uint<8>,'n>) -> 'elem")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("let class = struct name { a : int<4>; b : int<5>; } in "
		                          "class::()->int<4> ")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("let class = struct name { a : int<4>; b : int<5>; } in "
		                          "class::()~>int<4> ")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("let class = struct name { a : int<4>; b : int<5>; } in "
		                          "~class::()")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("let class = struct name { a : int<4>; b : int<5>; } in "
		                          "class::()")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("struct C { field : int<4>; }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("(rf<ary<rf<ary<struct{int : int<4>; float : real<4>; },1>>,1>>,"
		                          " rf<ary<rf<ary<real<4>,1>>,1>>,"
		                          " rf<ary<uint<8>,1>>)")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("struct class {"
		                          "  a : int<4>;"
		                          "}")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("struct class {"
		                          "  a : int<4>;"
		                          "  b : int<4>;"
		                          "}")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("struct class {"
		                          "  a : int<4>;"
		                          "  ctor () { }"
		                          "}")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("struct class {"
		                          "  a : int<4>;"
		                          "  ctor () { }"
		                          "  ctor (a : int<4>) { }"
		                          "}")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("struct class {"
		                          "  a : int<4>;"
		                          "  dtor () { }"
		                          "}")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("struct class {"
		                          "  a : int<4>;"
		                          "  dtor virtual () { }"
		                          "}")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("struct class {"
		                          "  a : int<4>;"
		                          "  lambda f = () -> int<4> { return 1; }"
		                          "}")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("struct class {" //multiple functions with the same name
		                          "  a : int<4>;"
		                          "  lambda f = () -> int<4> { return 1; }"
		                          "  lambda f = (a : int<4>) -> int<4> { return 1; }"
		                          "}")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("struct class {"
		                          "  a : int<4>;"
		                          "  lambda f = () -> int<4> { return 1; }"
		                          "  lambda g = (b : int<4>) -> int<4> { return b; }"
		                          "}")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("struct class {"
		                          "  a : int<4>;"
		                          "  const lambda b = () -> int<4> { return 1; }"
		                          "  volatile lambda c = () -> int<4> { return 1; }"
		                          "  volatile const lambda d = (a : int<4>) -> int<4> { return 1; }"
		                          "  const volatile lambda e = (a : int<4>) -> int<4> { return 1; }"
		                          "  virtual const lambda f = () -> int<4> { return 1; }"
		                          "  virtual volatile lambda g = () -> int<4> { return 1; }"
		                          "  virtual volatile const lambda h = (a : int<4>) -> int<4> { return 1; }"
		                          "  virtual const volatile lambda i = (a : int<4>) -> int<4> { return 1; }"
		                          "}")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("struct class {"
		                          "  a : int<4>;"
		                          "  pure virtual b : () -> int<4>"
		                          "  pure virtual const c : () -> int<4>"
		                          "  pure virtual volatile d : () -> int<4>"
		                          "  pure virtual volatile const e : (int<4>) -> int<4>"
		                          "  pure virtual const volatile f : (int<4>) -> int<4>"
		                          "}")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("struct class {"
		                          "  a : int<4>;"
		                          "  ctor () { }"
		                          "  ctor (a : int<4>) { }"
		                          "  dtor () { }"
		                          "  lambda f = () -> int<4> { return 1; }"
		                          "  virtual const volatile lambda g = () -> int<4> { return 1; }"
		                          "  pure virtual h : () -> int<4>"
		                          "}")));
	}

	TEST(CodeProperties, Expressions) {
		NodeManager mgr;
		IRBuilder builder(mgr);
		Context ctxt;

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("true?1:0")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("type_lit(1)")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("1")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("1u")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("1l")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("1ul")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("1ll")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("1ull")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("1.0f")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("1.0")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("2.3E-5f")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("2.0E+0")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("1 + 3")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("1 - 0")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("1 * 0")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("1 / 0")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("1 | 0")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("1 & 0")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("1 ^ 0")));

		// precedence
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("1 + 0 * 5")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("1 * 0 + 5")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("(1.0)")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("((1.0))")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("((1.0) + 4.0)")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("1 + 2 * 3")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("(1 + 2) * 3")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("def struct x { a : int<4>; b : int<4>; }; <ref<x>> { 4, 5 }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("def struct x { a : int<4>; }; <ref<x>> { 4 }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("def struct x { }; <ref<x>> { }")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseType("def union uni { a : int<4>; lambda f = ()->unit {} }; uni")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("def union uni { a : int<4>; lambda f = ()->unit {} }; { var ref<uni,f,f,plain> a; }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("def union uni { a : int<4>; lambda f = ()->unit {} }; { <ref<uni>> { 4 }; }")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("(_ : 'a) -> bool { return true; }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("(x : 'a) -> 'a { return x+CAST('a) 3; }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("(x : 'a) => x+CAST('a) 3")));

		// return type deduction
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("() => { return true; }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("(x : bool) => { if(x) { return true; } else { return false; } }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("(x : bool) => { if(x) { return 1; } else { return -5; } }")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("type_lit(int<4>)")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("decl f : ()->unit;"
		                                "let f = ()->unit { 5; ()->unit { f(); } (); } in f")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("(v : int<4>, exp : int<4>) -> int<4> { "
		                                "	let one = (_ : int<4>)=>4; "
		                                "	let two = (x : int<4>)=>x+exp; "
		                                "    return one(two(exp));"
		                                "}")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("(v : int<4>, exp : int<4>) -> int<4> { "
		                                "	let one = (_ : int<4>)-> int<4> { return 4;  };"
		                                "	let two = (x : int<4>)-> int<4> { return x+5; };"
		                                "    return one(two(exp));"
		                                "}")));
	}

	TEST(CodeProperties, Statements) {
		NodeManager mgr;
		IRBuilder builder(mgr);
		Context ctxt;

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("1;")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("1u;")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("1.0f;")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("1.0;")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("1 + 3;")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("1 - 0;")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("1 * 0;")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("1 / 0;")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("1 | 0;")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("1 & 0;")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("1 ^ 0;")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("(1.0);")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("((1.0));")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("{ var int<4> x = 0; x+1; }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("{ var ref<int<4>,f,f,plain> x; }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("{ auto x = 0; x+1; }")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("if ( true ) {}")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("if ( true ) {} else {}")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("if ( true ) { if ( false ) { } else { } }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("if ( true ) { if ( false ) { } else { } } else { } ")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("if( false ) { return 0; } else { return 1+2; }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("if( false ) { return 0; }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("if(1 != 0) { return 0; }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("while ( true ) { 1+1; }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("while ( false ) { 1+1; }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("while ( false || true ) { 1+1; }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("for ( int<4> it = 1 .. 3) { 1+1; }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("for ( int<4> it = 1 .. 3: 2) { 1+1; }")));
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("for ( int<4> it = 1 .. 3: 2) { 1+1; }")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("switch (2) { case 1: { 1; } case 2: { 2; } }")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("{ }")));


		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("alias type = struct a { a : int<4>; b : int<8>; };"
		                               "{"
		                               "    var ref<type,f,f,plain> variable;"
		                               "    var ref<rf<type>,f,f,plain> var2;"
		                               "    auto var3 = var2;"
		                               "}")));

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("alias class = struct name { a : int<2>; };"
		                               "alias collection = array<class, 10u>;"
		                               "{"
		                               "    var ref<collection,f,f,plain> x;"
		                               "    var int<2> y = CAST(int<2>) 5;"
		                               "    x[5].a = y;"
		                               "}")));

		// return statements may also declare a variable which can be used in the return expression itself
		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("def struct A { a : int<4>; };"
		                               "def foo = () -> A {"
		                               "  return A::(ref_decl(type_lit(ref<A>))) in ref<A>;"
		                               "};"
		                               "foo();")));
	}

	TEST(CodeProperties, TryCatch) {
		NodeManager mgr;
		IRBuilder builder(mgr);
		Context ctxt;

		EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("<ref<int<6>>> { 3 }")));
	        EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("throw true;")));
	        EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseStmt("try {} catch (v1 : bool) {v1;} catch (v2 : int<4>) {v2;}")));
	        EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseProgram("int main() { return 0; }")));
	        EXPECT_TRUE(getTopLevelNodes(ctxt, builder.parseExpr("spawn 14")));
	}

	TEST(CodeProperties, HappensBefore_1) {
		NodeManager mgr;
		IRBuilder builder(mgr);
		Context ctxt;

		auto addresses = builder.parseAddressesStatement(
				"{"
				"	$1$;"
				"	$2$;"
				"}"
		);

		ASSERT_EQ(2,addresses.size());
		auto a = addresses[0].as<StatementAddress>();
		auto b = addresses[1].as<StatementAddress>();

		// simple cases
		EXPECT_FALSE(happensBefore(ctxt, a,a));
		EXPECT_FALSE(happensBefore(ctxt, b,b));

		// just a bit more complicated
		EXPECT_TRUE(happensBefore(ctxt, a,b));
		EXPECT_FALSE(happensBefore(ctxt, b,a));
	}

} // end namespace datalog
} // end namespace cba
} // end namespace analysis
} // end namespace insieme



