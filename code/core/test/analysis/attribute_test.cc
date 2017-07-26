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

#include <vector>

#include <gtest/gtest.h>

#include "insieme/core/ir_builder.h"
#include "insieme/core/ir_address.h"
#include "insieme/core/analysis/attributes.h"

namespace insieme {
namespace core {
namespace analysis {

	TEST(Attributes, Basic) {
		NodeManager manager;
		IRBuilder builder(manager);
		auto& ext = manager.getLangExtension<AttributeExtension>();


		AttributePtr a1 = ext.getUnordered();
		AttributePtr a2 = builder.literal("attr2", ext.getAttributeType());
		AttributePtr a3 = builder.literal("attr3", ext.getAttributeType());

		ExpressionPtr expr = builder.intLit(1);
		ExpressionPtr tmp;

		AttributeSet set;
		EXPECT_EQ(set, getAttributes(expr));
		EXPECT_EQ(set, getAttributes(tmp));

		EXPECT_FALSE(hasAttribute(tmp, a1));
		EXPECT_FALSE(hasAttribute(tmp, a2));
		EXPECT_FALSE(hasAttribute(tmp, a3));


		// add an attribute
		set.insert(a1);
		tmp = addAttribute(expr, a1);
		EXPECT_EQ(set, getAttributes(tmp));
		EXPECT_EQ("attr(1, list_cons(unordered, list_empty(type<attribute>)))", toString(*tmp));

		// add another attribute
		set.insert(a2);
		tmp = addAttribute(tmp, a2);
		EXPECT_EQ(set, getAttributes(tmp));

		// check has-attribute
		EXPECT_TRUE(hasAttribute(tmp, a1));
		EXPECT_TRUE(hasAttribute(tmp, a2));
		EXPECT_FALSE(hasAttribute(tmp, a3));


		// check remove attribute
		set.erase(a2);
		tmp = remAttribute(tmp, a2);
		EXPECT_EQ(set, getAttributes(tmp));

		// check has-attribute
		EXPECT_TRUE(hasAttribute(tmp, a1));
		EXPECT_FALSE(hasAttribute(tmp, a2));
		EXPECT_FALSE(hasAttribute(tmp, a3));
	}

} // end namespace analysis
} // end namespace core
} // end namespace insieme
