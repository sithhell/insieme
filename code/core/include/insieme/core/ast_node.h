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

#include <cassert>

#include "insieme/utils/hash_utils.h"
#include "insieme/utils/instance_manager.h"
#include "insieme/utils/string_utils.h"

#include "insieme/core/annotated_ptr.h"
#include "insieme/core/int_type_param.h"

#include "insieme/core/lang/basic.h"

namespace insieme {
namespace core {

/**
 * Defines an enumeration containing an entry for every node type. This
 * enumeration can than be used to identify the actual type of AST nodes
 * in case the exact type cannot be determined statically.
 */
#define CONCRETE(name) NT_ ## name,
enum NodeType {
// the necessary information is obtained from the node-definition file
#include "ast_nodes.def"
};
#undef CONCRETE

/**
 * An enumeration covering the five basic node categories.
 */
enum NodeCategory {
	NC_Support, /* < The node represents a supporting element. */
	NC_Type, /* < The node represents a type. */
	NC_Expression, /* < The node represents an expression. */
	NC_Statement, /* < The node represents a statement. */
	NC_Program /* < The node represents a program. */
};


template<typename T> class Address;
template<typename T> class AnnotatedPtr;

/**
 * Adds forward declarations for all AST node types. Further, for each
 * type a type definition for a corresponding annotated pointer is added.
 */
#define NODE(NAME) \
	class NAME; \
	typedef AnnotatedPtr<const NAME> NAME ## Ptr; \
	typedef Address<const NAME> NAME ## Address;

	// take all nodes from within the definition file
	#include "ast_nodes.def"

#undef NODE

/**
 * Implements a node manager to be used for maintaining AST node instances.
 */
class NodeManager: public InstanceManager<Node, AnnotatedPtr> {
public:
	const lang::BasicGenerator basic;

	NodeManager() : basic(*this) { }
};


// forward declaration of the clonePtr operation
template<typename T> AnnotatedPtr<T> clonePtr(NodeManager& manager, const AnnotatedPtr<T>& ptr);

/**
 * This class constitutes an interface for utility class required for transforming AST nodes.
 * Instances of this class represent mappings between nodes. During the transformation process,
 * each referenced pointer is replaced by the element is mapped to.
 */
class NodeMapping {

protected:

	/**
	 * Implements the actual mapping operation by mapping the given pointer (and the context information)
	 * to a new pointer.
	 *
	 * @param index the index of the ptr within the parents child list
	 * @param ptr the pointer to be resolved
	 * @return the pointer the given pointer is mapped to by this mapper
	 */
	virtual const NodePtr mapElement(unsigned index, const NodePtr& ptr) =0;

public:

	/**
	 * A virtual destructor of the mapping for a proper cleanup.
	 */
	virtual ~NodeMapping() {
	}
	;

	/**
	 * Requests to map the given integer type parameter to another element.
	 */
	virtual IntTypeParam mapParam(const IntTypeParam& param);

	/**
	 * Requests to map the given vector if integer type parameters to corre
	 */
	virtual vector<IntTypeParam> mapParam(const vector<IntTypeParam>& list);

	/**
	 * A generic version of the map operation handling pointer types properly.
	 */
	template<typename T>
	inline AnnotatedPtr<T> map(unsigned index, const AnnotatedPtr<T>& ptr) {
		// short-cut for null
		if (!ptr) {
			return static_pointer_cast<T> (ptr);
		}

		// map and cast
		const NodePtr res = mapElement(index, ptr);
		// during development, make cast secure
		assert(dynamic_pointer_cast<T> (res) && "Invalid conversion");
		return static_pointer_cast<T> (res);
	}

	/**
	 * Obtains a container of pointers referencing clones of nodes referenced by a given
	 * container of pointers. Thereby, annotations are properly preserved and isolated.
	 *
	 * @param container the container including the pointers to be cloned
	 * @return a new container including pointers referencing clones of the nodes referenced
	 * 		   by the original container.
	 */
	template<typename Container>
	Container map(unsigned offset, const Container& container) {
		Container res;

		auto first = container.begin();
		auto last = container.end();
		auto out = inserter(res, res.end());
		for (auto it = first; it != last; ++it) {
			*out = map(offset++, *it);
			out++;
		}

		return res;
	}

};

template<typename Lambda>
class LambdaNodeMapper: public NodeMapping {
	Lambda lambda;
public:
	LambdaNodeMapper(Lambda lambda)
		: lambda(lambda) { };

