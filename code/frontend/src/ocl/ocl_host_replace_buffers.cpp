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

#include "insieme/frontend/ocl/ocl_host_replace_buffers.h"
#include "insieme/utils/logging.h"
#include "insieme/analysis/cba/analysis.h"
#include "insieme/core/ir_visitor.h"
#include "insieme/core/pattern/ir_pattern.h"
#include "insieme/core/pattern/pattern.h"
#include "insieme/core/transform/node_replacer.h"
#include "insieme/core/types/subtyping.h"

using namespace insieme::core;
using namespace insieme::core::pattern;

namespace insieme {
namespace frontend {
namespace ocl {

namespace {
bool extractSizeFromSizeof(const core::ExpressionPtr& arg, core::ExpressionPtr& size, core::TypePtr& type, bool foundMul) {
	// get rid of casts
	NodePtr uncasted = arg;
	while (uncasted->getNodeType() == core::NT_CastExpr) {
		uncasted = static_pointer_cast<CastExprPtr>(uncasted)->getType();
	}

	if (const CallExprPtr call = dynamic_pointer_cast<const CallExpr> (uncasted)) {
		// check if there is a multiplication
		if(call->getFunctionExpr()->toString().find(".mul") != string::npos && call->getArguments().size() == 2) {
			IRBuilder builder(arg->getNodeManager());
			// recursively look into arguments of multiplication
			if(extractSizeFromSizeof(call->getArgument(0), size, type, true)) {
				if(size)
					size = builder.callExpr(call->getType(), call->getFunctionExpr(), size, call->getArgument(1));
				else
					size = call->getArgument(1);
				return true;
			}
			if(extractSizeFromSizeof(call->getArgument(1), size, type, true)){
				if(size)
					size = builder.callExpr(call->getType(), call->getFunctionExpr(), call->getArgument(0), size);
				else
					size = call->getArgument(0);
				return true;
			}
		}
		// check if we reached a sizeof call
		if (call->toString().substr(0, 6).find("sizeof") != string::npos) {
			// extract the type to be allocated
			type = dynamic_pointer_cast<GenericTypePtr>(call->getArgument(0)->getType())->getTypeParameter(0);
			assert(type && "Type could not be extracted!");

			if(!foundMul){ // no multiplication, just sizeof alone is passed as argument -> only one element
				IRBuilder builder(arg->getNodeManager());
				size = builder.literal(arg->getNodeManager().getLangBasic().getUInt8(), "1");
				return true;
			}

			return true;
		}
	}
	return false;
}


template<typename Enum>
void recursiveFlagCheck(const NodePtr& flagExpr, std::set<Enum>& flags) {
	if (const CallExprPtr call = dynamic_pointer_cast<const CallExpr>(flagExpr)) {
		const lang::BasicGenerator& gen = flagExpr->getNodeManagerPtr()->getLangBasic();
		// check if there is an lshift -> flag reached
		if (call->getFunctionExpr() == gen.getGenLShift() ||
					call->getFunctionExpr() == gen.getSignedIntLShift() || call->getFunctionExpr() == gen.getUnsignedIntLShift()) {
			if (const LiteralPtr flagLit = dynamic_pointer_cast<const Literal>(call->getArgument(1))) {
				int flag = flagLit->getValueAs<int>();
				if (flag < Enum::size) // last field of enum to be used must be size
					flags.insert(Enum(flag));
				else
					LOG(ERROR) << "Flag " << flag << " is out of range. Max value is " << CreateBufferFlags::size - 1;
			}
		} else if (call->getFunctionExpr() == gen.getGenOr() ||
				call->getFunctionExpr() == gen.getSignedIntOr() || call->getFunctionExpr() == gen.getUnsignedIntOr()) {
			// two flags are ored, search flags in the arguments
			recursiveFlagCheck(call->getArgument(0), flags);
			recursiveFlagCheck(call->getArgument(1), flags);
		} else
			LOG(ERROR) << "Unexpected operation in flag argument: " << call->getFunctionExpr() << "\nUnable to deduce flags, using default settings";

	}
}

template<typename Enum>
std::set<Enum> getFlags(const NodePtr& flagExpr) {

	const lang::BasicGenerator& gen = flagExpr->getNodeManagerPtr()->getLangBasic();
	std::set<Enum> flags;
	// remove cast to uint<8>
	if (flagExpr.isa<CallExprPtr>() && gen.isScalarCast(flagExpr.as<CallExprPtr>()->getFunctionExpr())){
		recursiveFlagCheck(flagExpr.as<CallExprPtr>()->getArgument(0), flags);
	}else
	if (const CastExprPtr cast = dynamic_pointer_cast<const CastExpr>(flagExpr)) {
		recursiveFlagCheck(cast->getSubExpression(), flags);
	} else
		LOG(ERROR) << "No flags found in " << flagExpr << "\nUsing default settings";
	return flags;
}

ExpressionAddress extractVariable(ExpressionAddress expr) {
	const lang::BasicGenerator& gen = expr->getNodeManagerPtr()->getLangBasic();

	if(expr->getNodeType() == NT_Variable) // return variable
		return expr;

	if(expr->getNodeType() == NT_Literal) // return literal, e.g. global varialbe
		return expr;

	if(CallExprAddress call = dynamic_address_cast<const CallExpr>(expr)) {
		if(gen.isSubscriptOperator(call->getFunctionExpr()))
			return expr;

		if(gen.isCompositeRefElem(call->getFunctionExpr())) {
			return expr;
		}
	}

	return expr;
}
}

const NodePtr BufferMapper::resolveElement(const NodePtr& ptr) {
	return ptr;
}

BufferReplacer::BufferReplacer(ProgramPtr& prog) : prog(prog) {
	collectInformation();
	generateReplacements();
}

void BufferReplacer::collectInformation() {
	NodeManager& mgr = prog->getNodeManager();
	ProgramAddress pA(prog->getChild(0));

	TreePatternPtr clCreateBuffer = irp::callExpr(pattern::any, irp::literal("clCreateBuffer"),
			pattern::any << var("flags", pattern::any) << var("size", pattern::any) << var("host_ptr", pattern::any) << pattern::any);
	TreePatternPtr bufferDecl = irp::declarationStmt(var("buffer", pattern::any), irp::callExpr(pattern::any, irp::atom(mgr.getLangBasic().getRefVar()),
			pattern::single(clCreateBuffer)));
	TreePatternPtr bufferAssign = irp::callExpr(pattern::any, irp::atom(mgr.getLangBasic().getRefAssign()),
			var("buffer", pattern::any) << clCreateBuffer);
	TreePatternPtr bufferPattern = bufferDecl | bufferAssign;
	visitDepthFirst(pA, [&](const NodeAddress& node) {
		AddressMatchOpt createBuffer = bufferPattern->matchAddress(node);

		if(createBuffer) {
			NodePtr flagArg = createBuffer->getVarBinding("flags").getValue();
			std::set<enum CreateBufferFlags> flags = getFlags<enum CreateBufferFlags>(flagArg);
			// check if CL_MEM_USE_HOST_PTR is set
//			bool usePtr = flags.find(CreateBufferFlags::CL_MEM_USE_HOST_PTR) != flags.end();
			// check if CL_MEM_COPY_HOST_PTR is set
//			bool copyPtr = flags.find(CreateBufferFlags::CL_MEM_COPY_HOST_PTR) != flags.end();

			// extract type form size argument
			ExpressionPtr size;
			TypePtr type;
#ifdef	NDEBUG
			extractSizeFromSizeof(createBuffer->getVarBinding("size").getValue().as<ExpressionPtr>(), size, type, false);
#else
			assert(extractSizeFromSizeof(createBuffer->getVarBinding("size").getValue().as<ExpressionPtr>(), size, type, false)
					&& "cannot extract size and type from size paramater fo clCreateBuffer");
#endif
			ExpressionPtr hostPtr = createBuffer->getVarBinding("host_ptr").getValue().as<ExpressionPtr>();

			// get the buffer expression
			ExpressionAddress lhs = createBuffer->getVarBinding("buffer").getValue().as<ExpressionAddress>();
//			std::cout << "\nyipieaiey: " << lhs << std::endl << std::endl;

			// add gathered information to clMemMetaMap
			this->clMemMeta[lhs] = ClMemMetaInfo(size, type, flags, hostPtr);
		}
		return;
	});

}

void BufferReplacer::generateReplacements() {
	NodeManager& mgr = prog.getNodeManager();
	IRBuilder builder(mgr);

	TypePtr clMemTy;

	for_each(clMemMeta, [&](std::pair<ExpressionAddress, ClMemMetaInfo> meta) {
		ExpressionAddress bufferExpr = extractVariable(meta.first);
		visitDepthFirst(ExpressionPtr(bufferExpr)->getType(), [&](const NodePtr& node) {
	//			std::cout << node << std::endl;
		//if(node.toString().compare("_cl_mem") == 0) std::cout << "\nGOCHA" << node << "  " << node.getNodeType() << "\n";
			if(GenericTypePtr gt = dynamic_pointer_cast<const GenericType>(node)) {
				if(gt.toString().compare("_cl_mem") == 0)
					clMemTy = gt;
			}
		}, true, true);

std::cout << NodePtr(meta.first) << " "  << meta.second.type << std::endl;

		TypePtr newType = transform::replaceAll(mgr, meta.first->getType(), clMemTy, meta.second.type).as<TypePtr>();
		bool alreadyThereAndCorrect = false;

		// check if there is already a replacement for the current expression (or an alias of it) with a different type
		for_each(clMemReplacements, [&](std::pair<ExpressionAddress, ExpressionPtr> replacement) {
			if(replacement.first == bufferExpr/* || analysis::cba::isAlias(replacement.first, bufferExpr)*/) {
				std::cout << "repty " << replacement.second << std::endl;
				std::cout << "newTy " << newType << std::endl;
				if(replacement.second->getType() != newType) {
					if(types::isSubTypeOf(replacement.second->getType(), newType)) { // if the new type is subtype of the current one, replace the type
						//newType = replacement.second->getType();
						bufferExpr = replacement.first; // do not add aliases to the replacement map
					} else if(types::isSubTypeOf(newType, replacement.second->getType())) { // if the current type is subtype of the new type, do nothing
						alreadyThereAndCorrect = true;
						return;
					} else // if the types are not related, fail
						assert(false && "Buffer used twice with different types. Not supported by Insieme.");
				} else
					alreadyThereAndCorrect = true;
			}
		});

		if(alreadyThereAndCorrect) return;

		// local variable case
		if(VariableAddress var = dynamic_address_cast<const Variable>(bufferExpr)) {
			clMemReplacements[var] = builder.variable(newType);
		}
		// global variable case
		if(LiteralAddress lit = dynamic_address_cast<const Literal>(bufferExpr)) {
			clMemReplacements[lit] = builder.literal(newType, lit->getStringValue());
		}

		// try to extract the variable
		if(mgr.getLangBasic().isSubscriptOperator(bufferExpr)){
			std::cout << "tolles subscript\n";

			assert(false && "fuck you");
		}

		TreePatternPtr subscriptPattern = irp::callExpr(pattern::any, pattern::any, pattern::any << pattern::any);

		AddressMatchOpt subscript = subscriptPattern->matchAddress(bufferExpr);

		if(subscript) {
			std::cout << "MATCH: " << subscript << std::endl;
		}
//AP(rec v0.{v0=fun(ref<array<'elem,1>> v1, uint<8> v2) {return ref.narrow(v1, dp.element(dp.root, v2), 'elem);}}(ref.vector.to.ref.array(v36), 1u))
//AP(rec v0.{v0=fun(ref<array<'elem,1>> v1, uint<8> v2) {return ref.narrow(v1, dp.element(dp.root, v2), 'elem);}}(ref.deref(v36), 0u))
	});

	for_each(clMemReplacements, [&](std::pair<ExpressionPtr, ExpressionPtr> replacement) {
		std::cout << replacement.first->getType() << " -> " << replacement.second->getType() << std::endl;
	});
}

} //namespace ocl
} //namespace frontend
} //namespace insieme
