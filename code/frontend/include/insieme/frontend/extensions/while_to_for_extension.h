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

#pragma once

#include <string>
#include <vector>

#include "insieme/core/printer/pretty_printer.h"
#include "insieme/frontend/extensions/frontend_extension.h"

namespace printer=insieme::core::printer;

namespace insieme {
namespace frontend {
namespace extensions {

/// NodeBookmark allows to capture the status related to a list of nodes, by allowing to store the list of nodes
/// together with some integer value and an error msg, for passing around and later processing.
class NodeBookmark {
public:
	/// a vector of nodes which should be preserved for further actions
	std::vector<insieme::core::NodeAddress> nodes;
	/// an integer value associated with these nodes
	int value;
	/// an error that has been encountered while trying to compile the list of bookmarks
	std::string msg;
	
	/// constructor to initialize the data structures (value:=0)
	NodeBookmark(): value(0) {}
	
	/// returns true if no error has been set yet; also see err()
	bool ok() {
		return msg.empty();
	}
	/// err with this empty signature returns true in case at least one error has been encountered
	bool err() {
		return !ok();
	}
	/// use err to set an error: err comes in handy in case we want to preserve the first error encountered, as it will ignore further calls to err
	void err(std::string s) {
		if(msg.empty()) {
			msg=s;
		}
	}
	/// merge two NodeBookmarks so that the nodes themselves are appended, and the integer value is added
	void merge(NodeBookmark b) {
		if(err()) {
			return;
		}
		err(b.msg);
		value+=b.value;
		nodes.reserve(nodes.size()+b.nodes.size());
		nodes.insert(nodes.end(), b.nodes.begin(), b.nodes.end());
	}
	/// prependPath will prefix the internally saved nodes with the node root given in newroot
	void prependPath(insieme::core::NodeAddress newroot) {
		std::vector<insieme::core::NodeAddress> m;
		for(auto n: nodes) {
			m.push_back(newroot >> n);
		}
		nodes=m;
	}
};

/**
 *
 * This extension converts suitable while loops generated by the frontend to for loops.
 */
class WhileToForExtension: public insieme::frontend::extensions::FrontendExtension {
public:
	virtual insieme::core::ProgramPtr IRVisit(insieme::core::ProgramPtr& prog);
protected:
	insieme::utils::set::PointerSet<core::VariablePtr> extractCondVars(std::vector<core::NodeAddress> cvars);
	std::vector<core::NodeAddress> getAssignmentsForVar(core::NodeAddress body, core::VariablePtr var);
	NodeBookmark extractStepFromAssignment(core::Address<const core::Node> a);
	NodeBookmark extractStepForVar(core::NodeAddress body, core::VariablePtr var);
	NodeBookmark extractInitialValForVar(core::NodeAddress loop, core::VariablePtr var);
	NodeBookmark extractTargetValForVar(core::NodeAddress cond, core::VariablePtr var);
	core::ProgramPtr replaceWhileByFor(core::NodeAddress whileaddr,
	                                   NodeBookmark initial, NodeBookmark target, NodeBookmark step);
private:
	printer::PrettyPrinter pp(const core::NodePtr& n);
	unsigned int maxDepth(const insieme::core::Address<const insieme::core::Node> n);
	void printNodes(const insieme::core::Address<const insieme::core::Node> n,
	                std::string prefix, int max, unsigned int depth);
};

} // namespace extensions
} // frontend
} // insieme
