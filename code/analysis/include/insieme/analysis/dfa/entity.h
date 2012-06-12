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

#include <vector>
#include <map>
#include <tuple>

#include "insieme/utils/map_utils.h"
#include "insieme/utils/set_utils.h"
#include "insieme/core/ir_expressions.h"

#include "insieme/core/ir_visitor.h"

#include "insieme/analysis/cfg.h"

namespace insieme { 
namespace analysis { 
namespace dfa {

/** 
 * An Entity represents the unit of information examined by a dataflow problem. 
 *
 * An entity could be of various form: expressions, variables, control flow blocks, etc... 
 *
 * Not always an entity represent a physical node of the IR, the concept is more abstract. 
 * For this reason each entity should describe the domain of values which can be assumed by 
 * an instance of an entity and how this entity is extracted starting from a generic CFG.
 */
template <class... T>
class Entity {

	std::string description;

public:
	Entity(const std::string& description="") : description(description) { }

	/**
	 * Return the arity of this entity 
	 */
	constexpr static size_t arity() { return sizeof...(T); }

};


/**
 * Utility functions to create an entity 
 */
template <class T>
Entity<T> makeAtomicEntity(const std::string& description=std::string()) { 
	return Entity<T>(description); 
}

template <class... T>
Entity<T...> makeCompoundEntity(const std::string& description=std::string()) {
	return Entity<T...>(description);
}

template <class...T>
struct container_type_traits {
	typedef std::tuple< typename container_type_traits<T>::type... > type;
};

template <class T>
struct container_type_traits<T> {
	typedef std::set<T> type;
};

template <class T>
struct container_type_traits<core::Pointer<const T>> {
	typedef utils::set::PointerSet<core::Pointer<const T>> type;
};

template <class T>
struct container_type_traits<core::Address<const T>> {
	typedef std::set<core::Address<const T>> type;
};

/**
 * Extract the set of entities existing in the given CFG. 
 *
 * Every entity needs to specialize the extract method for that kind of entity,
 * compound entityes can be extracted by combining the results of the value obtained by extracting
 * the single entities 
 */
template <class... E>
typename container_type_traits<E...>::type extract(const Entity<E...>& e, const CFG& cfg) {
	return std::make_tuple( extract(Entity<E>(),cfg)... );
}

/**
 * Generic extractor for IR entities 
 *
 * IR entities (NodePtrs) can be extracted via this specialization of the extract method 
 */
template <class IRE, template <class> class Cont=core::Pointer>
typename container_type_traits<Cont<const IRE>>::type 
extract(const Entity<Cont<const IRE>>& e, const CFG& cfg) {
	
	typedef typename container_type_traits<Cont<const IRE>>::type Container;

	Container entities;

	std::function<void (const cfg::BlockPtr&, std::tuple<Container&>&)> collector =
		[] (const cfg::BlockPtr& block, std::tuple<Container&>& vec) {

			auto visitor = core::makeLambdaVisitor(
				[] (const Cont<const IRE>& var, std::tuple<Container&>& vec) { 
					std::get<0>(vec).insert( var );
			}, true);

			for_each(block->stmt_begin(), block->stmt_end(), [&] (const Cont<const core::Statement>& cur) {
				auto v = makeDepthFirstVisitor( visitor );
				v.visit(cur, vec);
			});
		};

	cfg.visitDFS(collector, entities);

	return entities;
}

} } } // end insieme::analysis::dfa