	const NodePtr mapElement(unsigned index, const NodePtr& ptr) {
		return lambda(index, ptr);
	}
};

template<typename Lambda>
LambdaNodeMapper<Lambda> makeLambdaMapper(Lambda lambda) {
	return LambdaNodeMapper<Lambda> (lambda);
}

/**
 * This class models an abstract base class for all AST nodes to be used within a program
 * representation. It defines a minimum number of functionality to be supported by all nodes
 * (including to be hash- and comparable, such that instances can be used within unordered
 * sets).
 */
class Node: public insieme::utils::HashableImmutableData<Node>, public Annotatable {

	/**
	 * Allow the instance manager to access the private clone method.
	 */
	friend class InstanceManager<Node, AnnotatedPtr> ;

public:

	/**
	 * The type of instance manager to be used with this type.
	 */
	typedef NodeManager Manager;

	/**
	 * The type used to represent the list of children of a node.
	 */
	typedef vector<NodePtr> ChildList;

	/**
	 * A type used to represent a optionally available child list.
	 */
	typedef std::shared_ptr<ChildList> OptionChildList;

private:

	/**
	 * The type of node to be represented by this instance.
	 */
	const NodeType nodeType;

	/**
	 * The category of node to be represented by this instance.
	 */
	const NodeCategory nodeCategory;

	/**
	 * A pointer to the manager this instance is maintained by.
	 */
	NodeManager* manager;

	/**
	 * The list of child nodes referenced by this node.
	 */
	mutable OptionChildList children;

	/**
	 * Retrieves a clone of this node, hence a newly allocated instance representing the same value
	 * as the current instance. The new instance (and all its referenced nodes) should be maintained
	 * by the given manager.
	 *
	 * @param manager the manager which should maintain all referenced nodes
	 * @return a clone of this instance referencing elements maintained exclusively by the given manager
	 */
	const Node* cloneTo(NodeManager& manager) const {
		// NOTE: this method is performing the all-AST-node work, the rest is done by createCloneUsing(...)

		// check whether cloning is necessary
		if (this->manager == &manager) {
			return this;
		}

		// trigger the creation of a clone
		auto cloner = makeLambdaMapper([&manager](unsigned, const NodePtr& ptr) {
					return clonePtr(manager, ptr);
				});
		Node* res = createCopyUsing(cloner);

		// update manager
		res->manager = &manager;

		// copy annotations
		res->setAnnotations(getAnnotations());

		// done
		return res;
	}

	/**
	 * A virtual method to be implemented by sub-classes to realize the actual creation of
	 * modified instances of AST nodes.
	 *
	 * @param mapper a mapping allowing to alter pointer instances during the copy process. For instance
	 * 			the mapper may re-direct all pointers to point to instances maintained by a different
	 * 			node manager (node migration) or to a totally different instance during transformations.
	 * @return a pointer to a new instance, representing an independent, transformed instance.
	 */
	virtual Node* createCopyUsing(NodeMapping& mapper) const =0;

protected:

	/**
	 * Construct a new node instance based on the essential features.
	 *
	 * @param nodeType the type of node to be created
	 * @param nodeCategory the category of the node to be created
	 * @param hashCode the hash code of the new node
	 */
	Node(const NodeType nodeType, const NodeCategory nodeCategory, const std::size_t& hashCode) :
		HashableImmutableData(hashCode), nodeType(nodeType), nodeCategory(nodeCategory), manager(NULL) {
	}

	/**
	 * Defines the new operator to be protected. This prevents instances of AST nodes to be
	 * created on the heap, thereby enforcing the usage of the static factory methods and
	 * NodeManager.
	 */
	static void* operator new(size_t);

	/**
	 * Defines the delete operator to be protected. This prevents instances of AST nodes to be
	 * created on the heap, thereby enforcing the usage of the static factory methods and
	 * NodeManager.
	 */
	void operator delete(void*);

	/**
	 * Defines the new operator for arrays to be protected. This prevents instances of AST nodes to be
	 * created on the heap, thereby enforcing the usage of the static factory methods and
	 * NodeManager.
	 */
	static void* operator new[](size_t);

	/**
	 * Defines the delete operator for arrays to be protected. This prevents instances of AST nodes to be
	 * created on the heap, thereby enforcing the usage of the static factory methods and
	 * NodeManager.
	 */
	void operator delete[](void*, size_t);

	/**
	 * Requests a list of child nodes from the actual node implementation.
	 */
	virtual OptionChildList getChildNodes() const = 0;

public:

	/**
	 * The default virtual destructor enforcing the proper destruction of derived
	 * instances.
	 */
	virtual ~Node() {
	}
	;

	/**
	 * Creates a new version of this node where every referenced to a child node
	 * is replaced by a pointer to the node returned by the given mapper.
	 *
	 * @param manager the manager to be used to create the new node
	 * @param mapper the mapper used to translate child node references
	 * @return a pointer to the modified node.
	 */
	NodePtr substitute(NodeManager& manager, NodeMapping& mapper) const;

