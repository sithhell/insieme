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

#include <iostream>
#include <gtest/gtest.h>
#include "insieme/core/parser/detail/driver.h"
#include "insieme/core/ir.h"
#include "insieme/core/ir_builder.h"
#include "insieme/core/checks/full_check.h"


namespace insieme {
namespace core {
namespace parser {

	using namespace detail;

	bool test_type(NodeManager& nm, const std::string& x) {
		InspireDriver driver(x, nm);
		driver.parseType();
		if(driver.result) {
//			dumpColor(driver.result);
//			std::cout << " ============== TEST ============ " << std::endl;
			auto msg = checks::check(driver.result);
			EXPECT_TRUE(msg.empty()) << msg;
		}
		return driver.result;
	}

	TEST(IR_Parser, Types) {
		NodeManager nm;
		EXPECT_TRUE(test_type(nm, "int<4>"));
		EXPECT_TRUE(test_type(nm, "someweirdname<>"));
		EXPECT_TRUE(test_type(nm, "vector<int<4>, 4>"));
		EXPECT_TRUE(test_type(nm, "vector<'a, 4>"));
		EXPECT_TRUE(test_type(nm, "struct {  a : int<4>; b : int<5> }"));
		EXPECT_TRUE(test_type(nm, "struct name { a : int<4> ; b : int<5> }"));
		EXPECT_TRUE(test_type(nm, "let papa = t<11> in struct name : [papa] { int<4> a; int<5> b}"));
		EXPECT_TRUE(test_type(nm, "struct { int<4> a; int<5> b;}"));
		EXPECT_TRUE(test_type(nm, "let int = int<4> in int"));

		EXPECT_TRUE(test_type(nm, "( int<4> , rf<int<4>>) -> int<4>"));
		EXPECT_TRUE(test_type(nm, "( int<4> , rf<int<4>>) => int<4>"));
		EXPECT_TRUE(test_type(nm, "(ary<'elem,'n>, vector<uint<8>,'n>) -> 'elem"));

		EXPECT_TRUE(test_type(nm, "let class = struct name { int<4> a; int<5> b} in"
		                          "class::()->int<4> "));
		EXPECT_TRUE(test_type(nm, "let class = struct name { int<4> a; int<5> b} in"
		                          "class::()~>int<4> "));
		EXPECT_TRUE(test_type(nm, "let class = struct name { int<4> a; int<5> b} in"
		                          "~class::()"));
		EXPECT_TRUE(test_type(nm, "let class = struct name { int<4> a; int<5> b} in"
		                          "class::()"));

		EXPECT_TRUE(test_type(nm, "struct C { int<4> field; }"));
		EXPECT_TRUE(test_type(nm, "(rf<ary<rf<ary<struct{int<4> int; real<4> float },1>>,1>>,"
		                          " rf<ary<rf<ary<real<4>,1>>,1>>,"
		                          " rf<ary<uint<8>,1>>)"));

	}

	bool test_expression(NodeManager& nm, const std::string& x) {
		InspireDriver driver(x, nm);
		driver.parseExpression();
		if(driver.result) {
//			std::cout << driver.result << std::endl;
//			dumpColor(driver.result);
			auto msg = checks::check(driver.result);
			EXPECT_TRUE(msg.empty()) << msg;
		} else {
			driver.printErrors();
		}
		//   std::cout << " ============== TEST ============ " << std::endl;
		return driver.result;
	}

	TEST(IR_Parser, Strings) {
		NodeManager nm;
		EXPECT_TRUE(test_expression(nm, "lit(\"foo\")"));
		EXPECT_TRUE(test_expression(nm, "lit(\"\"foo\"\")"));
		EXPECT_TRUE(test_expression(nm, "lit(\"5\")"));
		EXPECT_TRUE(test_expression(nm, "lit(\"\"foo\\nbar\"\")"));
	}

