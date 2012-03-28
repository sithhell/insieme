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

#include <gtest/gtest.h>

#include "insieme/core/ir_program.h"
#include "insieme/core/checks/ir_checks.h"
#include "insieme/core/printer/pretty_printer.h"

#include "insieme/frontend/program.h"
#include "insieme/frontend/clang_config.h"

#include "insieme/backend/ocl_kernel/kernel_preprocessor.h"

#include "insieme/analysis/features/code_feature_catalog.h"

#include "insieme/utils/logging.h"

#include <iostream>
#include <fstream>
#include <sstream>,
#include <cmath>

using namespace insieme;
namespace af = analysis::features;

TEST(OclFeaturesTest, StaticFeaturesTest) {
	Logger::get(std::cerr, INFO);
	CommandLineOptions::Verbosity = 2;
	core::NodeManager manager;
	core::ProgramPtr program = core::Program::get(manager);


	LOG(INFO) << "Converting input program '" << std::string(SRC_DIR) << "inputs/hello.cl" << "' to IR...";
	insieme::frontend::Program prog(manager);

	std::cout << SRC_DIR << std::endl;
	prog.addTranslationUnit(std::string(SRC_DIR) + "inputs/hello.cl");
	program = prog.convert();
	LOG(INFO) << "Done.";

	backend::ocl_kernel::KernelPreprocessor kpp;
	core::NodePtr kernel = kpp.process(manager, program);

	core::printer::PrettyPrinter pp(kernel, core::printer::PrettyPrinter::OPTIONS_DETAIL);

	LOG(INFO) << "Printing the IR: " << pp;

	auto errors = core::check(program, insieme::core::checks::getFullCheck()).getAll();

	EXPECT_EQ(errors.size(), 0u);

	std::sort(errors.begin(), errors.end());

	for_each(errors, [](const core::Message& cur) {
		LOG(INFO) << cur << std::endl;
	});

	// extracting features from the kernel
	af::FeatureCatalog catalog;
	catalog.addAll(af::getFullCodeFeatureCatalog());

	double intOPs = af::getValue<double>(catalog.getFeature("SCF_NUM_integer_all_OPs_real")->extractFrom(kernel));
	double vecIntOPs = af::getValue<double>(catalog.getFeature("SCF_NUM_integer_all_VEC_OPs_real")->extractFrom(kernel));

	double floatOPs = af::getValue<double>(catalog.getFeature("SCF_NUM_real*_all_OPs_real")->extractFrom(kernel));
	double vecFloatOPs = af::getValue<double>(catalog.getFeature("SCF_NUM_real*_all_VEC_OPs_real")->extractFrom(kernel));

	double intrinsics = af::getValue<double>(catalog.getFeature("SCF_NUM_externalFunction_lambda_real")->extractFrom(kernel));

	double barriers = af::getValue<double>(catalog.getFeature("SCF_NUM_barrier_Calls_real")->extractFrom(kernel));

	double memoryAccesses = af::getValue<double>(catalog.getFeature("SCF_IO_NUM_any_read/write_OPs_real")->extractFrom(kernel));
	double relLocalmemAcc = af::getValue<double>(catalog.getFeature("SCF_COMP_localMemoryAccesses-allMemoryAccesses_real_ratio")->extractFrom(kernel));
	double cpmputeMemoryRatio = af::getValue<double>(catalog.getFeature("SCF_COMP_allOPs-memoryAccesses_real_2:1ratio")->extractFrom(kernel));

	double totalComputation = af::getValue<double>(catalog.getFeature("SCF_COMP_scalarOps-vectorOps_real_sum")->extractFrom(kernel));

	EXPECT_EQ(1.0, intOPs);
	EXPECT_EQ(0.0, vecIntOPs);

	EXPECT_EQ(1.0, floatOPs);
	EXPECT_EQ(800.0, vecFloatOPs);

	EXPECT_EQ(106.0, intrinsics);

	EXPECT_EQ(0.0, barriers);

	EXPECT_EQ(34.0, memoryAccesses);
	EXPECT_GT(0.001, fabs(0.02941 - relLocalmemAcc));
	EXPECT_GT(0.001, fabs(23.5882 - cpmputeMemoryRatio));

	EXPECT_EQ(802.0, totalComputation);

	// code_features.cpp:56
	// cache_utils.h:72

}
