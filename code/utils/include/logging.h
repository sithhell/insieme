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

#pragma once

#include <iostream>

//FIXME: which define for Visual Studio?
#ifdef WIN32
#  define GOOGLE_GLOG_DLL_DECL
#  pragma warning(push)
#  pragma warning(disable:4244)
#endif

//FIXME: InstallFailureSignalHandler had to be deactivated (Visual Studio 2010 link fix)
#include <glog/logging.h>

#ifdef WIN32
#  pragma warning(pop)
#endif

#include "cmd_line_utils.h"

using namespace google;

// we remove the Google Log DVLOG and VLOG macro
#undef VLOG
#undef DVLOG
#undef VLOG_IS_ON

#ifndef WIN32
#define VLOG(level) 		LOG_IF(INFO, (level) <= CommandLineOptions::Verbosity)
#define DVLOG(level) 		DLOG_IF(INFO, (level) <= CommandLineOptions::Verbosity)
#define VLOG_IS_ON(level) 	( (level) <= CommandLineOptions::Verbosity )
#else
#define VLOG(level)   std::cout  << "\n"
#define DVLOG(level)  std::cout  << "\n"
#undef LOG
#undef DLOG
#define LOG(x) std::cout << "\n"
#define DLOG(x) std::cout << "\n"
#define VLOG_IS_ON(level) false
#endif




namespace insieme {
namespace utils {

void InitLogger(const char* progName, google::LogSeverity level, bool enableFailureHandler);

} // End utils namespace
} // End insieme namespace