	TEST(IR_Parser, Expressions) {
		NodeManager nm;
		EXPECT_TRUE(test_expression(nm, "true?1:0"));
		EXPECT_TRUE(test_expression(nm, "param(1)"));

		EXPECT_TRUE(test_expression(nm, "1"));
		EXPECT_TRUE(test_expression(nm, "1u"));
		EXPECT_TRUE(test_expression(nm, "1l"));
		EXPECT_TRUE(test_expression(nm, "1ul"));
		EXPECT_TRUE(test_expression(nm, "1ll"));
		EXPECT_TRUE(test_expression(nm, "1ull"));

		EXPECT_TRUE(test_expression(nm, "1.0f"));
		EXPECT_TRUE(test_expression(nm, "1.0"));

		EXPECT_TRUE(test_expression(nm, "1 + 3"));
		EXPECT_TRUE(test_expression(nm, "1 - 0"));
		EXPECT_TRUE(test_expression(nm, "1 * 0"));
		EXPECT_TRUE(test_expression(nm, "1 / 0"));
		EXPECT_TRUE(test_expression(nm, "1 | 0"));
		EXPECT_TRUE(test_expression(nm, "1 & 0"));
		EXPECT_TRUE(test_expression(nm, "1 ^ 0"));

		// precedence
		EXPECT_TRUE(test_expression(nm, "1 + 0 * 5"));
		EXPECT_TRUE(test_expression(nm, "1 * 0 + 5"));

		EXPECT_TRUE(test_expression(nm, "(1.0)"));
		EXPECT_TRUE(test_expression(nm, "((1.0))"));
		EXPECT_TRUE(test_expression(nm, "((1.0) + 4.0)"));

		EXPECT_TRUE(test_expression(nm, "1 + 2 * 3"));
		EXPECT_TRUE(test_expression(nm, "(1 + 2) * 3"));

		EXPECT_TRUE(test_expression(nm, "let uni = union{ int<4> a }; union uni {a=4}"));
		EXPECT_TRUE(test_expression(nm, "let x = struct{ int<4> a }; struct x { 4}"));
		EXPECT_TRUE(test_expression(nm, "let x = struct{ }; struct x { }"));

		EXPECT_FALSE(test_expression(nm, "x"));

		EXPECT_TRUE(test_expression(nm, "lambda ('a _) -> bool { return true; }"));
		EXPECT_TRUE(test_expression(nm, "lambda ('a x) -> 'a { return x+CAST('a) 3; }"));
		EXPECT_TRUE(test_expression(nm, "lambda ('a x) => x+CAST('a) 3"));

		// return type deduction
		EXPECT_TRUE(test_expression(nm, "lambda () => { return true; }"));
		EXPECT_TRUE(test_expression(nm, "lambda (bool x) => { if(x) { return true; } else { return false; } }"));
		EXPECT_TRUE(test_expression(nm, "lambda (bool x) => { if(x) { return 1; } else { return -5; } }"));

		EXPECT_TRUE(test_expression(nm, "type_lit(int<4>)"));

		EXPECT_TRUE(test_expression(nm, "let f = lambda ()->unit {  5;  lambda ()->unit { f(); } (); }; f"));

		EXPECT_TRUE(test_expression(nm, "lambda (int<4> v, int<4> exp) -> int<4> { "
		                                "	let one = lambda(int<4> _)=>4; "
		                                "	let two = lambda(int<4> x)=>x+exp; "
		                                "    return one(two(exp));"
		                                "}  "));
		EXPECT_TRUE(test_expression(nm, "lambda (int<4> v, int<4> exp) -> int<4> { "
		                                "	let one = lambda(int<4> _)-> int<4> { return 4;  };"
		                                "	let two = lambda(int<4> x)-> int<4> { return x+5; };"
		                                "    return one(two(exp));"
		                                "}  "));

		EXPECT_TRUE(test_expression(nm, "    let class = struct name { int<4> a; int<5> b};"
		                                "    lambda ctor class::() { }"));

		EXPECT_TRUE(test_expression(nm, "    let class = struct name { int<4> a; int<5> b};"
		                                "    lambda class::()->int<4> { return 1; }"));

		EXPECT_TRUE(test_expression(nm, "    let class = struct name { int<4> a; int<5> b};"
		                                "    lambda  ~class::() { }"));
	}

