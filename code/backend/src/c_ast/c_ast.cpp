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

#include "insieme/backend/c_ast/c_ast.h"

#include "insieme/utils/container_utils.h"
#include "insieme/utils/functional_utils.h"

namespace insieme {
namespace backend {
namespace c_ast {

	CNodeManager::~CNodeManager() {
		// free all managed nodes
		for_each(nodes, [](const NodePtr cur) { delete &*cur;});
		for_each(identMap, [](const std::pair<string, IdentifierPtr>& cur) { delete &*cur.second; });
	}


	IdentifierPtr CNodeManager::create(const string& name) {
		auto pos = identMap.find(name);
		if (pos != identMap.end()) {
			return pos->second;
		}

		IdentifierPtr res = new Identifier(name);
		res->setManager(this);
		identMap.insert(std::make_pair(name, res));
		return res;
	}

	/**
	 * Defining the equality ID generator.
	 */
	utils::SimpleIDGenerator<Node::EqualityID> Node::equalityClassIDGenerator;


	bool Node::operator==(const Node& other) const {
		// test for identity
		if (this == &other) {
			return true;
		}

		// check node type
		if (type != other.type) {
			return false;
		}

		// check equality ID
		if (equalityID != 0 && other.equalityID != 0 && equalityID == other.equalityID) {
			// just compare equality IDs (having different IDs does not mean it is different)
			return true;
		}

		// use virtual equals method
		bool res = equals(other);

		// infect both nodes with a new ID
		if (res) {
			// update equality IDs - both should have the same id
			if (equalityID == 0 && other.equalityID == 0) {
				// non is set yet => pick a new ID and use for both
				equalityID = equalityClassIDGenerator.getNext();
				other.equalityID = equalityID;
			} else if (equalityID == 0) {
				// other.equalityID != 0 ... update local ID with other ID
				equalityID = other.equalityID;
			} else if (other.equalityID == 0){
				// equality ID != 0 ... update other ID
				other.equalityID = equalityID;
			} else {
				// both are != 0
				assert(equalityID != 0 && other.equalityID != 0 && "Equality IDs should be != 0");

				// pick smaller ID for both
				if (equalityID < other.equalityID) {
					other.equalityID = equalityID;
				} else {
					equalityID = other.equalityID;
				}
			}
		}

		// return the comparison result.
		return res;
	}

	// ----------- Particular Nodes --------------

	/**
	 * This utility struct definition defines a predicate comparing two pairs
	 * of pointers. The pairs are equivalent if the included elements point to equal
	 * objects.
	 *
	 * @tparam T the first pointer type within the pair
	 * @tparam P the second pointer type within the pair
	 */
	template<typename T, typename P>
	struct equal_pointer_pair : public std::binary_function<const std::pair<T,P>&, const std::pair<T,P>&, bool> {
		/**
		 * Performs the actual comparison by using the operator== of the generic
		 * pointer types.
		 *
		 * @param x a reference to the first pair
		 * @param y a reference to the second pair
		 */
		bool operator()(const std::pair<T,P>& x, const std::pair<T,P>& y) const {
			return *x.first == *y.first && *x.second == *y.second;
		}
	};


	bool Identifier::equals(const Node& other) const {
		assert(dynamic_cast<const Identifier*>(&other));
		return name == static_cast<const Identifier&>(other).name;
	}

	bool Comment::equals(const Node& other) const {
		assert(dynamic_cast<const Comment*>(&other));
		return comment == static_cast<const Comment&>(other).comment;
	}

	bool OpaqueCode::equals(const Node& other) const {
		assert(dynamic_cast<const OpaqueCode*>(&other));
		return code == static_cast<const OpaqueCode&>(other).code;
	}

	bool PrimitiveType::equals(const Node& other) const {
		assert(dynamic_cast<const PrimitiveType*>(&other));
		return type == static_cast<const PrimitiveType&>(other).type;
	}

	bool NamedType::equals(const Node& other) const {
		assert(dynamic_cast<const NamedType*>(&other));
		return *name == *static_cast<const NamedType&>(other).name;
	}

	bool PointerType::equals(const Node& other) const {
		assert(dynamic_cast<const PointerType*>(&other));
		return *elementType == *static_cast<const PointerType&>(other).elementType;
	}

	bool VectorType::equals(const Node& node) const {
		assert(dynamic_cast<const VectorType*>(&node));
		auto other = static_cast<const VectorType&>(node);
		return *elementType == *other.elementType && *size == *other.size;
	}

	bool NamedCompositeType::equals(const Node& node) const {
		assert(dynamic_cast<const NamedCompositeType*>(&node));
		auto other = static_cast<const NamedCompositeType&>(node);
		return *name == *other.name && ::equals(elements, other.elements, equal_target<VariablePtr>());
	}

