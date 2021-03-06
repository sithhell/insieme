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

#include "insieme/analysis/cba/datalog/code_properties.h"

#include "insieme/analysis/cba/datalog/framework/souffle_extractor.h"

#include "insieme/core/ir_address.h"

#include "souffle/gen/polymorph_types_analysis.h"
#include "souffle/gen/top_level_term.h"
#include "souffle/gen/exit_point_analysis.h"

#include "souffle/gen/definition_point.h"
#include "souffle/gen/happens_before_analysis.h"

namespace insieme {
namespace analysis {
namespace cba {
namespace datalog {

	/**
	 * Determines whether the given type is a polymorph type.
	 */
	bool isPolymorph(Context& context, const core::TypePtr& type, bool debug)
	{
		// Instantiate analysis
		auto &analysis = context.getAnalysis<souffle::Sf_polymorph_types_analysis>(type, debug);

		// Get target node ID for which to check -> the root node
		int targetID = 0;

		// Get result
		auto &resultRel = analysis.rel_result;
		auto resultRelContents = resultRel.template equalRange<0>({{targetID}});

		// If result is non-empty, return true
		return resultRelContents.begin() != resultRelContents.end();
	}

	/**
	 * Determine top level nodes
	 */
	bool getTopLevelNodes(Context& context, const core::NodePtr& root, bool debug)
	{
		// Instantiate analysis
		auto &analysis = context.getAnalysis<souffle::Sf_top_level_term>(root, debug);

		// Get target node ID for which to check -> the root node
		int targetID = 0;

		// Get result
		auto &resultRel = analysis.rel_TopLevel;

		return resultRel.size() != 0 && resultRel.contains(targetID);
	}

	/**
	 * Get exit points from a given lambda function
	 */
	std::set<core::ReturnStmtAddress> performExitPointAnalysis(Context& context, const core::LambdaAddress& lambda, bool debug)
	{
		// Instantiate analysis
		auto &analysis = context.getAnalysis<souffle::Sf_exit_point_analysis>(lambda.getRootNode(), debug);

		// Get ID for given Lambda
		int targetLambdaID = context.getNodeID(lambda);

		// Get result
		auto &resultRel = analysis.rel_ExitPoints;

		// Now map the exit point IDs from root Lambda to actual ReturnStmts
		std::set<core::ReturnStmtAddress> res;

		for (const auto &cur : resultRel) {
			int lambdaID = cur[0];
			int returnStmtID = cur[1];

			if (lambdaID != targetLambdaID) {
				continue;
			}

			auto rsa = context.getNodeForID(lambda.getRootNode(), returnStmtID, debug).as<core::ReturnStmtAddress>();

			// Crop addresses to start at given lambda
			rsa = core::cropRootNode(rsa, lambda);

			res.insert(rsa);
		}

		return res;
	}


	core::VariableAddress getDefinitionPoint(Context& context, const core::VariableAddress& var, bool debug)
	{
		// Instantiate analysis
		auto &analysis = context.getAnalysis<souffle::Sf_definition_point>(var.getRootNode(), debug);

		// Get ID for variable we are interested in (if it's there)
		int targetVariableID = context.getNodeID(var, debug);

		// Get result
		auto &resultRel = analysis.rel_Result;

		auto defPoint = resultRel.template equalRange<0>({{targetVariableID,0}});

		if (defPoint.empty()) {
			if (debug)
				std::cout << "Debug: Could not find definition point for variable " << var << "...";
			return {};
		}

		auto defPointID = (*defPoint.begin())[1];

		assert_true(++defPoint.begin() == defPoint.end())
		                << "Analysis failed: Multiple definition points found for var " << var << "!";

		return context.getNodeForID(var.getRootNode(), defPointID, debug).as<core::VariableAddress>();
	}


	bool happensBefore(Context &context, const core::StatementAddress& a, const core::StatementAddress& b, bool debug)
	{
		assert_eq(a.getRootNode(), b.getRootNode());

		// Instantiate analysis
		auto &analysis = context.getAnalysis<souffle::Sf_happens_before_analysis>(a.getRootNode(), debug);

		// Get ID of both statements
		int targetStartID = context.getNodeID(a);
		int targetEndID = context.getNodeID(b);

		// Get result
		auto &resultRel = analysis.rel_Result;

		// Filter for first value in result set
		auto tmp = resultRel.template equalRange<0>({{targetStartID,0}});

		// Filter for second value in result set
		for (auto it = tmp.begin(); it != tmp.end(); ++it) {
			// Found target end
			if ((*it)[1] == targetEndID) {
				// Check result size
				if (++it == tmp.end() || (*it)[1] != targetEndID)
					return true; // Only value, good!
				assert_fail() << "Happens-before analysis seems to be broken!";
				return false;
			}
		}

		return false;
	}

} // end namespace datalog
} // end namespace cba
} // end namespace analysis
} // end namespace insieme
