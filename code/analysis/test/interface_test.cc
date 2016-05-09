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

#include <gtest/gtest.h>

#include <boost/optional.hpp>

#include "insieme/analysis/dataflow.h"

#include "insieme/core/ir_builder.h"


namespace insieme {
namespace analysis {

	using namespace core;

	#define create_dispatcher_for(FUNC)                                                                             \
	    boost::optional<core::VariableAddress> dispatch_##FUNC(const core::VariableAddress& var, Backend backend) { \
	        switch(backend) {                                                                                       \
	        case Backend::DATALOG: return FUNC<Backend::DATALOG>(var);                                              \
	        case Backend::HASKELL: return FUNC<Backend::HASKELL>(var);                                              \
	        default: assert_not_implemented() << "Backend not implemented!";                                        \
	        }                                                                                                       \
	        return boost::optional<core::VariableAddress>();                                                        \
	    }

	create_dispatcher_for(getDefinitionPoint)

	#undef create_dispatcher_for


	class CBA_Interface : public ::testing::TestWithParam<Backend> {

	};

	TEST_P(CBA_Interface, DefinitionPoint) {
		NodeManager mgr;
		IRBuilder builder(mgr);

		auto addresses = builder.parseAddressesStatement(
			"{ var int<4> x = 12; $x$; }"
		);

		ASSERT_EQ(1, addresses.size());

		auto var = addresses[0].as<VariableAddress>();
		auto param = var.getRootAddress().as<CompoundStmtAddress>()[0].as<DeclarationStmtAddress>()->getVariable();

		std::cout << "Parameter: " << param << "\n";
		std::cout << "Variable:  " << var << "\n";

		EXPECT_EQ(param, dispatch_getDefinitionPoint(var, GetParam()));

	}

	INSTANTIATE_TEST_CASE_P(CBA, CBA_Interface, ::testing::Values(Backend::DATALOG, Backend::HASKELL));

} // end namespace analysis
} // end namespace insieme