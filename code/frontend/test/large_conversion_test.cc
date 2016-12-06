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

#include "insieme/frontend/extensions/variable_argument_list_extension.h"
#include "insieme/frontend/extensions/variable_length_array_extension.h"

namespace insieme {
namespace frontend {

	static inline void runLargeConversionTestOn(const string& fn, std::vector<std::string> includeDirs = toVector<std::string>()) {
		utils::runConversionTestOn(fn, [&](ConversionJob& job) {
			job.registerFrontendExtension<extensions::VariableLengthArrayExtension>();
			job.registerFrontendExtension<extensions::VariableArgumentListExtension>();
			for(auto inc : includeDirs) {
				job.addIncludeDirectory(inc);
			}
		});
	}

	TEST(LargeConversionTest, HelloWorld) {
		runLargeConversionTestOn(FRONTEND_TEST_DIR + "/inputs/conversion/c_hello_world.c");
	}

	TEST(LargeConversionTest, MatrixMul) {
		runLargeConversionTestOn(FRONTEND_TEST_DIR + "/inputs/conversion/c_matrix_mul.c");
	}

	TEST(LargeConversionTest, Pendulum) {
		runLargeConversionTestOn(FRONTEND_TEST_DIR + "../../../test/pendulum/pendulum.c");
	}

	TEST(LargeConversionTest, Pyramids) {
		runLargeConversionTestOn(FRONTEND_TEST_DIR + "../../../test/pyramids/pyramids.c");
	}

	TEST(LargeConversionTest, Stencil3D) {
		runLargeConversionTestOn(FRONTEND_TEST_DIR + "../../../test/stencil3d/stencil3d.c");
	}

	// enable after checking recursive struct resolve
	//TEST(LargeConversionTest, QAP) {
	//	runLargeConversionTestOn(FRONTEND_TEST_DIR "../../../test/qap/qap.c");
	//}

} // fe namespace
} // insieme namespace