	TEST(IR_Parser, Precedence) {
		NodeManager mgr;
		EXPECT_EQ("int_add(1, int_mul(2, 3))", toString(*parseExpr(mgr, "1+2*3")));
		EXPECT_EQ("int_mul(int_add(1, 2), 3)", toString(*parseExpr(mgr, "(1+2)*3")));
		EXPECT_EQ("int_add(1, int_mul(2, 3))", toString(*parseExpr(mgr, "1+(2*3)")));
	}

	TEST(IR_Parser, LambdasAndFunctions) {
		NodeManager mgr;
		IRBuilder builder(mgr);

		auto funA = builder.normalize(parseExpr(mgr, "lambda (int<4> x) -> int<4> { return x; }"));
		auto funB = builder.normalize(parseExpr(mgr, "function (ref<int<4>,f,f,plain> x) -> int<4> { return *x; }"));

		auto funC = builder.normalize(parseExpr(mgr, "let f = lambda (int<4> x) -> int<4> { return x; }; f"));
		auto funD = builder.normalize(parseExpr(mgr, "let f = function (ref<int<4>,f,f,plain> y) -> int<4> { return *y; }; f"));

		EXPECT_EQ(funA, funB);
		EXPECT_EQ(funB, funC);
		EXPECT_EQ(funC, funD);
	}

	bool test_statement(NodeManager& nm, const std::string& x) {
		InspireDriver driver(x, nm);
		driver.parseStmt();
		if(driver.result) {
//			dumpColor(driver.result);
			auto msg = checks::check(driver.result);
			EXPECT_TRUE(msg.empty()) << msg;
		} else {
			driver.printErrors();
		}
//		std::cout << " ============== TEST ============ " << std::endl;
		return driver.result;
	}

