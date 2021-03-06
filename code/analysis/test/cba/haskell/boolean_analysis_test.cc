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

#include "insieme/analysis/cba/haskell/interface.h"

#include "../common/boolean_analysis_test.inc"

namespace insieme {
namespace analysis {
namespace cba {

	/**
	 * Run the boolean value tests using the haskell backend.
	 */
	INSTANTIATE_TYPED_TEST_CASE_P(Haskell, BooleanValue, HaskellEngine);


	bool isTrue(const StatementAddress& stmt) {
		return isTrue<HaskellEngine>(stmt.as<ExpressionAddress>());
	}

	bool isFalse(const StatementAddress& stmt) {
		return isFalse<HaskellEngine>(stmt.as<ExpressionAddress>());
	}

	TEST(AdvancedBooleanAnalysis, FunctionReferences) {
		NodeManager mgr;
		IRBuilder builder(mgr);

		auto stmt = builder.parseStmt(
				"def always = ()->bool {"
				"	return true;"
				"};"
				""
				"def never = ()->bool {"
				"	return false;"
				"};"
				""
				"{"
				"	always();"
				"	never();"
				"	"
				"	var ref<()->bool> f = always;"
				"	(*f)();"
				"	f = never;"
				"	(*f)();"
				"	"
				"	var ref<()->bool> g = ref_of_function(always);"
				"	(*g)();"
				""
				"}"
		).as<CompoundStmtPtr>();

		auto comp = CompoundStmtAddress(stmt);

		EXPECT_TRUE(isTrue(comp[0]));
		EXPECT_TRUE(isFalse(comp[1]));

		EXPECT_TRUE(isTrue(comp[3]));
		EXPECT_TRUE(isFalse(comp[5]));

		EXPECT_TRUE(isTrue(comp[7]));


	}

} // end namespace cba
} // end namespace analysis
} // end namespace insieme

