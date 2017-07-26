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

#include <gtest/gtest.h>

#include "insieme/core/ir_program.h"
#include "insieme/core/ir_visitor.h"
#include "insieme/core/ir_builder.h"

#include "insieme/core/transform/node_replacer.h"

using namespace insieme::core;
using namespace insieme::core::transform;

TEST(IRVisitor, NodeReplacementTest) {
	// copy and clone the type
	NodeManager manager;
	IRBuilder builder(manager);

	GenericTypePtr type = builder.genericType("int");

	LiteralPtr toReplace = builder.literal(type, "14");
	LiteralPtr replacement = builder.literal(type, "0");

	IfStmtPtr ifStmt = builder.ifStmt(builder.literal(type, "12"), toReplace, builder.compoundStmt());

	NodePtr newTree = transform::replaceAll(builder.getNodeManager(), ifStmt, toReplace, replacement);
	EXPECT_EQ(newTree, builder.ifStmt(builder.literal(type, "12"), replacement, builder.compoundStmt()));
}