	/**
	 * Obtains a reference to the manager maintaining this node instance. In case this
	 * node is not managed by any node manager (by any reason), an assertion will be violated.
	 *
	 * @return a reference to the manager maintaining this node
	 */
	inline NodeManager& getNodeManager() const {
		assert(manager && "NodeManager must not be null - unmanaged node detected!");
		return *manager;
	}

	/**
	 * Obtains a pointer to the manager maintaining this node instance. In case this
	 * node is not managed by any node manager (by any reason), NULL will be returned.
	 *
	 * @return a pointer to the manager maintaining this node
	 */
	inline NodeManager* getNodeManagerPtr() const {
		return manager;
	}

	/**
	 * Retrieves a reference to the internally maintained list of child nodes. The
	 * retrieved list is valid as long as this node exists. Hence, during its destruction,
	 * the returned list will also be eliminated.
	 *
	 * @return a reference to the internally maintained list of child nodes.
	 */
	const ChildList& getChildList() const;

	/**
	 * This pure abstract method is imposing the requirement to every node to
	 * be printable to an output stream.
	 *
	 * @param out the stream to be printed to
	 * @return the output stream to print further information (allows to concatenate print operations)
	 */
	virtual std::ostream& printTo(std::ostream& out) const = 0;

	/**
	 * Provides a string representation of this node, which is by default
	 * the same as would be printed in case the object is written into an
	 * output stream.
	 *
	 * @return a string representation for this node instance
	 */
	virtual string toString() const {
		return ::toString(*this);
	}

	/**
	 * Obtains the type of this node.
	 *
	 * @return the type of the node represented by this instance
	 *
	 * @see NodeType
	 */
	NodeType getNodeType() const {
		return nodeType;
	}

	/**
	 * Obtains the category of this node.
	 *
	 * @return the category of the node represented by this instance
	 *
	 * @see NodeCategory
	 */
	NodeCategory getNodeCategory() const {
		return nodeCategory;
	}

	/**
	 * A default implementation of the equals operator comparing the actual
	 * names of the types.
	 */
	bool operator==(const Node& other) const {
		// test for identity
		if (this == &other) {
			return true;
		}

		// fast hash code test
		if (hash() != other.hash()) {
			return false;
		}

		// check node type
		if (nodeType != other.nodeType) {
			return false;
		}

		// use virtual equals method
		return equals(other);
	}

};

/**
 * Isolates the given pointer such that it is no longer sharing the same annotation
 * map with other pointers referencing the same element.
 *
 * NOTE: the handed in reference can be constant since annotations are even mutable within
 * constant instances.
 *
 * @param ptr the pointer to be isolated.
 * @return a reference to the handed in pointer.
 */
template<typename T>
const AnnotatedPtr<const T>& isolate(const AnnotatedPtr<const T>& ptr) {
	// no longer required ...
	// TODO: remove this function
	// ptr.isolateAnnotations();
	return ptr;
}

/**
 * Isolates the pointers within the given list from copies eventual copies, thereby separating
 * the internal link allowing copies to shared annotations.
 *
 * @param container the container listing the pointers to be isolated
 * @return a reference to the handed in container
 */
template<typename Container>
const Container& isolate(const Container& container, typename Container::value_type* = 0) {
	// no longer required ...
	// TODO: remove this function

//	for_each(container, [](const typename Container::value_type& cur) {
//				cur.isolateAnnotations();
//			});
	return container;
}

/**
 * Obtains a pointer referencing a clone of the element referenced by the given pointer within the
 * given manager. If there is no such instance, a new one will be created. Further, the annotations
 * associated to the pointer will be preserved.
 *
 * @param manager the manager from which a cloned node should be obtained from
 * @param ptr the pointer from which a clone should be obtained from
 * @return the pointer to the cloned node within the given manager
 */
template<typename T>
inline AnnotatedPtr<T> clonePtr(NodeManager& manager, const AnnotatedPtr<T>& ptr) {
	// null pointers or proper points do not have to be modified
	if (!ptr || ptr->getNodeManagerPtr() == &manager) {
		return ptr;
	}

	// obtain pointer to same instance within new manager
	return manager.get(ptr);
}

/**
 * Checks whether the given two node pointer are the root of the same AST tree
 * containing the same set of annotations on the nodes and the pointer between
 * nodes.
 *
 * @param nodeA the root of the first tree
 * @param nodeB the root of the second tree
 * @return true if they are equivalent, false otherwiser
 */
bool equalsWithAnnotations(const NodePtr& nodeA, const NodePtr& nodeB);

} // end namespace core
} // end namespace insieme

/**
 * Allows nodes to be printed to a stream (especially useful during debugging and
 * within test cases where equals expects values to be printable).
 */
std::ostream& operator<<(std::ostream& out, const insieme::core::Node& node);

