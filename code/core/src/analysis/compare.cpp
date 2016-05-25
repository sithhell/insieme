/**
 * Copyright (c) 2002-2016 Distributed and Parallel Systems Group,
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

#include "insieme/core/analysis/compare.h"

#include "insieme/core/ir_visitor.h"
#include "insieme/utils/map_utils.h"
#include "insieme/utils/logging.h"
#include "insieme/utils/string_utils.h"

#include "insieme/core/ir_builder.h"
#include "insieme/core/transform/node_replacer.h"

namespace insieme {
namespace core {
namespace analysis {

	namespace {

		NodePtr wipeNames(const NodePtr& node) {

			struct Annotation {
				NodePtr cleaned;
				bool operator==(const Annotation& other) const {
					return cleaned == other.cleaned;
				}
			};

			// check the annotation
			if (node->hasAttachedValue<Annotation>()) {
				return node->getAttachedValue<Annotation>().cleaned;
			}

			// compute a cleaned version
			std::map<NodePtr,NodePtr> map;
			std::map<NodeAddress,NodePtr> replacements;
			unsigned id = 0;
			NodeManager& mgr = node->getNodeManager();
			IRBuilder builder(mgr);
			visitDepthFirstPrunable(NodeAddress(node),[&](const NodeAddress& cur)->Action {

				// skip root node
				if (cur.getAddressedNode() == node) return Action::Descent;

				// check some cases
				if (auto ref = cur.isa<LambdaReferencePtr>()) {

					auto& replacement = map[ref];
					if (!replacement) replacement = builder.lambdaReference(ref->getType(), format("l_ref_%d", ++id));
					replacements[cur.as<LambdaReferenceAddress>()->getName()] = replacement.as<LambdaReferencePtr>()->getName();
					return Action::Descent;

				} else if (auto ref = cur.isa<TagTypeReferencePtr>()) {

					auto& replacement = map[ref];
					if (!replacement) replacement = builder.tagTypeReference(format("t_ref_%d", ++id));
					replacements[cur] = replacement;
					return Action::Prune;

				} else if (auto type = cur.isa<TagTypePtr>()) {

					auto clean = wipeNames(type);
					if (*type != *clean) {
						replacements[cur] = clean;
					}
					return Action::Prune;

				}

				// continue searching
				return Action::Descent;
			},true);


			// conduct replacement
			auto cleaned = (replacements.empty()) ? node : core::transform::replaceAll(mgr, replacements);

			// attach and return
			node->attachValue(Annotation{cleaned});
			return cleaned;
		}

	}

	bool equalNameless(const NodePtr& nodeA, const NodePtr& nodeB) {
		return *nodeA == *nodeB || *wipeNames(nodeA) == *wipeNames(nodeB);
	}

} // namespace analysis
} // namespace core
} // namespace insieme
