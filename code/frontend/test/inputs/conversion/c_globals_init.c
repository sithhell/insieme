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

int initedGlobal = 5;

typedef struct S {
	int x;
	unsigned y;
} S;

S y = { 1, 5u };

const S klaus_test[] = {{1,2u},{3,4u},{5,6u}};

#pragma test expect_ir(R"INSPIRE(
def struct IMP_S { x: int<4>; y: uint<4>; };
def IMP_main = ()->int<4> {
	lit("initedGlobal":ref<int<4>>) = 5;
	lit("y":ref<IMP_S>) = <IMP_S>{1, 5u};
	ref_cast(lit("klaus_test":ref<array<IMP_S,3>,t,f>), type_lit(f), type_lit(f), type_lit(plain)) =
		array_create(type_lit(3), [<IMP_S>{1,2u},<IMP_S>{3,4u},<IMP_S>{5,6u}]);
	*lit("initedGlobal":ref<int<4>>);
	*lit("y":ref<IMP_S>);
	ptr_from_array(lit("klaus_test":ref<array<IMP_S,3>,t,f>));
	return 0;
};
IMP_main
)INSPIRE")
int main() {
	initedGlobal;
	y;
	klaus_test;
	return 0;
}
