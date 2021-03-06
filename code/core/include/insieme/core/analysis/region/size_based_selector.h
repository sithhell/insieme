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

#pragma once

#include "insieme/core/analysis/region/region_selector.h"

namespace insieme {
namespace core {
namespace analysis {
namespace region {

	/**
	 * This region selector is picking regions based on a estimated computation
	 * cost model.
	 */
	class SizeBasedRegionSelector : public RegionSelector {
		/**
		 * The lower limit for the cost of a code fragment to be classified
		 * as a region.
		 */
		unsigned minSize;

		/**
		 * The upper bound for the estimated costs a code fragment can have
		 * to be classified as a region.
		 */
		unsigned maxSize;

	  public:
		/**
		 * Creates a new selector identifying regions having a estimated execution
		 * cost within the given boundaries.
		 */
		SizeBasedRegionSelector(unsigned minSize, unsigned maxSize) : minSize(minSize), maxSize(maxSize) {}

		/**
		 * Selects all regions within the given code fragment.
		 */
		virtual RegionList getRegions(const core::NodeAddress& code) const;
	};

} // end namespace region
} // end namespace analysis
} // end namespace core
} // end namespace insieme
