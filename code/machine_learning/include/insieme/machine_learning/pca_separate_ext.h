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

#include "insieme/machine_learning/machine_learning_exception.h"
#include "insieme/machine_learning/pca_extractor.h"

namespace insieme {
namespace ml {

class PcaSeparateExt : public PcaExtractor {
	/*
	 * query to query for dynamic features
	 */
	std::string dynamicQuery;

	/*
	 * calculates the pca for the static code features or dynamic setup features
	 * @param toBeCovered the percentage of variance that should be covered by the PCs
	 * @param dynamic a flag indicating whether the dynamic (true) or static (false) features should be evaluated
	 * @return the number of PCs generated
	 */
	size_t calcSpecializedPca(double toBeCovered, bool dynamic);

	/*
	 * calculates the principal components of static features based on the given query and stores them in the database
	 * @param nInFeatures the number of features to be analyzed/combined
	 * @param nOutFeatures the number to which the features should be reduced
	 * @param dynamic a flag indicating whether the dynamic (true) or static (false) features should be evaluated
	 * @return the percentabe of the variance covered by the first nOutFeatures PCs
	 */
	virtual double calcSpecializedPca(size_t nInFeatures, size_t nOutFeatures, bool dynamic);

public:
	/*
	 * constructor specifying the number of (original) input classes and (reduced) output classes
	 * @param myDbPath the path to the database to read from and write the PCs to
	 * @param manglingPostfix a postfix that will be added to every feature name in order to distinguish this PCs from others
	 */
	PcaSeparateExt(const std::string& myDbPath, std::string manglingPostfix = "")
		: PcaExtractor(myDbPath, manglingPostfix) {}

	/*
	 * generates the default query, querying for all static features which share a common cid and have been specified
	 * using setStaticFeatures before
	 * The first n columns of the query must contain the values of the n features, the n+1 column must hold the [c|s]id
	 * The rows represent features of different codes/setups
	 */
	virtual void genDefaultQuery();

	/*
	 * generates the default query, querying for all dynamic features which share a common sid and have been specified
	 * using setStaticFeatures before
	 */
	void genDefaultDynamicQuery();

	/*
	 * calculates the principal components of static features based on the given query and stores them in the database
	 * @param toBeCovered the percentage of variance that should be covered by the PCs
	 * @return the number of PCs generated
	 */
	virtual size_t calcPca(double toBeCovered);


	/*
	 * calculates the principal components of static features based on the given query and stores them in the database
	 * @param nDynamicOutFeatures the number to which the dynamic features should be reduced (if any dynamic features have been set before)
	 * @param nStaticOutFeatures the number to which the static features should be reduced (if any static features have been set before)
	 * @return the percentage of the variance covered by the first nOutFeatures PCs
	 */
	virtual double calcPca(size_t nDynamicOutFeatures, size_t nStaticOutFeatures);

	/*
	 * sets the query for dynamic features
	 * @param customQuery the query as a string
	 */
	void setDynamicQuery(const std::string customQuery) { query = customQuery; }


};

} // end namespace ml
} // end namespace insieme
