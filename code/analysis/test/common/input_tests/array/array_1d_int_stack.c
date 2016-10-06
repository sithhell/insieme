/**
 * Copyright (c) 2002-2016 Distributed and Parallel Systems Group,
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

/**
 * A simple test case covering some arithmetic.
 */

#include "../../input_tests/cba.h"

int main(int argc, char** argv) {

	// create an array
	int a[10];

	// check that elements of a are not defined at this point
//	cba_expect_undefined_int(a[0]);
//	cba_expect_undefined_int(a[1]);
//	cba_expect_undefined_int(a[2]);
//	cba_expect_undefined_int(a[4]);
//	cba_expect_undefined_int(a[argc]);

	// fix one value
	a[0] = 12;
//	cba_expect_eq_int(a[0],12);
//	cba_expect_undefined_int(a[1]);
//	cba_expect_undefined_int(a[2]);
//	cba_expect_undefined_int(a[4]);
//	cba_expect_undefined_int(a[argc]);

	// fix some values
	a[1] = 14;
	a[4] = 16;

	// check those values
	cba_expect_eq_int(a[0],12);
//	cba_expect_eq_int(a[1],14);
//	cba_expect_undefined_int(a[2]);
//	cba_expect_eq_int(a[4],16);
//	cba_expect_undefined_int(a[argc]);

//	cba_print_code();
//	cba_dump_solution();
//	cba_dump_json();

	return 0;
}