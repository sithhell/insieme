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

#include "insieme/backend/addons/io.h"

#include "insieme/backend/converter.h"
#include "insieme/backend/function_manager.h"
#include "insieme/backend/operator_converter.h"

#include "insieme/backend/c_ast/c_ast_utils.h"
#include "insieme/backend/statement_converter.h"

#include "insieme/core/ir_builder.h"
#include "insieme/core/lang/io.h"
#include "insieme/core/lang/pointer.h"


namespace insieme {
namespace backend {
namespace addons {

	namespace {

		/**
		 * A language module defining concrete printf operators
		 */
		class InputOutputExtension : public core::lang::Extension {

			/**
			 * Allow the node manager to create instances of this class.
			 */
			friend class core::NodeManager;

			/**
			 * Creates a new instance based on the given node manager.
			 */
			InputOutputExtension(core::NodeManager& manager) : core::lang::Extension(manager) {}

		public:

			// this extension is based upon the symbols defined by the pointer module
			IMPORT_MODULE(core::lang::PointerExtension);

			// -------------------- basic IO operations ---------------------------

			/**
			 * An operation reading formated input from the command line (scanf)
			 */
			LANG_EXT_LITERAL(Scan, "scanf", "(ptr<char,t,f>, var_list)->int<4>")

			/**
			 * An operation writing formated output to the command line (printf)
			 */
			LANG_EXT_LITERAL(Print, "printf", "(ptr<char,t,f>, var_list)->int<4>")

		};

		OperatorConverterTable getInputOutputOperatorTable(core::NodeManager& manager) {
			OperatorConverterTable res;
			const auto& ext = manager.getLangExtension<core::lang::InputOutputExtension>();

			#include "insieme/backend/operator_converter_begin.inc"

			res[ext.getScan()] =  OP_CONVERTER {
				core::IRBuilder builder(call->getNodeManager());
				const auto& inputOutputExt = call->getNodeManager().getLangExtension<InputOutputExtension>();
				ADD_HEADER("stdio.h");
				return CONVERT_EXPR(builder.callExpr(call->getType(), inputOutputExt.getScan(), call[0], call[1]));
			};
			res[ext.getPrint()] = OP_CONVERTER {
				core::IRBuilder builder(call->getNodeManager());
				const auto& inputOutputExt = call->getNodeManager().getLangExtension<InputOutputExtension>();
				ADD_HEADER("stdio.h");
				return CONVERT_EXPR(builder.callExpr(call->getType(), inputOutputExt.getPrint(), call[0], call[1]));
			};

			#include "insieme/backend/operator_converter_end.inc"

			return res;
		}
	}

	void InputOutput::installOn(Converter& converter) const {
		// register additional operators
		converter.getFunctionManager().getOperatorConverterTable().insertAll(getInputOutputOperatorTable(converter.getNodeManager()));
	}

} // end namespace addons
} // end namespace backend
} // end namespace insieme