	bool FunctionType::equals(const Node& node) const {
		assert(dynamic_cast<const FunctionType*>(&node));
		auto other = static_cast<const FunctionType&>(node);
		return *returnType == *other.returnType && ::equals(parameterTypes, other.parameterTypes, equal_target<TypePtr>());
	}

	bool VarDecl::equals(const Node& node) const {
		assert(dynamic_cast<const VarDecl*>(&node));
		auto other = static_cast<const VarDecl&>(node);
		return *var == *other.var && *init == *other.init;
	}

	bool Compound::equals(const Node& node) const {
		assert(dynamic_cast<const Compound*>(&node));
		auto other = static_cast<const Compound&>(node);
		return ::equals(statements, other.statements, equal_target<StatementPtr>());
	}

	bool If::equals(const Node& node) const {
		assert(dynamic_cast<const If*>(&node));
		auto other = static_cast<const If&>(node);
		return *condition==*other.condition && *thenStmt == *other.thenStmt && *elseStmt == *other.elseStmt;
	}

	bool Switch::equals(const Node& node) const {
		assert(dynamic_cast<const Switch*>(&node));
		auto other = static_cast<const Switch&>(node);
		return *value == *other.value && *defaultBranch==*other.defaultBranch &&
				::equals(cases, other.cases, equal_pointer_pair<ExpressionPtr, StatementPtr>());
	}

	bool For::equals(const Node& node) const {
		assert(dynamic_cast<const For*>(&node));
		auto other = static_cast<const For&>(node);
		return *init==*other.init && *check == *other.check && *step== *other.step && *body == *other.body;
	}

	bool While::equals(const Node& node) const {
		assert(dynamic_cast<const While*>(&node));
		auto other = static_cast<const While&>(node);
		return *condition==*other.condition && *body == *other.body;
	}

	bool Return::equals(const Node& node) const {
		assert(dynamic_cast<const Return*>(&node));
		auto other = static_cast<const Return&>(node);
		return *value==*other.value;
	}

	bool Literal::equals(const Node& node) const {
		assert(dynamic_cast<const Literal*>(&node));
		auto other = static_cast<const Literal&>(node);
		return value==other.value;
	}

	bool Variable::equals(const Node& node) const {
		assert(dynamic_cast<const Variable*>(&node));
		auto other = static_cast<const Variable&>(node);
		return *type==*other.type && *name == *other.name;
	}

	bool Initializer::equals(const Node& node) const {
		assert(dynamic_cast<const Initializer*>(&node));
		auto other = static_cast<const Initializer&>(node);
		return *type==*other.type && ::equals(values, other.values, equal_target<NodePtr>());
	}

	bool UnaryOperation::equals(const Node& node) const {
		assert(dynamic_cast<const UnaryOperation*>(&node));
		auto other = static_cast<const UnaryOperation&>(node);
		return operation==other.operation && *operand==*other.operand;
	}

	bool BinaryOperation::equals(const Node& node) const {
		assert(dynamic_cast<const BinaryOperation*>(&node));
		auto other = static_cast<const BinaryOperation&>(node);
		return operation==other.operation && *operandA==*other.operandA && *operandB==*other.operandB;
	}

	bool TernaryOperation::equals(const Node& node) const {
		assert(dynamic_cast<const TernaryOperation*>(&node));
		auto other = static_cast<const TernaryOperation&>(node);
		return operation==other.operation && *operandA==*other.operandA && *operandB==*other.operandB && *operandC==*other.operandC;
	}

	bool Call::equals(const Node& node) const {
		assert(dynamic_cast<const Call*>(&node));
		auto other = static_cast<const Call&>(node);
		return *function==*other.function && ::equals(arguments, other.arguments, equal_target<ExpressionPtr>());
	}

	bool Parentheses::equals(const Node& node) const {
		assert(dynamic_cast<const Parentheses*>(&node));
		auto other = static_cast<const Parentheses&>(node);
		return *expression==*other.expression;
	}

	bool TypeDeclaration::equals(const Node& node) const {
		assert(dynamic_cast<const TypeDeclaration*>(&node));
		auto other = static_cast<const TypeDeclaration&>(node);
		return *type==*other.type;
	}

	bool FunctionPrototype::equals(const Node& node) const {
		assert(dynamic_cast<const FunctionPrototype*>(&node));
		auto other = static_cast<const FunctionPrototype&>(node);
		return *function==*other.function;
	}

	bool TypeDefinition::equals(const Node& node) const {
		assert(dynamic_cast<const TypeDefinition*>(&node));
		auto other = static_cast<const TypeDefinition&>(node);
		return *type==*other.type && *name==*other.name;
	}

	bool Function::equals(const Node& node) const {
		assert(dynamic_cast<const Function*>(&node));
		auto other = static_cast<const Function&>(node);
		return
				*returnType==*other.returnType &&
				*name==*other.name &&
				::equals(parameter, other.parameter, equal_target<VariablePtr>()) &&
				*body==*other.body;
	}


} // end namespace c_ast
} // end namespace backend
} // end namespace insieme
