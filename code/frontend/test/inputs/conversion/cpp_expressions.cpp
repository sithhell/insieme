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


int main() {
//===-------------------------------------------------------------------------------------------------------------------------------- UNARY OPERATORS ---===

#pragma test expect_ir("int_not(3)")
	~3;

	#pragma test expect_ir("!(3!=0)")
	!3;

	#pragma test expect_ir("3")
	+3;

	#pragma test expect_ir("-3")
	-3;

	#pragma test expect_ir("{ var ref<int<4>,f,f> v1 = ref_var_init(0); ptr_from_ref(v1); }")
	{
		int x = 0;
		&x;
	}

	// In c++, default is to return lvalues, once used, they might get derefed (LtoRcast)
	#pragma test expect_ir("{ var ref<ptr<int<4>,f,f>,f,f> v0; ptr_to_ref(*v0); }")
	{
		int* x;
		*x;
	}

	#pragma test expect_ir("{ var ref<int<4>,f,f> v1 = ref_var_init(0); 0-v1; }")
	{
		int x = 0;
		-x;
	}

	#pragma test expect_ir("{ var ref<int<4>,f,f> v1 = ref_var_init(0); gen_pre_inc(v1); }")
	{
		int v = 0;
		++v;
	}

	#pragma test expect_ir("{ var ref<uint<2>,f,f> v1 = ref_var_init(num_cast(0, type_lit(uint<2>))); gen_post_inc(v1); }")
	{
		unsigned short v = 0;
		v++;
	}

	#pragma test expect_ir("{ var ref<char,f,f> v1 = ref_var_init(num_cast(0, type_lit(char))); gen_pre_dec(v1); }")
	{
		char v = 0;
		--v;
	}

	#pragma test expect_ir("{ var ref<int<1>,f,f> v1 = ref_var_init(num_cast(0, type_lit(int<1>))); gen_post_dec(v1); }")
	{
		signed char v = 0;
		v--;
	}

	//===------------------------------------------------------------------------------------------------------------------------------- BINARY OPERATORS ---===

	// COMMA OPERATOR //////////////////////////////////////////////////////////////

	#pragma test expect_ir("{ comma_operator(() -> int<4> { return 2; }, () -> int<4> { return 3; }); }")
	{ 2, 3; }
	#pragma test expect_ir("EXPR_TYPE", "int<4>")
	2, 3;

	#pragma test expect_ir(                                                                                                                                    \
	    "{ comma_operator(() -> int<4> { return comma_operator(() -> int<4> { return 2; }, () -> int<4> { return 3; }); }, () -> int<4> { return 4; }); }")
	{ 2, 3, 4; }

	#pragma test expect_ir("{ comma_operator(() -> int<4> { return 2; }, () -> real<8> { return lit(\"3.0E+0\":real<8>); }); }")
	{ 2, 3.0; }
	#pragma test expect_ir("EXPR_TYPE", "real<8>")
	2, 3.0;

	// MATH //////////////////////////////////////////////////////////////

	#pragma test expect_ir("int_add(1, 2)")
	1 + 2;

	#pragma test expect_ir("int_sub(3, 4)")
	3 - 4;

	#pragma test expect_ir("int_mul(5, 6)")
	5 * 6;

	#pragma test expect_ir("int_div(7, 8)")
	7 / 8;

	#pragma test expect_ir("int_mod(9, 10)")
	9 % 10;

	// BITS //////////////////////////////////////////////////////////////

	#pragma test expect_ir("int_lshift(11, 12)")
	11 << 12;

	#pragma test expect_ir("int_rshift(13, 14)")
	13 >> 14;

	#pragma test expect_ir("int_and(15, 16)")
	15 & 16;

	#pragma test expect_ir("int_xor(17, 18)")
	17 ^ 18;

	#pragma test expect_ir("int_or(19, 20)")
	19 | 20;

	// LOGICAL ////////////////////////////////////////////////////////////

	#pragma test expect_ir("(0!=0) || (1!=0)")
	0 || 1;

	#pragma test expect_ir("(1!=0) && (0!=0)")
	1 && 0;

	// COMPARISON /////////////////////////////////////////////////////////

	#pragma test expect_ir("int_eq(1, 2)")
	1 == 2;

	#pragma test expect_ir("int_ne(1, 2)")
	1 != 2;

	#pragma test expect_ir("real_ne(lit(\"1.0E+0\":real<8>), lit(\"2.0E+0\":real<8>))")
	1.0 != 2.0;

	#pragma test expect_ir("int_lt(1, 2)")
	1 < 2;

	#pragma test expect_ir("int_gt(1, 2)")
	1 > 2;

	#pragma test expect_ir("int_le(1, 2)")
	1 <= 2;

	#pragma test expect_ir("int_ge(1, 2)")
	1 >= 2;


	// WITH DIFFERENT TYPES ///////////////////////////////////////////////////

	// this should work: num_cast(1, type_lit(real<8>))+2.0E+0
	#pragma test expect_ir("num_cast(1, type_lit(real<8>))+lit(\"2.0E+0\":real<8>)")
	1 + 2.0;

	#pragma test expect_ir("lit(\"3.0E+0\":real<8>)-num_cast(4, type_lit(real<8>))")
	3.0 - 4;

	#pragma test expect_ir("lit(\"5.0E+0\":real<4>)*num_cast(6, type_lit(real<4>))")
	5.0f * 6;

	#pragma test expect_ir("7u/num_cast(8, type_lit(uint<4>))")
	7u / 8;

	#pragma test expect_ir("9u+num_cast(10, type_lit(uint<4>))")
	9u + 10;

	// POINTER & ARRAYS ///////////////////////////////////////////////////////

	// one dimension

	#pragma test expect_ir("{ var ref<array<int<4>,5>,f,f> v0; ptr_subscript(ptr_from_array(v0), 1); }")
	{
		int a[5];
		a[1];
	}

	#pragma test expect_ir("{ var ref<array<int<4>,5>,f,f> v0; ptr_subscript(ptr_from_array(v0), -1); }")
	{
		int a[5];
		a[-1];
	}

	#pragma test expect_ir("{ var ref<array<int<4>,5>,f,f> v0; ptr_subscript(ptr_from_array(v0), 1); }")
	{
		int a[5];
		1 [a];
	}

	#pragma test expect_ir("{ var ref<array<int<4>,1>,f,f> v0; ptr_to_ref(ptr_from_array(v0)); }")
	{
		int a[1];
		*a;
	}

	#pragma test expect_ir("{ var ref<ptr<int<4>,f,f>,f,f> v0; ptr_from_ref(v0); }")
	{
		int* a;
		&a;
	}

	#pragma test expect_ir("{ var ref<array<int<4>,5>,f,f> v0; ptr_from_ref(v0); }")
	{
		int a[5];
		&a;
	}

	// No ptr arithmetics with void* in C++
	//	{
	//		void* a;
	//		a+5;
	//	}
	//
	//	{
	//		void* a;
	//		5+a;
	//	}
	//
	//	{
	//		void* a;
	//		a++;
	//		a--;
	//		++a;
	//		--a;
	//	}
	//
	//	{
	//		void* a;
	//		a-5;
	//	}

	// No ptr difference with void* in C++
	//	{
	//		void *a, *b;
	//		a-b;
	//	}

	#pragma test expect_ir("{ var ref<ptr<unit,f,f>,f,f> v0; ptr_gt(*v0,*v0); ptr_lt(*v0,*v0); ptr_le(*v0,*v0); ptr_ge(*v0,*v0); }")
	{
		void* a;
		a > a;
		a < a;
		a <= a;
		a >= a;
	}

	// multidimensional

	#pragma test expect_ir("{ var ref<array<array<int<4>,3>,2>,f,f> v0; ptr_subscript(ptr_from_array(ptr_subscript(ptr_from_array(v0), 1)), 2); }")
	{
		int a[2][3];
		a[1][2];
	}

	// note: there are no rvalue arrays in C!
	#pragma test expect_ir("{ var ref<array<array<int<4>,3>,2>,f,f> v0; ptr_subscript(ptr_from_array(v0), 1); }")
	{
		int a[2][3];
		a[1];
	}

	// COMPOUND //////////////////////////////////////////////////////////////

	#pragma test expect_ir("{ var ref<int<4>,f,f> v1 = ref_var_init(1); cxx_style_assignment(v1, *v1+1); }")
	{
		int a = 1;
		a += 1;
	}

	#pragma test expect_ir("{ var ref<int<4>,f,f> v1 = ref_var_init(1); cxx_style_assignment(v1, *v1-2); }")
	{
		int a = 1;
		a -= 2;
	}

	#pragma test expect_ir("{ var ref<int<4>,f,f> v1 = ref_var_init(1); cxx_style_assignment(v1, *v1/1); }")
	{
		int a = 1;
		a /= 1;
	}

	#pragma test expect_ir("{ var ref<int<4>,f,f> v1 = ref_var_init(1); cxx_style_assignment(v1, *v1*5); }")
	{
		int a = 1;
		a *= 5;
	}

	#pragma test expect_ir("{ var ref<int<4>,f,f> v1 = ref_var_init(1); cxx_style_assignment(v1, *v1%5); }")
	{
		int a = 1;
		a %= 5;
	}

	#pragma test expect_ir("{ var ref<int<4>,f,f> v1 = ref_var_init(1); cxx_style_assignment(v1, *v1&5); }")
	{
		int a = 1;
		a &= 5;
	}

	#pragma test expect_ir("{ var ref<int<4>,f,f> v1 = ref_var_init(1); cxx_style_assignment(v1, *v1|5); }")
	{
		int a = 1;
		a |= 5;
	}

	#pragma test expect_ir("{ var ref<int<4>,f,f> v1 = ref_var_init(1); cxx_style_assignment(v1, *v1 ^ 5); }")
	{
		int a = 1;
		a ^= 5;
	}

	#pragma test expect_ir("{ var ref<int<4>,f,f> v1 = ref_var_init(1); cxx_style_assignment(v1, int_lshift(*v1, 5)); }")
	{
		int a = 1;
		a <<= 5;
	}

	#pragma test expect_ir("{ var ref<int<4>,f,f> v1 = ref_var_init(1); cxx_style_assignment(v1, int_rshift(*v1, 5)); }")
	{
		int a = 1;
		a >>= 5;
	}

	// ASSIGNMENT //////////////////////////////////////////////////////////////

	#pragma test expect_ir("{ var ref<int<4>,f,f> v1; cxx_style_assignment(v1, 5); }")
	{
		int a;
		a = 5;
	}

	#pragma test expect_ir("{ var ref<int<4>,f,f,plain> v0; var ref<int<4>,f,f,plain> v1; cxx_style_assignment(v0, *cxx_style_assignment(v1, 1)); }")
	{
		int a, b;
		a = b = 1;
	}

	//===------------------------------------------------------------------------------------------------------------------------------- TERNARY OPERATOR ---===

	#pragma test expect_ir("(1!=0)?2:3")
	1 ? 2 : 3;

	//===---------------------------------------------------------------------------------------------------------------------------------- MISCELLANEOUS ---===

	#pragma test expect_ir("{ var ref<uint<8>,f,f,plain> v0 = ref_var_init(sizeof(type_lit(real<8>))); cxx_style_assignment(v0, sizeof(type_lit(uint<8>))); }")
	{
		unsigned long i = sizeof(double);
		i = sizeof(i);
	}

	// FIXME: unsuported
	// #pr agma test expect_ir("{ var ref<uint<8>,f,f,plain> v0 = ref_var_init(sizeof(type_lit(real<8>))); cxx_style_assignment(v0, sizeof(type_lit(uint<8>)));
	// }")
	// {
	//     unsigned long i = alignof(double);
	//     i = alignof(i);
	// }

	#pragma test expect_ir("{ var ref<array<char,8>,f,f> v0; var ref<uint<8>,f,f,plain> v1 = ref_var_init(sizeof(type_lit(array<char,8>))); }")
	{
		char char_arr[8];
		unsigned long i = sizeof(char_arr);
	}
}