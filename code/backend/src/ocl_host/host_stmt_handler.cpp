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

#include "insieme/core/analysis/ir_utils.h"

#include "insieme/backend/converter.h"
#include "insieme/backend/type_manager.h"
#include "insieme/backend/variable_manager.h"

#include "insieme/backend/ocl_host/host_stmt_handler.h"
#include "insieme/backend/ocl_host/host_code_fragments.h"

#include "insieme/backend/ocl_kernel/kernel_extensions.h"

#include "insieme/backend/c_ast/c_code.h"
#include "insieme/backend/c_ast/c_ast_utils.h"
#include "insieme/backend/c_ast/c_ast_printer.h"

#include "insieme/utils/logging.h"

namespace insieme {
namespace backend {
namespace ocl_host {

	namespace {

		c_ast::NodePtr createKernelCall(ConversionContext& context, const core::CallExprPtr& call) {

			static const char* codeTemplate = "\n\n"
					"irt_ocl_kernel* kernel = &irt_context_get_current()->kernel_binary_table[0][%1$d];\n"
					"irt_ocl_set_kernel_ndrange(kernel, 1, irt_ocl_get_global_size(%1$d), irt_ocl_get_local_size(%1$d));\n"
					"irt_ocl_run_kernel(kernel, %2$d%3$s)";

			const Converter& converter = context.getConverter();
			StmtConverter& stmtConverter = converter.getStmtConverter();

			// register kernel
			KernelCodeTablePtr table = KernelCodeTable::get(converter);
			context.addDependency(table);

			unsigned kernelID = table->registerKernel(call->getFunctionExpr());
			unsigned numArgs = call->getArguments().size();

			const ocl_kernel::Extensions& ext = context.getConverter().getNodeManager().getLangExtension<ocl_kernel::Extensions>();

			std::stringstream argList;
			for_each(call->getArguments(), [&](const core::ExpressionPtr& cur) {
				c_ast::ExpressionPtr arg = stmtConverter.convertExpression(context, cur);
				if (ext.isWrapperType(cur->getType())) {
					argList << ", (size_t)0";
				} else {
					argList << ", " << c_ast::toC(c_ast::sizeOf(stmtConverter.convertType(context, cur->getType())));
				}
				argList << ", " << c_ast::toC(arg);
			});

			return converter.getCNodeManager()->create<c_ast::Literal>(format(codeTemplate, kernelID, numArgs, argList.str().c_str()));
		}


		c_ast::NodePtr handleStmts(ConversionContext& context, const core::NodePtr& node) {

			const ocl_kernel::Extensions& ext = context.getConverter().getNodeManager().getLangExtension<ocl_kernel::Extensions>();

			// filter out kernel calls => need to be treated differently
			if (node->getNodeType() == core::NT_CallExpr) {
				const core::CallExprPtr& call = static_pointer_cast<const core::CallExpr>(node);
				if (core::analysis::isCallOf(call->getFunctionExpr(), ext.kernelWrapper)) {
					return createKernelCall(context, call);
				}
			}

			// let somebody else encode it ..
			return c_ast::NodePtr();
		}

	}

	StmtHandler OpenCLStmtHandler = &handleStmts;

} // end namespace ocl_kernel
} // end namespace backend
} // end namespace insieme
