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

#include "insieme/analysis/haskell/boolean_analysis.h"
#include "insieme/analysis/common/failure.h"

extern "C" {

	namespace hat = insieme::analysis::haskell;

	// Analysis
	int hat_check_boolean(hat::StablePtr ctxt, const hat::HaskellNodeAddress expr_hs);

}

namespace insieme {
namespace analysis {
namespace haskell {

	#include "boolean_analysis.h"

	BooleanAnalysisResult checkBoolean(Context& ctxt, const core::ExpressionAddress& expr) {
		auto expr_hs = ctxt.resolveNodeAddress(expr);
		auto res = static_cast<BooleanAnalysisResult>(hat_check_boolean(ctxt.getHaskellContext(), expr_hs));
		if(res == BooleanAnalysisResult_Neither) {
			std::vector<std::string> msgs{"Boolean Analysis Error"};
			throw AnalysisFailure(msgs);
		}
		return res;
	}

	bool isTrue(Context& ctxt, const core::ExpressionAddress& expr) {
		return checkBoolean(ctxt, expr) == BooleanAnalysisResult_AlwaysTrue;
	}

	bool isFalse(Context& ctxt, const core::ExpressionAddress& expr) {
		return checkBoolean(ctxt, expr) == BooleanAnalysisResult_AlwaysFalse;
	}

	bool mayBeTrue(Context& ctxt, const core::ExpressionAddress& expr) {
		auto res = checkBoolean(ctxt, expr);
		return res == BooleanAnalysisResult_AlwaysTrue || res == BooleanAnalysisResult_Both;
	}

	bool mayBeFalse(Context& ctxt, const core::ExpressionAddress& expr) {
		auto res = checkBoolean(ctxt, expr);
		return res == BooleanAnalysisResult_AlwaysFalse || res == BooleanAnalysisResult_Both;
	}

} // end namespace haskell
} // end namespace analysis
} // end namespace insieme
