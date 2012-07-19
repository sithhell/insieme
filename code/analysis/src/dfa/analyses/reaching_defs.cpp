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

#include "insieme/analysis/dfa/analyses/reaching_defs.h"

#include "insieme/core/analysis/ir_utils.h"

#include "insieme/utils/logging.h"

namespace insieme { namespace analysis { namespace dfa { namespace analyses {

/**
 * ReachingDefinitions Problem
 */
typename ReachingDefinitions::value_type 
ReachingDefinitions::meet(const typename ReachingDefinitions::value_type& lhs, const typename ReachingDefinitions::value_type& rhs) const 
{
	// LOG(DEBUG) << "MEET ( " << lhs << ", " << rhs << ") -> ";
	typename ReachingDefinitions::value_type ret;
	std::set_union(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), std::inserter(ret,ret.begin()));
	// LOG(DEBUG) << ret;
	return ret;
}


typename ReachingDefinitions::value_type 
ReachingDefinitions::transfer_func(const typename ReachingDefinitions::value_type& in, const cfg::BlockPtr& block) const {
	typename ReachingDefinitions::value_type gen, kill;
	
	if (block->empty()) { return in; }
	assert(block->size() == 1);

	LOG(DEBUG) << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~";
	LOG(DEBUG) << "~ Block " << block->getBlockID();
	LOG(DEBUG) << "~ IN: " << in;

	for_each(block->stmt_begin(), block->stmt_end(), 
			[&] (const cfg::Element& cur) {

		core::StatementPtr stmt = cur.getAnalysisStatement();

		auto handle_def = [&](const core::VariablePtr& varPtr) { 

			analyses::VarEntity var = analyses::makeVarEntity( core::ExpressionAddress(varPtr) );

			gen.insert( std::make_tuple(var, block) );

			// kill all declarations reaching this block 
			std::copy_if(in.begin(), in.end(), std::inserter(kill,kill.begin()), 
					[&](const typename ReachingDefinitions::value_type::value_type& cur){
						return std::get<0>(cur) == var;
					} );
		};

		if (stmt->getNodeType() == core::NT_Literal) { return; }

		// assume scalar variables 
		if (core::DeclarationStmtPtr decl = core::dynamic_pointer_cast<const core::DeclarationStmt>(stmt)) {

			handle_def( decl->getVariable() );

		} else if (core::CallExprPtr call = core::dynamic_pointer_cast<const core::CallExpr>(stmt)) {

			if (core::analysis::isCallOf(call, call->getNodeManager().getLangBasic().getRefAssign()) ) { 
				handle_def( call->getArgument(0).as<core::VariablePtr>() );
			}

		} else {
			LOG(WARNING) << stmt;
			assert(false && "Stmt not handled");
		}
	});

	LOG(DEBUG) << "~ KILL: " << kill;
	LOG(DEBUG) << "~ GEN:  " << gen;

	typename ReachingDefinitions::value_type set_diff, ret;
	std::set_difference(in.begin(), in.end(), kill.begin(), kill.end(), std::inserter(set_diff, set_diff.begin()));
	std::set_union(set_diff.begin(), set_diff.end(), gen.begin(), gen.end(), std::inserter(ret, ret.begin()));

	LOG(DEBUG) << "~ RET: " << ret;

	return ret;
}

} } } } // end insieme::analysis::dfa::analyses namespace 