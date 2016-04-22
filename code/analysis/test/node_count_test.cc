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

#include "insieme/analysis/haskell/adapter.h"

static unsigned char forloops_irb[] = {
	0x45, 0x52, 0x49, 0x50, 0x53, 0x4e, 0x49, 0x00, 0x03, 0x00, 0x00, 0x00,
	0x1d, 0x00, 0x00, 0x00, 0x42, 0x75, 0x69, 0x6c, 0x74, 0x49, 0x6e, 0x54,
	0x61, 0x67, 0x41, 0x6e, 0x6e, 0x6f, 0x74, 0x61, 0x74, 0x69, 0x6f, 0x6e,
	0x43, 0x6f, 0x6e, 0x76, 0x65, 0x72, 0x74, 0x65, 0x72, 0x1a, 0x00, 0x00,
	0x00, 0x4e, 0x61, 0x6d, 0x65, 0x54, 0x61, 0x67, 0x41, 0x6e, 0x6e, 0x6f,
	0x74, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x43, 0x6f, 0x6e, 0x76, 0x65, 0x72,
	0x74, 0x65, 0x72, 0x1d, 0x00, 0x00, 0x00, 0x44, 0x65, 0x72, 0x69, 0x76,
	0x65, 0x64, 0x54, 0x61, 0x67, 0x41, 0x6e, 0x6e, 0x6f, 0x74, 0x61, 0x74,
	0x69, 0x6f, 0x6e, 0x43, 0x6f, 0x6e, 0x76, 0x65, 0x72, 0x74, 0x65, 0x72,
	0xa1, 0x00, 0x00, 0x00, 0x10, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00,
	0x00, 0x00, 0x94, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x12, 0x00,
	0x04, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x23, 0x00, 0x00, 0x00,
	0x25, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0f, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x21, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x02, 0x00, 0x00, 0x00,
	0x18, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x19, 0x00, 0x02, 0x00,
	0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x08, 0x00, 0x03, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
	0x08, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x19, 0x00, 0x02, 0x00,
	0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x04, 0x00, 0x03, 0x00, 0x00, 0x00, 0x72, 0x65, 0x66, 0x00,
	0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x24, 0x00, 0x04, 0x00, 0x00, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x12,
	0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00,
	0x00, 0x08, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x04, 0x00, 0x05, 0x00, 0x00, 0x00, 0x61, 0x72, 0x72, 0x61, 0x79,
	0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x02, 0x00, 0x00, 0x00, 0x0d, 0x00,
	0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00,
	0x03, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
	0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x04, 0x00,
	0x00, 0x00, 0x63, 0x68, 0x61, 0x72, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00,
	0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x0f, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x03, 0x00, 0x00, 0x00,
	0x69, 0x6e, 0x66, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00, 0x00,
	0x00, 0x13, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00,
	0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00,
	0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x66, 0x00, 0x00, 0x00, 0x00,
	0x08, 0x00, 0x03, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00, 0x08, 0x00,
	0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x04, 0x00, 0x05, 0x00, 0x00, 0x00,
	0x70, 0x6c, 0x61, 0x69, 0x6e, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x0b,
	0x00, 0x00, 0x00, 0x62, 0x75, 0x69, 0x6c, 0x74, 0x69, 0x6e, 0x5f, 0x74,
	0x61, 0x67, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x69, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00, 0x00, 0x00, 0x19,
	0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x00, 0x00, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x04,
	0x00, 0x04, 0x00, 0x00, 0x00, 0x75, 0x69, 0x6e, 0x74, 0x00, 0x00, 0x00,
	0x00, 0x24, 0x00, 0x01, 0x00, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x09, 0x00, 0x01, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0x00, 0x02, 0x00, 0x00, 0x00, 0x1d,
	0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08,
	0x00, 0x03, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00,
	0x00, 0x1e, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x06, 0x00, 0x00, 0x00, 0x24, 0x00, 0x01, 0x00, 0x00, 0x00, 0x10,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00,
	0x00, 0x34, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x02, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x19, 0x00, 0x02, 0x00, 0x00, 0x00, 0x18, 0x00,
	0x00, 0x00, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00,
	0x02, 0x00, 0x00, 0x00, 0x31, 0x30, 0x00, 0x00, 0x00, 0x00, 0x19, 0x00,
	0x02, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x00, 0x35, 0x30,
	0x00, 0x00, 0x00, 0x00, 0x19, 0x00, 0x02, 0x00, 0x00, 0x00, 0x18, 0x00,
	0x00, 0x00, 0x26, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x31, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x01,
	0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e,
	0x00, 0x04, 0x00, 0x00, 0x00, 0x29, 0x00, 0x00, 0x00, 0x2d, 0x00, 0x00,
	0x00, 0x8c, 0x00, 0x00, 0x00, 0x93, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x08, 0x00, 0x03, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x08,
	0x00, 0x00, 0x00, 0x2a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24,
	0x00, 0x04, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00,
	0x00, 0x12, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x08, 0x00, 0x03, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x08,
	0x00, 0x00, 0x00, 0x1a, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x04, 0x00, 0x03, 0x00, 0x00,
	0x00, 0x69, 0x6e, 0x74, 0x00, 0x00, 0x00, 0x00, 0x1b, 0x00, 0x03, 0x00,
	0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x46, 0x00, 0x00, 0x00, 0x47, 0x00,
	0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00,
	0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x2e, 0x00, 0x00, 0x00, 0x19, 0x00,
	0x02, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x2f, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x72, 0x65,
	0x66, 0x5f, 0x61, 0x72, 0x72, 0x61, 0x79, 0x5f, 0x65, 0x6c, 0x65, 0x6d,
	0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x03, 0x00, 0x00, 0x00, 0x31, 0x00,
	0x00, 0x00, 0x43, 0x00, 0x00, 0x00, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x24, 0x00, 0x02, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00,
	0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00,
	0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x33, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x04, 0x00, 0x00, 0x00,
	0x34, 0x00, 0x00, 0x00, 0x3a, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00,
	0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00,
	0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x35, 0x00,
	0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00,
	0x00, 0x00, 0x24, 0x00, 0x02, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00,
	0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00,
	0x00, 0x00, 0x37, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x61, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01,
	0x00, 0x00, 0x00, 0x39, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
	0x00, 0x01, 0x00, 0x00, 0x00, 0x73, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x3b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x63, 0x00, 0x00, 0x00, 0x00, 0x05,
	0x00, 0x01, 0x00, 0x00, 0x00, 0x3d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x76, 0x00, 0x00, 0x00, 0x00,
	0x08, 0x00, 0x03, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x08, 0x00,
	0x00, 0x00, 0x3f, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x24, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x01, 0x00,
	0x00, 0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0x00,
	0x02, 0x00, 0x00, 0x00, 0x1d, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x38, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00,
	0x00, 0x08, 0x00, 0x00, 0x00, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x24, 0x00, 0x04, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x3a,
	0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x1c, 0x00, 0x02, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x2f,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0x00, 0x01, 0x00, 0x00,
	0x00, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0x00, 0x02,
	0x00, 0x00, 0x00, 0x46, 0x00, 0x00, 0x00, 0x49, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x31, 0x00, 0x03, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00,
	0x00, 0x4a, 0x00, 0x00, 0x00, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x37, 0x00, 0x02, 0x00, 0x00, 0x00, 0x4b, 0x00, 0x00, 0x00, 0x51,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x02, 0x00, 0x00,
	0x00, 0x4e, 0x00, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
	0x00, 0x01, 0x00, 0x00, 0x00, 0x4c, 0x00, 0x00, 0x00, 0x19, 0x00, 0x02,
	0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x4d, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x72, 0x00, 0x00,
	0x00, 0x00, 0x08, 0x00, 0x03, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
	0x08, 0x00, 0x00, 0x00, 0x4f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x24, 0x00, 0x04, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x12, 0x00,
	0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x1a, 0x00, 0x02, 0x00, 0x00, 0x00, 0x52, 0x00, 0x00, 0x00, 0x45, 0x00,
	0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00,
	0x00, 0x00, 0x08, 0x00, 0x03, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
	0x08, 0x00, 0x00, 0x00, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x24, 0x00, 0x04, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x12, 0x00,
	0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x10, 0x00, 0x01, 0x00, 0x00, 0x00, 0x55, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x02, 0x00, 0x00, 0x00, 0x56, 0x00,
	0x00, 0x00, 0x8b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x00,
	0x04, 0x00, 0x00, 0x00, 0x43, 0x00, 0x00, 0x00, 0x57, 0x00, 0x00, 0x00,
	0x67, 0x00, 0x00, 0x00, 0x6d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x19, 0x00, 0x02, 0x00, 0x00, 0x00, 0x5a, 0x00, 0x00, 0x00, 0x59, 0x00,
	0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00,
	0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x58, 0x00, 0x00, 0x00, 0x19, 0x00,
	0x02, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x59, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x0a, 0x00, 0x00, 0x00, 0x72, 0x65,
	0x66, 0x5f, 0x6e, 0x61, 0x72, 0x72, 0x6f, 0x77, 0x00, 0x00, 0x00, 0x00,
	0x06, 0x00, 0x03, 0x00, 0x00, 0x00, 0x5b, 0x00, 0x00, 0x00, 0x65, 0x00,
	0x00, 0x00, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00,
	0x02, 0x00, 0x00, 0x00, 0x5c, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00, 0x00, 0x00, 0x07, 0x00,
	0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x5d, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x24, 0x00, 0x04, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00,
	0x3a, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x5e, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x5f, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x6b, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00, 0x00, 0x00, 0x61,
	0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x62, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x04, 0x00, 0x08, 0x00, 0x00, 0x00, 0x64, 0x61, 0x74,
	0x61, 0x70, 0x61, 0x74, 0x68, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x02,
	0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x63, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x62,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00, 0x00, 0x00, 0x07, 0x00,
	0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x24, 0x00, 0x04, 0x00, 0x00, 0x00, 0x63, 0x00, 0x00, 0x00,
	0x3a, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x5e, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x03, 0x00, 0x00, 0x00, 0x32, 0x00,
	0x00, 0x00, 0x68, 0x00, 0x00, 0x00, 0x4b, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x19, 0x00, 0x02, 0x00, 0x00, 0x00, 0x6b, 0x00, 0x00, 0x00,
	0x6a, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x06, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x69, 0x00, 0x00, 0x00,
	0x19, 0x00, 0x02, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x6a, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x09, 0x00, 0x00, 0x00,
	0x72, 0x65, 0x66, 0x5f, 0x64, 0x65, 0x72, 0x65, 0x66, 0x00, 0x00, 0x00,
	0x00, 0x06, 0x00, 0x03, 0x00, 0x00, 0x00, 0x6c, 0x00, 0x00, 0x00, 0x36,
	0x00, 0x00, 0x00, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24,
	0x00, 0x01, 0x00, 0x00, 0x00, 0x5c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x1e, 0x00, 0x04, 0x00, 0x00, 0x00, 0x6e, 0x00, 0x00, 0x00, 0x70,
	0x00, 0x00, 0x00, 0x79, 0x00, 0x00, 0x00, 0x8a, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00, 0x00, 0x00, 0x61, 0x00, 0x00,
	0x00, 0x08, 0x00, 0x00, 0x00, 0x6f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x24, 0x00, 0x02, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00, 0x36,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0x00, 0x02, 0x00, 0x00,
	0x00, 0x73, 0x00, 0x00, 0x00, 0x72, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
	0x00, 0x71, 0x00, 0x00, 0x00, 0x19, 0x00, 0x02, 0x00, 0x00, 0x00, 0x05,
	0x00, 0x00, 0x00, 0x72, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
	0x00, 0x0a, 0x00, 0x00, 0x00, 0x64, 0x70, 0x5f, 0x65, 0x6c, 0x65, 0x6d,
	0x65, 0x6e, 0x74, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x03, 0x00, 0x00,
	0x00, 0x74, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x45, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x02, 0x00, 0x00, 0x00, 0x75,
	0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08,
	0x00, 0x03, 0x00, 0x00, 0x00, 0x61, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00,
	0x00, 0x76, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x02,
	0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x77, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00,
	0x00, 0x08, 0x00, 0x00, 0x00, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x24, 0x00, 0x02, 0x00, 0x00, 0x00, 0x63, 0x00, 0x00, 0x00, 0x38,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x03, 0x00, 0x00,
	0x00, 0x7a, 0x00, 0x00, 0x00, 0x7c, 0x00, 0x00, 0x00, 0x86, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00, 0x00, 0x00, 0x61,
	0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x7b, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x24, 0x00, 0x02, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00,
	0x00, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0x00, 0x02,
	0x00, 0x00, 0x00, 0x7f, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x02,
	0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x7d, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x19, 0x00, 0x02, 0x00, 0x00,
	0x00, 0x05, 0x00, 0x00, 0x00, 0x7e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x04, 0x00, 0x07, 0x00, 0x00, 0x00, 0x64, 0x70, 0x5f, 0x72, 0x6f,
	0x6f, 0x74, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x03, 0x00, 0x00, 0x00,
	0x80, 0x00, 0x00, 0x00, 0x84, 0x00, 0x00, 0x00, 0x45, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x01, 0x00, 0x00, 0x00, 0x81, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00, 0x00, 0x00,
	0x82, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x83, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x04, 0x00, 0x00, 0x00, 0x74, 0x79,
	0x70, 0x65, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00,
	0x00, 0x00, 0x61, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x85, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x02, 0x00, 0x00, 0x00,
	0x36, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x19, 0x00, 0x02, 0x00, 0x00, 0x00, 0x87, 0x00, 0x00, 0x00, 0x89, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0x00, 0x00, 0x00,
	0x82, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x88, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x01, 0x00, 0x00, 0x00, 0x34, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x0c, 0x00, 0x00, 0x00,
	0x74, 0x79, 0x70, 0x65, 0x5f, 0x6c, 0x69, 0x74, 0x65, 0x72, 0x61, 0x6c,
	0x00, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x03, 0x00, 0x00, 0x00, 0x3e, 0x00,
	0x00, 0x00, 0x68, 0x00, 0x00, 0x00, 0x51, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x1a, 0x00, 0x02, 0x00, 0x00, 0x00, 0x43, 0x00, 0x00, 0x00,
	0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x02, 0x00,
	0x00, 0x00, 0x8d, 0x00, 0x00, 0x00, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x08, 0x00, 0x03, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
	0x08, 0x00, 0x00, 0x00, 0x8e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x24, 0x00, 0x04, 0x00, 0x00, 0x00, 0x8f, 0x00, 0x00, 0x00, 0x12, 0x00,
	0x00, 0x00, 0x12, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x08, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00,
	0x08, 0x00, 0x00, 0x00, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x24, 0x00, 0x02, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x00, 0x00, 0x91, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x92, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0x00, 0x02, 0x00,
	0x00, 0x00, 0x1d, 0x00, 0x00, 0x00, 0x26, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x1f, 0x00, 0x02, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00,
	0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x12, 0x00, 0x04, 0x00,
	0x00, 0x00, 0x95, 0x00, 0x00, 0x00, 0x9c, 0x00, 0x00, 0x00, 0x25, 0x00,
	0x00, 0x00, 0x9e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00,
	0x02, 0x00, 0x00, 0x00, 0x96, 0x00, 0x00, 0x00, 0x9a, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x02, 0x00, 0x00, 0x00, 0x18, 0x00,
	0x00, 0x00, 0x99, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00,
	0x00, 0x00, 0x97, 0x00, 0x00, 0x00, 0x19, 0x00, 0x02, 0x00, 0x00, 0x00,
	0x05, 0x00, 0x00, 0x00, 0x98, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x6a, 0x00, 0x00, 0x00, 0x00, 0x03,
	0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0x00, 0x02,
	0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x9b, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x35, 0x00, 0x00,
	0x00, 0x00, 0x19, 0x00, 0x02, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00,
	0x9d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x02, 0x00,
	0x00, 0x00, 0x32, 0x35, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x01, 0x00,
	0x00, 0x00, 0x9f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x00,
	0x04, 0x00, 0x00, 0x00, 0x29, 0x00, 0x00, 0x00, 0x2d, 0x00, 0x00, 0x00,
	0x8c, 0x00, 0x00, 0x00, 0xa0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x1f, 0x00, 0x02, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x96, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static unsigned int forloops_irb_len = 3198;

namespace insieme {
namespace analysis {
namespace haskell {

	TEST(HaskellAdapter, NodeCount) {
		enter();
		void* dump = passDump(forloops_irb, forloops_irb_len);
		EXPECT_EQ(1599, getNodeCount(dump));
		freeDump(dump);
		exit();
	}

} // end namespace haskell
} // end namespace analysis
} // end namespace insieme
