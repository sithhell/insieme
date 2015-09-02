
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
	
	#pragma test expect_ir("{ decl ref<int<4>,f,f> v1 = var(0); ptr_from_ref(v1); }")
	{
		int x = 0;
		&x;
	}

	#pragma test expect_ir("{ decl ref<ptr<int<4>,f,f>,f,f> v0; *ptr_to_ref(*v0); }")
	{
		int* x;
		*x;
	}
	
	#pragma test expect_ir("{ decl ref<int<4>,f,f> v1 = var(0); 0-v1; }")
	{
		int x = 0;
		-x;
	}

	#pragma test expect_ir("{ decl ref<int<4>,f,f> v1 = var(0); int_pre_inc(v1); }")
	{
		int v = 0;
		++v;
	}

	#pragma test expect_ir("{ decl ref<uint<2>,f,f> v1 = var(type_cast(0, type(uint<2>))); uint_post_inc(v1); }")
	{
		unsigned short v = 0;
		v++;
	}

	#pragma test expect_ir("{ decl ref<char,f,f> v1 = var(type_cast(0, type(char))); char_pre_dec(v1); }")
	{
		char v = 0;
		--v;
	}

	#pragma test expect_ir("{ decl ref<int<1>,f,f> v1 = var(type_cast(0, type(int<1>))); int_post_dec(v1); }")
	{
		signed char v = 0;
		v--;
	}

	//===------------------------------------------------------------------------------------------------------------------------------- BINARY OPERATORS ---===
	
	// COMMA OPERATOR //////////////////////////////////////////////////////////////

	#pragma test expect_ir("lambda () -> int<4> { 2; return 3; }()")
	2, 3;

	#pragma test expect_ir("lambda () -> int<4> { lambda () -> int<4> { 2; return 3; }(); return 4; }()")
	2, 3, 4;
	
	#pragma test expect_ir("lambda () -> real<8> { 2; return lit(\"3.0E+0\":real<8>); }()")
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
	
	#pragma test expect_ir("((0!=0) || (1!=0))")
	0 || 1;

	#pragma test expect_ir("((1!=0) && (0!=0))")
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

	// POINTER & ARRAYS ///////////////////////////////////////////////////////

	// one dimension

	#pragma test expect_ir("{ decl ref<array<int<4>,5>,f,f> v0; ref_deref(ptr_subscript(ptr_from_array(v0), 1)); }")
	{
		int a[5];
		a[1];
	}

	#pragma test expect_ir("{ decl ref<array<int<4>,5>,f,f> v0; ref_deref(ptr_subscript(ptr_from_array(v0), -1)); }")
	{
		int a[5];
		a[-1];
	}

	#pragma test expect_ir("{ decl ref<array<int<4>,5>,f,f> v0; ref_deref(ptr_subscript(ptr_from_array(v0), 1)); }")
	{
		int a[5];
		1[a];
	}
	
	#pragma test expect_ir("{ decl ref<array<int<4>,1>,f,f> v0; ref_deref(ptr_to_ref(ptr_from_array(v0))); }")
	{
		int a[1];
		*a;
	}

	#pragma test expect_ir("{ decl ref<ptr<int<4>,f,f>,f,f> v0; ptr_from_ref(v0); }")
	{
		int* a;
		&a;
	}
	
	#pragma test expect_ir("{ decl ref<array<int<4>,5>,f,f> v0; ptr_from_ref(v0); }")
	{
		int a[5];
		&a;
	}

	// multidimensional
	
	#pragma test expect_ir("{ decl ref<array<array<int<4>,3>,2>,f,f> v0; ref_deref(ptr_subscript(ptr_from_array(ptr_subscript(ptr_from_array(v0), 1)), 2)); }")
	{
		int a[2][3];
		a[1][2];
	}
	
	// note: there are no rvalue arrays in C!
	#pragma test expect_ir("{ decl ref<array<array<int<4>,3>,2>,f,f> v0; ptr_from_array(ptr_subscript(ptr_from_array(v0), 1)); }")
	{
		int a[2][3];
		a[1];
	}

	// COMPOUND //////////////////////////////////////////////////////////////

	// TODO FE NG new call semantic
	#define C_STYLE_ASSIGN "let c_ass = lambda (ref<'a,f,'b> v1, 'a v2) -> 'a { v1 = v2; return *v1; };"

	#pragma test expect_ir("{", C_STYLE_ASSIGN, "decl ref<int<4>,f,f> v1 = var(1); c_ass(v1, (( *v1)+1)); }")
	{
		int a = 1;
		a += 1;
	}
	
	#pragma test expect_ir("{", C_STYLE_ASSIGN, "decl ref<int<4>,f,f> v1 = var(1); c_ass(v1, (( *v1)-2)); }")
	{
		int a = 1;
		a -= 2;
	}
	
	#pragma test expect_ir("{", C_STYLE_ASSIGN, "decl ref<int<4>,f,f> v1 = var(1); c_ass(v1, (( *v1)/1)); }")
	{
		int a = 1;
		a /= 1;
	}
	
	#pragma test expect_ir("{", C_STYLE_ASSIGN, "decl ref<int<4>,f,f> v1 = var(1); c_ass(v1, (( *v1)*5)); }")
	{
		int a = 1;
		a *= 5;
	}

	#pragma test expect_ir("{", C_STYLE_ASSIGN, "decl ref<int<4>,f,f> v1 = var(1); c_ass(v1, (( *v1)%5)); }")
	{
		int a = 1;
		a %= 5;
	}

	#pragma test expect_ir("{", C_STYLE_ASSIGN, "decl ref<int<4>,f,f> v1 = var(1); c_ass(v1, (( *v1)&5)); }")
	{
		int a = 1;
		a &= 5;
	}

	#pragma test expect_ir("{", C_STYLE_ASSIGN, "decl ref<int<4>,f,f> v1 = var(1); c_ass(v1, (( *v1)|5)); }")
	{
		int a = 1;
		a |= 5;
	}

	#pragma test expect_ir("{", C_STYLE_ASSIGN, "decl ref<int<4>,f,f> v1 = var(1); c_ass(v1, (( *v1)^5)); }")
	{
		int a = 1;
		a ^= 5;
	}

	#pragma test expect_ir("{", C_STYLE_ASSIGN, "decl ref<int<4>,f,f> v1 = var(1); c_ass(v1, int_lshift(( *v1), 5)); }")
	{
		int a = 1;
		a <<= 5;
	}

	#pragma test expect_ir("{", C_STYLE_ASSIGN, "decl ref<int<4>,f,f> v1 = var(1); c_ass(v1, int_rshift(( *v1), 5)); }")
	{
		int a = 1;
		a >>= 5;
	}

	// ASSIGNMENT //////////////////////////////////////////////////////////////

	#pragma test expect_ir("{", C_STYLE_ASSIGN, "decl ref<int<4>,f,f> v1; c_ass(v1, 5); }")
	{
		int a;
		a = 5;
	}
	
	#pragma test expect_ir("{", C_STYLE_ASSIGN, "decl ref<int<4>,f,f> v0; decl ref<int<4>,f,f> v1; c_ass(v0, c_ass(v1, 1)); }")
	{
		int a, b;
		a = b = 1;
	}
	
	//===------------------------------------------------------------------------------------------------------------------------------- TERNARY OPERATOR ---===

	#pragma test expect_ir("(1!=0)?2:3")
	1?2:3;
	
	//===---------------------------------------------------------------------------------------------------------------------------------- MISCELLANEOUS ---===
	
	#pragma test expect_ir("sizeof(type(real<8>))")
	sizeof(double);

	#pragma test expect_ir("sizeof(type(char))")
	sizeof(char);
	
	#pragma test expect_ir("{ decl ref<int<4>,f,f> v0; sizeof(type(int<4>)); }")
	{
		int sizeof_int;
		sizeof(sizeof_int);
	}

	#pragma test expect_ir("{ decl ref<array<char,8>,f,f> v0; sizeof(type(array<char,8>)); }")
	{
		char char_arr[8];
		sizeof(char_arr);
	}

}