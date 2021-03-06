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

#include "insieme/backend/variable_manager.h"

#include "insieme/core/ir_expressions.h"
#include "insieme/core/lang/reference.h"

#include "insieme/backend/c_ast/c_ast_utils.h"
#include "insieme/backend/converter.h"
#include "insieme/backend/name_manager.h"
#include "insieme/backend/type_manager.h"

#include "insieme/utils/logging.h"

namespace insieme {
namespace backend {

	const VariableInfo& VariableManager::getInfo(const core::VariablePtr& var) const {
		// find requested variable within info
		auto pos = infos.find(var);
		if(pos != infos.end()) { return pos->second; }

		LOG(FATAL) << "Requesting info for unknown variable " << *var << " of type " << *var->getType() << "!!!" << "\nVM: " << this;

		assert(pos != infos.end() && "Requested variable infos for unknown variable!");
		return pos->second;
	}

	const VariableInfo& VariableManager::addInfo(ConversionContext& context, const core::VariablePtr& var, VariableInfo::MemoryLocation location) {
		const Converter& converter = context.getConverter();
		// forward call more detailed implementation
		return addInfo(context, var, location, converter.getTypeManager().getTypeInfo(context, var->getType()));
	}

	const VariableInfo& VariableManager::addInfo(ConversionContext& context, const core::VariablePtr& var, VariableInfo::MemoryLocation location,
	                                             const TypeInfo& typeInfo) {
		const Converter& converter = context.getConverter();

		// create new variable info instance (implicit)
		VariableInfo& info = infos[var];

		// obtain type info
		info.typeInfo = &typeInfo;

		// obtain name and type of the variable
		c_ast::TypePtr type = (location == VariableInfo::DIRECT) ? info.typeInfo->lValueType : info.typeInfo->rValueType;
		c_ast::IdentifierPtr name = converter.getCNodeManager()->create(converter.getNameManager().getName(var));

		info.var = c_ast::var(type, name);

		// if var is cpp ref/rref, it's always indirect
		if(core::lang::isCppReference(var) || core::lang::isCppRValueReference(var)) {
			location = VariableInfo::NONE;
		}
		info.location = location;

		return info;
	}


	void VariableManager::remInfo(const core::VariablePtr& var) {
		// just delete from internal map
		infos.erase(var);
	}

} // end namespace backend
} // end namespace insieme
