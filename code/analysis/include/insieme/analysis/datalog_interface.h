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

#pragma once

#include "insieme/analysis/cba_interface.h"
#include "insieme/analysis/datalog/context.h"
#include "insieme/analysis/datalog/alias_analysis.h"
#include "insieme/analysis/datalog/boolean_analysis.h"
#include "insieme/analysis/datalog/code_properties.h"
#include "insieme/analysis/datalog/integer_analysis.h"

namespace insieme {
namespace analysis {

	/*
	 * Create a type for this backend
	 */
	struct DatalogEngine : public analysis_engine<datalog::Context> {};


	// --- Alias Analysis ---

	register_analysis_implementation( DatalogEngine , areAlias, datalog::areAlias );
	register_analysis_implementation( DatalogEngine , mayAlias, datalog::mayAlias );
	register_analysis_implementation( DatalogEngine , notAlias, datalog::notAlias );


	// --- Boolean Analysis ---

	register_analysis_implementation( DatalogEngine , isTrue,     datalog::isTrue     );
	register_analysis_implementation( DatalogEngine , isFalse,    datalog::isFalse    );
	register_analysis_implementation( DatalogEngine , mayBeTrue,  datalog::mayBeTrue  );
	register_analysis_implementation( DatalogEngine , mayBeFalse, datalog::mayBeFalse );


	// --- Simple Integer Analysis ---

	register_analysis_implementation( DatalogEngine , getIntegerValues,  datalog::getIntegerValues  );
	register_analysis_implementation( DatalogEngine , isIntegerConstant, datalog::isIntegerConstant );

	register_analysis_implementation( DatalogEngine , areEqualInteger,      datalog::integer::areEqual    );
	register_analysis_implementation( DatalogEngine , areNotEqualInteger,   datalog::integer::areNotEqual );
	register_analysis_implementation( DatalogEngine , mayBeEqualInteger,    datalog::integer::mayEqual    );
	register_analysis_implementation( DatalogEngine , mayBeNotEqualInteger, datalog::integer::mayNotEqual );


} // end namespace analysis
} // end namespace insieme
