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

#include "insieme/core/forward_decls.h"
#include "insieme/utils/printable.h"
#include "insieme/utils/set_utils.h"
#include <vector>
#include <set>
#include <memory>

#include "boost/operators.hpp"

namespace insieme {

namespace core {
typedef std::vector<StatementPtr> StatementList;
typedef insieme::utils::set::PointerSet<StatementPtr> StatementSet;
}// end core namespace 

namespace analysis {

/** 
 * Class Ref represent a generic IR ref which can be either assigned or read. In this context 
 * a Ref can be either a scalar variable, an array or a vector (having a ref type), a struct/class
 * member or the return value of a call expression returning a ref. 
 */
struct Ref : public utils::Printable {

	// possible usage of a variable can be of three types: 
	// 	USE: the variable is being accessed, therefore the memory location is read and not modified 
	// 	DEF: the variable is being redefined (declaration and assignment), this means that the
	// 	     memory associated to that variable is being written 
	// 	UNKNOWN: the variable is being used as input parameter to a function which can either read
	// 	         or modify the value. UNKNOWN type of usages can be refined through advanced dataflow
	// 	         analysis
	// 	ANY: utilized in the RefList class to iterate through any type of usage
	enum UseType { ANY_USE=-1, DEF, USE, UNKNOWN };

	// Possible type of references are:
	// VAR:    reference to scalar variables 
	// ARRAY:  reference to arrays 
	// CALL:   return value of a function returning a reference 
	// ANY:    used in the RefList class in order to iterate through any reference type 
	enum RefType { ANY_REF=-1, VAR, ARRAY, MEMBER, CALL };

	Ref(const RefType& type, const core::ExpressionPtr& var, const UseType& usage = USE);

	std::ostream& printTo(std::ostream& out) const;

	const UseType& getUsage() const { return usage; }
	
	const RefType& getType() const { return type; }

private:
	// Define the type of this reference 
	RefType type;

	// Points to the base expression: 
	// 	 this can be either a scalar variable, an array or a call to a function 
	core::ExpressionPtr baseExpr;

	// Define the use for this expression  
	UseType usage; 
};

// In the case of arrays (or vectors), we also store the list of expressions used to index each of the
// array dimensions
struct ArrayRef : public Ref { 
	
	typedef std::vector<core::ExpressionPtr> ExpressionList;  

	ArrayRef(const core::ExpressionPtr& arrayVar, const ExpressionList& idxExpr, const UseType& usage = USE) :
		Ref(Ref::ARRAY, arrayVar, usage), idxExpr(idxExpr) { }

	std::ostream& printTo(std::ostream& out) const;	

	const ExpressionList& getIndexExpressions() const { return idxExpr; }

private:
	ExpressionList idxExpr;
};

typedef std::shared_ptr<Ref> RefPtr; 

// Store the set of refs found by the visitor 
class RefList: public std::vector<RefPtr> {
	
public:
	class ref_iterator : public boost::forward_iterator_helper<ref_iterator, const Ref> { 
		
		RefList::const_iterator it, end;
		Ref::RefType type;
		Ref::UseType usage; 

		void inc(bool first=false);
	public:
		ref_iterator(RefList::const_iterator begin, RefList::const_iterator end, 
				Ref::RefType type=Ref::ANY_REF, Ref::UseType usage=Ref::ANY_USE) : 
			it(begin), end(end), type(type), usage(usage) { inc(true); }

		const Ref& operator*() const { assert(it != end && "Iterator out of bounds"); return **it; }	
		iterator& operator++() { inc(); return *this; }

		bool operator==(const ref_iterator& rhs) const { return it == rhs.it && usage == rhs.usage && type == rhs.type; }
	};

	RefList::ref_iterator arrays_begin(const Ref::UseType& usage=Ref::ANY_USE) { return ref_iterator(begin(), end(), Ref::ARRAY, usage); }
	RefList::ref_iterator arrays_end(const Ref::UseType& usage=Ref::ANY_USE) { return ref_iterator(end(), end(), Ref::ARRAY, usage); }

	RefList::ref_iterator vars_begin(const Ref::UseType& usage=Ref::ANY_USE) { return ref_iterator(begin(), end(), Ref::VAR, usage); }
	RefList::ref_iterator vars_end(const Ref::UseType& usage=Ref::ANY_USE) { return ref_iterator(end(), end(), Ref::VAR, usage); }

	// add iterators for members/callexpr
};

// Main entry method, visits the IR starting from the root node collecting refs. The list of
// detected refs is returned to the caller.
//
// The method also takes a set of statements which should not be skipped during the analysis. This
// is useful in the case the def-use analysis has been performed on a sub tree of the current root
// and we want to perform the analysis only on the remaining part of the tree 
RefList collectDefUse(const core::NodePtr& root, const core::StatementSet& skipList = core::StatementSet());

} // end namespace analysis 
} // end namesapce insieme 
