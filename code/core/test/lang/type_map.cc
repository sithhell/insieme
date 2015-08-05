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

#include "insieme/core/lang/type_map.h"

namespace insieme {
namespace core {
namespace lang {

template<typename T>
bool isSigned() {
	T x = 0;
	return ((T)(x - 1)) < x;
}

TEST(LangBasic, Generation) {

	EXPECT_EQ(1, (type_map<INT, 1>::bits));
	
	EXPECT_EQ(static_cast<std::size_t>(1), sizeof(type_map<INT,1>::value_type));
	EXPECT_EQ(static_cast<std::size_t>(2), sizeof(type_map<INT,2>::value_type));
	EXPECT_EQ(static_cast<std::size_t>(4), sizeof(type_map<INT,4>::value_type));
	EXPECT_EQ(static_cast<std::size_t>(8), sizeof(type_map<INT,8>::value_type));
	
	EXPECT_EQ(static_cast<std::size_t>(1), sizeof(type_map<UINT,1>::value_type));
	EXPECT_EQ(static_cast<std::size_t>(2), sizeof(type_map<UINT,2>::value_type));
	EXPECT_EQ(static_cast<std::size_t>(4), sizeof(type_map<UINT,4>::value_type));
	EXPECT_EQ(static_cast<std::size_t>(8), sizeof(type_map<UINT,8>::value_type));
	
	EXPECT_EQ(static_cast<std::size_t>(4), sizeof(type_map<REAL,4>::value_type));
	EXPECT_EQ(static_cast<std::size_t>(8), sizeof(type_map<REAL,8>::value_type));
	
	EXPECT_TRUE((isSigned<type_map<INT,1>::value_type>()));
	EXPECT_TRUE((isSigned<type_map<INT,2>::value_type>()));
	EXPECT_TRUE((isSigned<type_map<INT,4>::value_type>()));
	EXPECT_TRUE((isSigned<type_map<INT,8>::value_type>()));
	
	EXPECT_FALSE((isSigned<type_map<UINT,1>::value_type>()));
	EXPECT_FALSE((isSigned<type_map<UINT,2>::value_type>()));
	EXPECT_FALSE((isSigned<type_map<UINT,4>::value_type>()));
	EXPECT_FALSE((isSigned<type_map<UINT,8>::value_type>()));
	
	EXPECT_TRUE((isSigned<type_map<REAL,4>::value_type>()));
	EXPECT_TRUE((isSigned<type_map<REAL,8>::value_type>()));
}

} // end namespace lang
} // end namespace core
} // end namespace insieme