	TEST(IR_Parser, Statements) {
		NodeManager nm;
		EXPECT_TRUE(test_statement(nm, ";"));

		EXPECT_TRUE(test_statement(nm, "1;"));
		EXPECT_TRUE(test_statement(nm, "1u;"));

		EXPECT_TRUE(test_statement(nm, "1.0f;"));
		EXPECT_TRUE(test_statement(nm, "1.0;"));

		EXPECT_TRUE(test_statement(nm, "1 + 3;"));
		EXPECT_TRUE(test_statement(nm, "1 - 0;"));
		EXPECT_TRUE(test_statement(nm, "1 * 0;"));
		EXPECT_TRUE(test_statement(nm, "1 / 0;"));
		EXPECT_TRUE(test_statement(nm, "1 | 0;"));
		EXPECT_TRUE(test_statement(nm, "1 & 0;"));
		EXPECT_TRUE(test_statement(nm, "1 ^ 0;"));

		EXPECT_TRUE(test_statement(nm, "(1.0);"));
		EXPECT_TRUE(test_statement(nm, "((1.0));"));

		EXPECT_FALSE(test_statement(nm, "x"));
		EXPECT_FALSE(test_statement(nm, "x;"));

		EXPECT_TRUE(test_statement(nm, "{ decl int<4> x = 0; x+1; }"));
		EXPECT_TRUE(test_statement(nm, "{ decl auto x = 0; x+1; }"));

		EXPECT_TRUE(test_statement(nm, "if ( true ) {}"));
		EXPECT_TRUE(test_statement(nm, "if ( true ) {} else {}"));
		EXPECT_TRUE(test_statement(nm, "if ( true ) if ( false ) { } else 1 ;"));
		EXPECT_TRUE(test_statement(nm, "if ( true ) if ( false ) { } else 1 ; else 2; "));
		EXPECT_TRUE(test_statement(nm, "if( false ) { return 0; } else { return 1+2; }"));
		EXPECT_TRUE(test_statement(nm, "if( false ) { return 0; }"));
		EXPECT_TRUE(test_statement(nm, "if(1 != 0) { return 0; }"));
		EXPECT_TRUE(test_statement(nm, "while ( true ) 1+1;"));
		EXPECT_TRUE(test_statement(nm, "while ( false ) 1+1;"));
		EXPECT_TRUE(test_statement(nm, "while ( false || true ) { 1+1; }"));
		EXPECT_TRUE(test_statement(nm, "for ( int<4> it = 1 .. 3) 1+1;"));
		EXPECT_TRUE(test_statement(nm, "for ( int<4> it = 1 .. 3: 2) 1+1;"));
		EXPECT_TRUE(test_statement(nm, "for ( int<4> it = 1 .. 3: 2) { 1+1; }"));

		EXPECT_TRUE(test_statement(nm, "switch (2) { case 1: 1; case 2: 2; }"));
		EXPECT_FALSE(test_statement(nm, "switch (2) { case 1: 1; case 2: 2; default 2;}"));
		EXPECT_FALSE(test_statement(nm, "switch (2) { case 1+1: 1; case 2: 2; default: 2;}"));

		EXPECT_FALSE(test_statement(nm, "try  2; catch( int<4> e) { 1+1; }"));
		EXPECT_FALSE(test_statement(nm, "try  {2;} catch( int<4> e)  1+1; "));

		EXPECT_TRUE(test_statement(nm, "try  {2;} catch( int<4> e) { 1+1; }"));
		EXPECT_TRUE(test_statement(nm, "try  {2;} catch( int<4> e) { 1+1; } catch (rf<int<4>> r) { 3+4; }"));

		EXPECT_TRUE(test_statement(nm, "{ }"));


		EXPECT_TRUE(test_statement(nm, "{"
		                               "    let type = struct a { int<4> a; int<8> b; };"
		                               "    decl type varable;"
		                               "    decl rf<type> var2;"
		                               "    decl auto var3 = undefined(type);"
		                               "}"));

		EXPECT_TRUE(test_statement(nm, "{"
		                               "    let class = struct name { int<2> a};"
		                               "    let collection = array<class, 10>;"
		                               "    decl ref<collection,f,f,plain> x;"
		                               "    decl int<2> y;"
		                               "    x[5].a = y;"
		                               "}"));
	}


	TEST(IR_Parser, Tuples) {
		NodeManager nm;

		EXPECT_TRUE(test_statement(nm,
				"{ "
				"	let int = int<4>;"
				"	let f = expr lit(\"f\" : (int,(int),(int,int),(int,int,int)) -> unit); "
				"	f(1,(1),(1,2),(1,2,3));"
				"}"));

		EXPECT_TRUE(test_statement(nm,
				"{ "
				"	let int = int<4>;"
				"	let f = expr lit(\"f\" : (int,(int),(int,int),(int,int,int)) -> unit); "
				"	f(1+2,(1+2),(1,2),(1,2,3+3));"
				"}"));

	}

	bool test_program(NodeManager& nm, const std::string& x) {

		InspireDriver driver(x, nm);
	   	std::cout << " ============== TEST ============ " << std::endl;

		driver.parseProgram();
		std::cout << " ============= PARSED =========== " << std::endl;

		if(driver.result) {
//			dumpColor(driver.result);
			auto msg = checks::check(driver.result);
			EXPECT_TRUE(msg.empty()) << msg;
		} else {
			driver.printErrors();
		}
//		   std::cout << " ============== TEST ============ " << std::endl;
		return driver.result;
	}

