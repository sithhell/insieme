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

#include "insieme/core/ast_node.h"
#include "insieme/core/expressions.h"

#include "insieme/simple_backend/code_management.h"
#include "insieme/simple_backend/type_manager.h"
#include "insieme/simple_backend/name_manager.h"

#include "insieme/utils/map_utils.h"

namespace insieme {
namespace simple_backend {

using namespace insieme::core;

class Converter;


class FunctionManager {
protected:
	/**
	 * The conversion context this manager is part of.
	 */
	Converter& cc;

	/**
	 *  
	 */

	bool createWrapper;
	
	/**
	 * A map linking lambda nodes to prototype declarations within the program code.
	 */
	utils::map::PointerMap<core::LambdaPtr, CodeFragmentPtr> prototypes;

	/**
	 * A map linking Lambda expressions (recursive or non-recursive) to closure definitions and
	 * functions.
	 */
	utils::map::PointerMap<core::LambdaPtr, CodeFragmentPtr> functions;

	/**
	 * A map linking external function literals to their prototypes.
	 */
	utils::map::PointerMap<core::LiteralPtr, CodeFragmentPtr> externalFunctions;

	/**
	 * A set accumulating all handled lambda definitions.
	 */
	utils::map::PointerMap<core::LambdaDefinitionPtr, CodeFragmentPtr> functionGroup;

	/**
	 * A map linking bind expressions to a code fragment referencing the associated closures
	 * and functions.
	 */
	utils::map::PointerMap<core::BindExprPtr, CodeFragmentPtr> binds;

public:

	/**
	 * A constructor for this manager being initialized with the conversion context.
	 */
	FunctionManager(Converter& conversionContext, bool wrapper = true) : cc(conversionContext), createWrapper(wrapper) { }

	/**
	 * A virtual destructor introduced to support proper sub-classing.
	 */
	virtual ~FunctionManager() {};

	/**
	 * Appends the name of the external function to the given context.
	 *
	 * @param context the code fragment the given external function call should be appended to.
	 * @param external the literal representing the external function.
	 */
	string getFunctionName(const CodeFragmentPtr& context, const core::LiteralPtr& external);

	string getFunctionName(const CodeFragmentPtr& context, const core::LambdaExprPtr& lambda);

	// TODO: add the same signature for lambdas and literals
	void createClosure(const CodeFragmentPtr& target, const core::BindExprPtr& bind);

protected:
	virtual void appendFunctionParameter(const CodeFragmentPtr& fragment, const VariablePtr& param);
	virtual void appendFunctionParameters(const CodeFragmentPtr& fragment, const vector<VariablePtr>& params);

	virtual void addFunctionPrefix(const CodeFragmentPtr& fragment, const core::LambdaPtr& lambda) { }

	virtual CodeFragmentPtr resolve(const LambdaDefinitionPtr& definition);
	virtual CodeFragmentPtr resolve(const LambdaPtr& lambda);

	virtual CodeFragmentPtr resolve(const LiteralPtr& literal);

	virtual CodeFragmentPtr resolve(const BindExprPtr& bind);
};


}
}
