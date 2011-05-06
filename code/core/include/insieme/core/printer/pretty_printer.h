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

#include <iostream>
#include <map>

#include "insieme/core/ast_node.h"

namespace insieme {
namespace core {
namespace printer {

/**
 * A struct representing a pretty print of a AST subtree. Instances may be streamed
 * into an output stream to obtain a readable version of an AST.
 */
struct PrettyPrinter {

	/**
	 * A list of options to adjust the print.
	 */
	enum Option {

		PRINT_DEREFS 			= 1<<0,
		PRINT_CASTS  			= 1<<1,
		PRINT_BRACKETS  		= 1<<2,
		PRINT_SINGLE_LINE		= 1<<3,
		PRINT_MARKERS			= 1<<4,
		NO_EXPAND_LAMBDAS       = 1<<5

	};

	/**
	 * An default setup resulting in a readable print out.
	 */
	static const unsigned OPTIONS_DEFAULT;

	/**
	 * An option to be used for a maximum of details.
	 */
	static const unsigned OPTIONS_DETAIL;

	/**
	 * An option to be used for a single-line print.
	 */
	static const unsigned OPTIONS_SINGLE_LINE;

	/**
	 * The root node of the sub-try to be printed
	 */
	const NodePtr& root;

	/**
	 * The flags set for customizing the formating
	 */
	unsigned flags;

	/**
	 * The maximum number of levels to be printed
	 */
	unsigned maxDepth;

	/**
	 * Sets the tab separator  
	 */
	std::pair<char, size_t> tabSep;

	/**
	 * Creates a new pretty print instance.
	 *
	 * @param root the root node of the AST to be printed
	 * @param flags options allowing users to customize the output
	 * @param maxDepth the maximum recursive steps the pretty printer is descending into the given AST
	 */
	PrettyPrinter( const NodePtr& root, unsigned flags = OPTIONS_DEFAULT, 
			       unsigned maxDepth = std::numeric_limits<unsigned>::max(),
				   std::pair<char,size_t> tabSep=std::make_pair(' ', 4) )
			: root(root), flags(flags), maxDepth(maxDepth), tabSep(tabSep) { }

	/**
	 * Tests whether a certain option is set or not.
	 *
	 * @return true if the option is set, false otherwise
	 */
	bool hasOption(Option option) const;

	/**
	 * Updates a format option for the pretty printer.
	 *
	 * @param option the option to be updated
	 * @param status the state this option should be set to
	 */
	void setOption(Option option, bool status = true);

};

// Represents the lineNo:columnNo pair used to identify a position in the generated output IR source file
typedef std::pair<size_t, size_t> SourceLocation;

// Represents a range start : end which identify the upper and lower bounds of source code generated by 
// an IR node 
typedef std::pair<SourceLocation, SourceLocation> SourceRange;

// Map use to map code ranges of the generated IR representation to the corresponding  
// IR node. (to use NodeAddress in the future maybe)
typedef std::map<SourceRange, NodePtr> SourceLocationMap;

// Prints the IR to the output stream and in parallel builds the map which associates positions 
// in the generated stream to IR nodes 
SourceLocationMap printAndMap( std::ostream& out, const insieme::core::printer::PrettyPrinter& print );

} // end of namespace printer
} // end of namespace core
} // end of namespace insieme

namespace std {

	/**
	 * Allows pretty prints to be directly printed into output streams.
	 */
	std::ostream& operator<<(std::ostream& out, const insieme::core::printer::PrettyPrinter& print);

	std::ostream& operator<<(std::ostream& out, const  insieme::core::printer::SourceLocation& loc) {
		return out << loc.first << ":" << loc.second;	
	}

	std::ostream& operator<<(std::ostream& out, const  insieme::core::printer::SourceRange& range) {
		return out << "[" << range.first << " - " << range.second << "]";
	}
	
	std::ostream& operator<<(std::ostream& out, const  insieme::core::printer::SourceLocationMap& srcMap);

}