	TEST(IR_Parser, Let) {
		NodeManager mgr;
		EXPECT_TRUE(test_program(mgr, "let int = int<4>; int main () { return 1; }"));
		EXPECT_TRUE(test_program(mgr, "let int = int<4>; let long = int<8>; long main (ref<int,f,f,plain> a) { return 1; }"));
		EXPECT_TRUE(test_program(mgr, "let int , long = int<4> ,int<8>; int<4> main () { return 1; }"));
		EXPECT_TRUE(test_program(mgr, "let f = lambda () -> unit { }; int<4> main () { f(); return 1; }"));
		EXPECT_TRUE(test_program(mgr, "let int = int<4>; let f = lambda (int a) -> int { return a; }; int<4> main () { f(1); return 1; }"));


		EXPECT_FALSE(test_program(mgr, "let int , long = int<4>; int<4> main () { return 1; }"));
		EXPECT_FALSE(test_program(mgr, "let a , b = a<4>; int<4> main () { return 1; }"));

		EXPECT_TRUE(test_program(mgr, "let a , b = struct { a<b> x; }, struct { b<a> x; } ; int<4> main () { decl a x = undefined(a); decl b y = undefined(b); return 1; }"));
		EXPECT_TRUE(test_program(mgr, "let f,g = lambda()->unit{g();},lambda()->unit{f();}; unit main() { f(); }"));

		EXPECT_TRUE(test_program(mgr, "let class = struct name { int<4> a; int<5> b};"
		                              "let f,g = lambda class :: ()->unit{"
		                              "        g(this);"
		                              "    },"
		                              "    lambda class ::()->unit{"
		                              "        f(this);"
		                              "    }; "
		                              "unit main() {  "
		                              "    decl ref<class,f,f,plain> x;"
		                              "    f(x);"
		                              "    g(x);"
		                              "}"));
	}

	TEST(IR_Parser, Program) {
		NodeManager nm;
		EXPECT_TRUE(test_program(nm, "int<4> main (ref<int<4>,f,f,plain> a, ref<int<4>,f,f,plain> b)  { return 1+1; }"));
		EXPECT_TRUE(test_program(nm, "let int = int<4>; int main (ref<int,f,f,plain> a, ref<int,f,f,plain> b) { return 1+1; }"));
		EXPECT_TRUE(test_program(nm, "let int = int<4>; let f = lambda (int a) ->int { return a; }; int main (ref<int,f,f,plain> a, ref<int,f,f,plain> b) { return f(1); }"));
		EXPECT_TRUE(test_program(nm, "let int = int<4> ; "
		                             "let h = lambda ((int)->int f)->int { return f(5); } ; "
		                             "let f,g = "
		                             "    lambda (int a)->int {"
		                             "        h(f);"
		                             "        f(4);"
		                             "        g(f(4));"
		                             "        return h(g);"
		                             "    },"
		                             "    lambda (int a)->int {"
		                             "        h(f);"
		                             "        f(g(4));"
		                             "        g(4);"
		                             "        return h(g);"
		                             "    };"
		                             "unit main() { f(1); }"));

		EXPECT_TRUE(test_program(nm, "let int = int<4>;"
		                             "let uint = uint<4>;"
		                             "let parallel = expr lit(\"parallel\" : (job) -> threadgroup);"
		                             "let differentbla = lambda ('b x) -> unit {"
		                             "    decl auto m = x;"
		                             "    decl auto l = m;"
		                             "};"
		                             "let bla = lambda ('a f) -> unit {"
		                             "    let anotherbla = lambda ('a x) -> unit {"
		                             "        decl auto m = x;"
		                             "    };"
		                             "    anotherbla(f);"
		                             "    differentbla(f);"
		                             "    parallel(job { decl auto l = f; });"
		                             "};"
		                             "int main() {"
		                             "    decl int x = 10;"
		                             "    bla(x);"
		                             "    return 0;"
		                             "}"));
	}


	TEST(IR_Parser, Scopes) {
		NodeManager nm;
		//        EXPECT_FALSE (test_expression(nm, R"(
		//            let a = lambda()->unit {};  let a = lambda ()->int<4>{ return 0; };  a
		//        )"));
		//        EXPECT_TRUE (test_expression(nm, R"(
		//            let a = lambda()->unit {};  let b = ()->int{ return 0; };  a
		//        )"));
		//
		EXPECT_FALSE(test_expression(nm, " let a = int<4>;  let a = float<4>;  1 "));
	}

} // parser
} // core
} // insieme
