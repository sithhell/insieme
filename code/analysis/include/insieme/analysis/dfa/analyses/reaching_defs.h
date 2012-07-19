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

#include "insieme/analysis/dfa/entity.h"
#include "insieme/analysis/dfa/problem.h"

#include "insieme/analysis/dfa/analyses/ir_var_entity.h"

namespace insieme { namespace analysis { namespace dfa { 
/**
 * Define the extractor for CFG Blocks. In this case we extract the address of
 * the CFG Blocks (an alternative would be to store the block ID)
 */
template <>
inline typename container_type_traits< dfa::elem<cfg::BlockPtr> >::type 
extract(const Entity< elem<cfg::BlockPtr> >& e, const CFG& cfg) {
	
	typedef typename container_type_traits< dfa::elem<cfg::BlockPtr> >::type Container;

	Container entities;
	auto collector = [&entities] (const cfg::BlockPtr& block) { entities.insert( block ); };
	cfg.visitDFS(collector);

	return entities;
}

namespace analyses {


/**
 * Define the DataFlow problem for Reaching Definitions 
 *
 * The Dataflow problem is defined on the powerset of the variables contained in
 * the code segment. 
 *
 * The TOP element of the generated lattice is the set of variables, while the
 * BOTTOM is the empty set 
 *
 * The MEET operator is the intersection operation
 */
class ReachingDefinitions: 
	public Problem<
				ReachingDefinitions, 
				ForwardAnalysisTag, 
				Entity< dfa::elem<analyses::VarEntity>, dfa::elem<cfg::BlockPtr> >, 
				PowerSet
			> 
{

	typedef Problem<
				ReachingDefinitions, 
				ForwardAnalysisTag, 
				Entity< dfa::elem<analyses::VarEntity>, dfa::elem<cfg::BlockPtr> >, 
				PowerSet
			> Base;
	
public:

	typedef typename Base::direction_tag direction_tag;

	typedef typename Base::value_type value_type;

	ReachingDefinitions(const CFG& cfg): Base(cfg) { }

	inline value_type init() const { return top(); }

	inline value_type top() const { 
		// the top element is the set of all variable present in the program
		return typename Base::value_type();
	}

	inline value_type bottom() const {
		// the bottom element is the empty set 
		const auto& lhsBase = extracted.getLeftBaseSet();
		return makeCartProdSet(
				lhsBase, 
				std::set<cfg::BlockPtr>( { cfg.getBlockPtr(cfg.entry()) } ) 
			).expand();
	}

	value_type meet(const value_type& lhs, const value_type& rhs) const;

	value_type transfer_func(const value_type& in, const cfg::BlockPtr& block) const;

};

} } } } // end insieme::analysis::dfa::analyses namespace 
