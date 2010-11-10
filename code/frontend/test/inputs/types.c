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

// builtin types
void basic_type_test() {
	#pragma test "ref<int<4>> v1 = ref.var(1)"
	int a = 1;

	#pragma test "ref<int<8>> v1 = ref.var(0)"
	long b;

	#pragma test "ref<int<2>> v1 = ref.var(cast<int<2>>(0xFFFF))"
	short c = 0xFFFF;

	#pragma test "ref<char> v1 = ref.var(cast<char>('a'))"
	char d = 'a';

//	#pragma test "ref<array<ref<'a>,1>> v4 = ref.var(null)"
	void* e;

	#pragma test "ref<real<4>> v1 = ref.var(0.00f)"
	float f = 0.00f;

	#pragma test "ref<real<8>> v1 = ref.var(0.0)"
	double g;
	
	#pragma test "ref<vector<ref<real<4>>,3>> v1 = ref.var(vector.initUniform(ref.var(0.0)))"
	float v[3];

    #pragma test "ref<vector<vector<ref<int<4>>,2>,3>> v1 = ref.var(vector.initUniform(vector.initUniform(ref.var(0))))"
	int vv[3][2];
}

// Simple struct
#pragma test "struct<name:array<ref<char>,1>,age:int<4>>"
struct Person {
	// cppcheck-suppress unusedStructMember
	char* name;
	// cppcheck-suppress unusedStructMember
	int age;
};

void test_func() {
	#pragma test "ref<struct<name:array<ref<char>,1>,age:int<4>>> v1 = ref.var(struct{name=null, age=0})" // FIXME: members should be ref
	struct Person p;

	// #pragma test "ref<struct<name:array<ref<char>,1>,age:int<4>>> v1 = ref.var(struct{name=null, age=0})" // FIXME: members should be ref
	struct Person p2 = {"Simone", 28};
}

// Self recursive struct
#pragma test "rec 'PersonList.{'PersonList=struct<name:array<ref<char>,1>,age:int<4>,next:array<ref<'PersonList>,1>>}"
struct PersonList {
	// cppcheck-suppress unusedStructMember
	char* name;
	// cppcheck-suppress unusedStructMember
	int age;
	// cppcheck-suppress unusedStructMember
	struct PersonList* next;
};

// Mutual recursive struct
struct A;
struct B;
struct C;

#pragma test "rec 'A.{'A=struct<b:array<ref<'B>,1>,c:array<ref<'C>,1>>, 'B=struct<b:array<ref<'C>,1>>, 'C=struct<a:array<ref<'A>,1>,b:array<ref<'B>,1>>}"
struct A {
	// cppcheck-suppress unusedStructMember
	struct B* b;
	// cppcheck-suppress unusedStructMember
	struct C* c;
};

#pragma test "rec 'B.{'A=struct<b:array<ref<'B>,1>,c:array<ref<'C>,1>>, 'B=struct<b:array<ref<'C>,1>>, 'C=struct<a:array<ref<'A>,1>,b:array<ref<'B>,1>>}"
struct B {
	// cppcheck-suppress unusedStructMember
	struct C* b;
};

#pragma test "rec 'C.{'A=struct<b:array<ref<'B>,1>,c:array<ref<'C>,1>>, 'B=struct<b:array<ref<'C>,1>>, 'C=struct<a:array<ref<'A>,1>,b:array<ref<'B>,1>>}"
struct C {
	// cppcheck-suppress unusedStructMember
	struct A* a;
	// cppcheck-suppress unusedStructMember
	struct B* b;
};

// A tricky mutual struct example
struct A1;
struct B1;
struct C1;
struct D1;

#pragma test "struct<b:array<ref<rec 'B1.{'C1=struct<b:array<ref<'B1>,1>,d:array<ref<struct<val:int<4>>>,1>>, 'B1=struct<b:array<ref<'C1>,1>>}>,1>>"
struct A1 {
	// cppcheck-suppress unusedStructMember
	struct B1* b;
};

#pragma test "rec 'B1.{'C1=struct<b:array<ref<'B1>,1>,d:array<ref<struct<val:int<4>>>,1>>, 'B1=struct<b:array<ref<'C1>,1>>}"
struct B1 {
	// cppcheck-suppress unusedStructMember
	struct C1* b;
};

#pragma test "rec 'C1.{'C1=struct<b:array<ref<'B1>,1>,d:array<ref<struct<val:int<4>>>,1>>, 'B1=struct<b:array<ref<'C1>,1>>}"
struct C1 {
	// cppcheck-suppress unusedStructMember
	struct B1* b;
	// cppcheck-suppress unusedStructMember
	struct D1* d;
};

#pragma test "struct<val:int<4>>"
struct D1 {
	// cppcheck-suppress unusedStructMember
	int val;
};



