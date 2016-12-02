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

#include "insieme/frontend/utils/conversion_test_utils.h"

#include "insieme/frontend/extensions/omp_frontend_extension.h"
#include "insieme/frontend/extensions/test_pragma_extension.h"

namespace insieme {
namespace frontend {

	namespace {
		void runOmpTestOn(const string& fn, std::function<void(ConversionJob&)> jobModifier = [](ConversionJob& job) {}) {
			utils::runConversionTestOn(fn, [&jobModifier](ConversionJob& job) {
				job.forceFrontendExtension<extensions::OmpFrontendExtension>();
				jobModifier(job);
			});
		}
	}

	TEST(OpenMPConversion, Basic) {
		runOmpTestOn(FRONTEND_TEST_DIR "/inputs/omp/omp_basic.c");
	}

	TEST(OpenMPConversion, Capture) {
		runOmpTestOn(FRONTEND_TEST_DIR "/inputs/omp/omp_capture.c");
	}

	TEST(OpenMPConversion, Functions) {
		runOmpTestOn(FRONTEND_TEST_DIR "/inputs/omp/omp_functions.c");
	}

	TEST(OpenMPConversion, Task) {
		runOmpTestOn(FRONTEND_TEST_DIR "/inputs/omp/omp_task.c");
	}

} // fe namespace
} // insieme namespace
