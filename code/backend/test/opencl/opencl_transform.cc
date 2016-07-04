/**
 * Copyright (c) 2002-2015 Distributed and Parallel Systems Group,
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

#include "insieme/backend/addons/pointer_type.h"
#include "insieme/backend/opencl/opencl_transform.h"

#include "insieme/backend/converter.h"
#include "insieme/backend/type_manager.h"
#include "insieme/backend/type_manager.h"
#include "insieme/backend/c_ast/c_ast_printer.h"

#include "insieme/core/ir_builder.h"
#include "insieme/core/lang/pointer.h"

namespace insieme {
namespace backend {
namespace opencl {

	using namespace core;
	using namespace core::lang;
	using namespace insieme::backend::opencl::transform;

	TEST(DISABLED_inlineAssignments, Basic) {

		NodeManager mgr;
		IRBuilder builder(mgr);

		/*
		 * this is taken from the frontend test input
		 * #pragma test expect_ir("{ var ref<int<4>,f,f> v1 = ref_var_init(1); c_style_assignment(v1, *v1+1); }")
		 * {
		 * 	int a = 1;
		 *	a += 1;
		 * }
		 */
		IRBuilder::EagerDefinitionMap symbols;
		symbols["c_style_assignment"] = builder.parseExpr("(lhs : ref<'a,f,'b>, rhs : 'a) -> 'a { lhs = rhs; return *lhs; }");
		
		StatementPtr stmt = builder.parseStmt("{ var ref<int<4>,f,f> v1 = ref_var_init(1); c_style_assignment(v1, *v1+1); }", symbols);
		// EXPECT_EQ("{ var ref<int<4>,f,f> v1 = ref_var_init(1); ref_assign(v1, *v1+1); }", toString(*inlineAssignments(mgr, stmt)));
	}


} // end namespace opencl
} // end namespace backend
} // end namespace insieme
