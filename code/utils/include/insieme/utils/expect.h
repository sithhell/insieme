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

#pragma once

/**
 * This header file defines a set of macros to define more readable and flexible expectations within
 * program code. Also, macros supporting the declaration of variables only required for checking
 * expectations are supported. Just like assertions, in case the macro NDEBUG is defined, they will be
 * ignored. In those cases, variables declared using the 'expect_decl' macro will not be declared.
 */

#ifdef NDEBUG

#define _expect_ignore                                                                                                                                         \
	if(false) std::cerr << ""

#define expect_decl(_DECL) ((void)0)
#define expect_true(_COND) _expect_ignore
#define expect_eq(_a, _b) _expect_ignore
#define expect_ne(_a, _b) _expect_ignore
#define expect_lt(_a, _b) _expect_ignore
#define expect_le(_a, _b) _expect_ignore
#define expect_gt(_a, _b) _expect_ignore
#define expect_ge(_a, _b) _expect_ignore

#else

#include <iostream>

#include "insieme/utils/unused.h"

namespace insieme {
namespace utils {
	namespace detail {

		struct LazyExpectation {
			bool value;
			LazyExpectation(bool value) : value(value) {}
			~LazyExpectation() {
				if(!value) { std::cerr << "\n"; }
			}
			operator bool() const {
				return !value;
			}
		};

	} // end namespace detail
} // end namespace utils
} // end namespace insieme

#define __xstr(a) __insieme_utils_str(a)
#define __insieme_utils_str(a) #a

#define expect_decl(_DECL) _DECL

#define expect_true(_COND)                                                                                                                                     \
	if(__insieme_unused auto x = insieme::utils::detail::LazyExpectation(_COND))                                                                                       \
	std::cerr << "\nExpectation " #_COND " of " __FILE__ ":" __xstr(__LINE__) " failed!"                                                                       \
	                                                                          "\n"

#define expect_eq(_A, _B)                                                                                                                                      \
	if(__insieme_unused auto x = insieme::utils::detail::LazyExpectation((_A) == (_B)))                                                                                \
	std::cerr << "\nExpectation " #_A " == " #_B " of " __FILE__ ":" __xstr(__LINE__) " failed!\n\t" #_A " = " << (_A) << "\n\t" #_B " = " << (_B) << "\n"

#define expect_ne(_A, _B)                                                                                                                                      \
	if(__insieme_unused auto x = insieme::utils::detail::LazyExpectation((_A) != (_B)))                                                                                \
	std::cerr << "\nExpectation " #_A " != " #_B " of " __FILE__ ":" __xstr(__LINE__) " failed!\n\t" #_A " = " << (_A) << "\n\t" #_B " = " << (_B) << "\n"

#define expect_lt(_A, _B)                                                                                                                                      \
	if(__insieme_unused auto x = insieme::utils::detail::LazyExpectation((_A) < (_B)))                                                                                 \
	std::cerr << "\nExpectation " #_A " < " #_B " of " __FILE__ ":" __xstr(__LINE__) " failed!\n\t" #_A " = " << (_A) << "\n\t" #_B " = " << (_B) << "\n"

#define expect_le(_A, _B)                                                                                                                                      \
	if(__insieme_unused auto x = insieme::utils::detail::LazyExpectation((_A) <= (_B)))                                                                                \
	std::cerr << "\nExpectation " #_A " <= " #_B " of " __FILE__ ":" __xstr(__LINE__) " failed!\n\t" #_A " = " << (_A) << "\n\t" #_B " = " << (_B) << "\n"

#define expect_gt(_A, _B)                                                                                                                                      \
	if(__insieme_unused auto x = insieme::utils::detail::LazyExpectation((_A) > (_B)))                                                                                 \
	std::cerr << "\nExpectation " #_A " > " #_B " of " __FILE__ ":" __xstr(__LINE__) " failed!\n\t" #_A " = " << (_A) << "\n\t" #_B " = " << (_B) << "\n"

#define expect_ge(_A, _B)                                                                                                                                      \
	if(__insieme_unused auto x = insieme::utils::detail::LazyExpectation((_A) >= (_B)))                                                                                \
	std::cerr << "\nExpectation " #_A " >= " #_B " of " __FILE__ ":" __xstr(__LINE__) " failed!\n\t" #_A " = " << (_A) << "\n\t" #_B " = " << (_B) << "\n"


#endif

// ------ derived definitions ------

#define expect_false(_COND) expect_true(!(_COND))
