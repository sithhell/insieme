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

#include <gtest/gtest.h>

#include "insieme/backend/sequential/sequential_backend.h"

#include "insieme/core/forward_decls.h"
#include "insieme/core/ir_builder.h"

#include "insieme/core/checks/full_check.h"
#include "insieme/core/printer/pretty_printer.h"
#include "insieme/core/printer/error_printer.h"

#include "insieme/utils/compiler/compiler.h"


namespace insieme {
namespace backend {

	using namespace core;

	TEST(OperatorTest, NarrowTest) {
		NodeManager mgr;
		IRBuilder builder(mgr);

		ProgramPtr prog = builder.parseProgram(R"(

			alias int = int<4>;

			alias pair = struct A {
				first : int;
				second : int;
			};

			alias obj = struct B {
				a : int;
				b : array<int,4u>;
				c : pair;
			};

			unit main() {

				var ref<obj> o;

				// check element o.a
				o.a = 0;
				print("o.a = %d\n", *o.a);

				// updated member a directly
				var ref<ref<int>> x = ref_narrow(o, dp_member(dp_root(type_lit(obj)), lit("a"), type_lit(int)));
				*x = 12;
				print("Equal: %d\n", ref_eq(o.a, *x));
				print("x = %d \t o.a = %d\n", **x, *o.a);


				// update element of b directly
				o.b[2u] = 0;
				print("o.b[2] = %d\n", *o.b[2u]);

				var ref<ref<int>> y = ref_narrow(o, dp_element(dp_member(dp_root(type_lit(obj)), lit("b"), type_lit(array<int,4u>)), 2u));
				*y = 12;
				print("Equal: %d\n", ref_eq(o.b[2u], *y));
				print("y = %d \t o.b[2] = %d\n", **y, *o.b[2u]);


				// expand a definition
				var ref<ref<array<int,4u>>> v = ref_expand(*y, dp_element(dp_root(type_lit(array<int,4u>)), 2u));
				(*v)[1u] = 10;
				(*v)[2u] = 14;

				print("Equal: %d\n", ref_eq(o.b, *v));
				print("v[1] = %d \t v[2] = %d \t o.v[1] = %d \t o.v[2] = %d \t y = %d\n",
						(**v)[1u], (**v)[2u], *o.b[1u], *o.b[2u], **y);


				// handle nested element
				var ref<ref<int>> first = ref_narrow(o, dp_member(dp_member(dp_root(type_lit(obj)), lit("c"), type_lit(pair)), lit("first"), type_lit(int)));

				// check reference equality
				print("Equal: %d\n", ref_eq(o.c.first, *first));

				// and the reverse
				var ref<ref<obj>> full = ref_expand(*first, dp_member(dp_member(dp_root(type_lit(obj)), lit("c"), type_lit(pair)), lit("first"), type_lit(int)));
				print("Equal: %d\n", ref_eq(o,*full));
			}
		)").as<core::ProgramPtr>();

		// check whether parsing was successful
		ASSERT_TRUE(prog);

		// check input program
		EXPECT_TRUE(core::checks::check(prog).empty()) << core::printer::dumpErrors(core::checks::check(prog));

		// print program using pretty printer
		//dumpColor(prog);

		// build it using the sequential backend
		auto targetCode = sequential::SequentialBackend::getDefault()->convert(prog);

		//std::cout << "Converted Code: \n" << *targetCode << "\n";

		// try compiling code
		EXPECT_TRUE(utils::compiler::compile(*targetCode));
	}


} // end namespace backend
} // end namespace insieme
