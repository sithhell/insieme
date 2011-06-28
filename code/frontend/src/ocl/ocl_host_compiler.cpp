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

#include "insieme/core/expressions.h"
#include "insieme/core/ast_node.h"

#include "insieme/core/transform/node_replacer.h"

#include "insieme/c_info/naming.h"
#include "insieme/c_info/location.h"
#include "insieme/frontend/ocl/ocl_host_compiler.h"
#include "insieme/frontend/ocl/ocl_annotations.h"
#include "insieme/frontend/clang_config.h"

#include <fstream>

namespace ba = boost::algorithm;
namespace iocl = insieme::ocl;

namespace insieme {
namespace frontend {
namespace ocl {
using namespace insieme::core;

namespace {

const ProgramPtr& loadKernelsFromFile(string path, const ASTBuilder& builder) {
	// delete quotation marks form path
	if (path[0] == '"')
		path = path.substr(1, path.length() - 2);

	std::ifstream check;
	string root = path;
	size_t nIncludes = CommandLineOptions::IncludePaths.size();
	// try relative path first
	check.open(path);
	// if not found now, check the include directories
	for (size_t i = 0; i < nIncludes && check.fail(); ++i) {
		check.close();
		// try with include paths
		path = CommandLineOptions::IncludePaths.at(i) + "/" + root;
		check.open(path);
	}
	// if there is still no match, try the paths of the input files
	size_t nInputFiles = CommandLineOptions::InputFiles.size();
	for (size_t i = 0; i < nInputFiles && check.fail(); ++i) {
		// try the paths of the input files
		string ifp = CommandLineOptions::InputFiles.at(i);
		size_t slash = ifp.find_last_of("/");
		path = ifp.substr(0u, slash + 1) + root;
		check.open(path);
	}

	check.close();

	if (check.fail()) {// no kernel file found, leave the error printing to the compiler frontend
	//		std::cerr << "FAIL! " << path << std::endl;
		path = root;
	}

	LOG(INFO)
		<< "Converting kernel file '" << path << "' to IR...";

	frontend::Program fkernels(builder.getNodeManager());
	fkernels.addTranslationUnit(path);
	return fkernels.convert();
}

void tryStructExtract(ExpressionPtr& expr, ASTBuilder& builder) {
	if (const CallExprPtr& cre = dynamic_pointer_cast<const CallExpr>(expr)) {
		if (cre->getFunctionExpr() == BASIC.getCompositeRefElem()) {
			expr = cre->getArgument(0);
		}
	}
}

bool isNullPtr(ExpressionPtr& expr, ASTBuilder& builder) {
	if (const CallExprPtr rta = dynamic_pointer_cast<const CallExpr>(expr))
		if (rta->getFunctionExpr() == BASIC.getRefToAnyRef())
			if (const CallExprPtr refVar = dynamic_pointer_cast<const CallExpr>(rta->getArgument(0)))
				if (refVar->getFunctionExpr() == BASIC.getRefVar())
					if (const CallExprPtr getNull = dynamic_pointer_cast<const CallExpr>(refVar->getArgument(0)))
						if (getNull->getFunctionExpr() == BASIC.getGetNull())
							return true;

	return false;
}

bool KernelCodeRetriver::visitNode(const core::NodePtr& node) {
	if (node == breakingStmt) {
		return false; // stop recursion
	}
	return true; // go on with search
}

bool KernelCodeRetriver::visitCallExpr(const core::CallExprPtr& callExpr) {
	if (callExpr->getFunctionExpr() != BASIC.getRefAssign())
		return true;
	// check if it is the variable we are looking for
	if (const VariablePtr& lhs = dynamic_pointer_cast<const Variable>(callExpr->getArgument(0))) {
		if (lhs->getId() != pathToKernelFile->getId())
			return true;
	} else {
		return true;
	}
	if (const CallExprPtr& rhs = dynamic_pointer_cast<const CallExpr>(callExpr->getArgument(0))) {
		if (const CallExprPtr& callSaC = dynamic_pointer_cast<const CallExpr>(rhs->getArgument(0))) {
			if (const LiteralPtr& stringAsChar = dynamic_pointer_cast<const Literal>(callSaC->getFunctionExpr())) {
				if (stringAsChar->getValue() == "string.as.char.pointer") {
					if (const LiteralPtr& pl = dynamic_pointer_cast<const Literal>(callSaC->getArgument(0))) {
						path = pl->getValue();
						return false;
					}
				}
			}
		}
	}
	return true;
}

bool KernelCodeRetriver::visitDeclarationStmt(const core::DeclarationStmtPtr& decl) {
	if (decl->getVariable()->getId() != pathToKernelFile->getId())
		return true;

	if (const CallExprPtr& callSaC = dynamic_pointer_cast<const CallExpr>(decl->getInitialization())) {
		if (const LiteralPtr& stringAsChar = dynamic_pointer_cast<const Literal>(callSaC->getFunctionExpr())) {
			if (stringAsChar->getValue() == "string.as.char.pointer") {
				if (const LiteralPtr& pl = dynamic_pointer_cast<const Literal>(callSaC->getArgument(0))) {
					path = pl->getValue();
					return false;
				}
			}
		}
	}
	return true;
}
bool Ocl2Inspire::extractSizeFromSizeof(const core::ExpressionPtr& arg, core::ExpressionPtr& size, core::TypePtr& type) {
	// get rid of casts
	NodePtr uncasted = arg;
	while (uncasted->getNodeType() == core::NT_CastExpr) {
		uncasted = uncasted->getChildList().at(1);
	}

	while (const CallExprPtr& mul = dynamic_pointer_cast<const CallExpr> (uncasted)) {
		for (int i = 0; i < 2; ++i) {
			// get rid of casts
			core::NodePtr arg = mul->getArgument(i);
			while (arg->getNodeType() == core::NT_CastExpr) {
				arg = arg->getChildList().at(1);
			}
			if (const CallExprPtr& sizeof_ = dynamic_pointer_cast<const CallExpr>(arg)) {
				if (sizeof_->toString().substr(0, 6).find("sizeof") != string::npos) {
					// extract the type to be allocated
					type = dynamic_pointer_cast<const Type> (sizeof_->getArgument(0)->getType()->getChildList().at(0));
					// extract the number of elements to be allocated
					size = mul->getArgument(1 - i);
					return true;
				}
			}
		}
		uncasted = mul->getArgument(0);
	}
	return false;
}

ExpressionPtr Ocl2Inspire::getClCreateBuffer(bool copyHostPtr) {
	// read/write flags ignored
	// errcorcode always set to 0 = CL_SUCCESS
	if (copyHostPtr)
		return parser.parseExpression("fun(type<'a>:elemType, uint<8>:flags, uint<8>:size, anyRef:hostPtr, ref<array<int<4>, 1> >:errorcode_ret) -> array<'a, 1>  {{ \
	            decl ref<array<'a, 1> >:devicePtr = (op<ref.var>( (op<array.create.1D>( elemType, size )) )); \
				decl ref<array<'a, 1> >:hp = (op<anyref.to.ref>(hostPtr, lit<type<array<'a, 1> >, type(array('a ,1)) > )); \
				for(decl uint<8>:i = lit<uint<8>, 0> .. size : 1) \
					( (op<array.ref.elem.1D>(devicePtr, i )) = (op<ref.deref>( (op<array.ref.elem.1D>(hp, i )) )) ); \
				 \
	            ( (op<array.ref.elem.1D>(errorcode_ret, lit<uint<8>, 0> )) = 0 ); \
				return (op<ref.deref> (devicePtr)); \
	       }}");

	return parser.parseExpression("fun(type<'a>:elemType, uint<8>:flags, uint<8>:size, ref<array<int<4>, 1> >:errorcode_ret) -> array<'a, 1>  {{ \
            ( (op<array.ref.elem.1D>(errorcode_ret, lit<uint<8>, 0> )) = 0 ); \
            return (op<array.create.1D>( elemType, size )); \
       }}");
}

ExpressionPtr Ocl2Inspire::getClWriteBuffer() {
	// blocking_write ignored
	// event stuff removed
	// always returns 0 = CL_SUCCESS
	return parser.parseExpression("fun(ref<array<'a, 1> >:devicePtr, uint<4>:blocking_write, uint<8>:offset, uint<8>:cb, anyRef:hostPtr) -> int<4> {{ \
            decl ref<array<'a, 1> >:hp = (op<anyref.to.ref>(hostPtr, lit<type<array<'a, 1> >, type(array('a ,1)) > )); \
            for(decl uint<8>:i = lit<uint<8>, 0> .. cb : 1) \
                ( (op<array.ref.elem.1D>(devicePtr, (i + offset) )) = (op<ref.deref>( (op<array.ref.elem.1D>(hp, i )) )) ); \
            return 0; \
    }}");
}

ExpressionPtr Ocl2Inspire::getClWriteBufferFallback() {
	// blocking_write ignored
	// event stuff removed
	// always returns 0 = CL_SUCCESS
	return parser.parseExpression("fun(ref<array<'a, 1> >:devicePtr, uint<4>:blocking_write, uint<8>:offset, uint<8>:cb, anyRef:hostPtr) -> int<4> {{ \
            decl ref<array<'a, 1> >:hp = (op<anyref.to.ref>(hostPtr, lit<type<array<'a, 1> >, type(array('a ,1)) > )); \
            decl uint<8>:size = (cb / (op<sizeof>( lit<type<'a>, type('a) > )) ); \
            for(decl uint<8>:i = lit<uint<8>, 0> .. size : 1) \
                ( (op<array.ref.elem.1D>(devicePtr, (i + offset) )) = (op<ref.deref>( (op<array.ref.elem.1D>(hp, i )) )) ); \
            return 0; \
    }}");
}

ExpressionPtr Ocl2Inspire::getClReadBuffer() {
	// blocking_write ignored
	// event stuff removed
	// always returns 0 = CL_SUCCESS
	return parser.parseExpression("fun(ref<array<'a, 1> >:devicePtr, uint<4>:blocking_read, uint<8>:offset, uint<8>:cb, anyRef:hostPtr) -> int<4> {{ \
            return 0; \
    }}");
	/*
	 decl ref<array<'a, 1> >:hp = (op<anyref.to.ref>(hostPtr, lit<type<array<'a, 1> >, type<array<'a ,1 > )); \
            for(decl uint<8>:i = lit<uint<8>, 0> .. cb : 1) \
                ( (op<array.ref.elem.1D>(hp, (i + offset) )) = (op<ref.deref>( (op<array.ref.elem.1D>(devicePtr, i )) )) );
	 */
}

ExpressionPtr Ocl2Inspire::getClReadBufferFallback() {
	// blocking_write ignored
	// event stuff removed
	// always returns 0 = CL_SUCCESS
	return parser.parseExpression("fun(ref<array<'a, 1> >:devicePtr, uint<4>:blocking_read, uint<8>:offset, uint<8>:cb, anyRef:hostPtr) -> int<4> {{ \
            decl ref<array<'a, 1> >:hp = (op<anyref.to.ref>(hostPtr, lit<type<array<'a, 1> >, type<array<'a ,1 > )); \
            decl uint<8>:size = (cb / (op<sizeof>( lit<type<'a>, type('a) > )) ); \
            for(decl uint<8>:i = lit<uint<8>, 0> .. size : 1) \
                ( (op<array.ref.elem.1D>(hp, (i + offset) )) = (op<ref.deref>( (op<array.ref.elem.1D>(devicePtr, i )) )) ); \
            return 0; \
    }}");
}

HostMapper::HostMapper(ASTBuilder& build, ProgramPtr& program) :
	builder(build), o2i(build.getNodeManager()), mProgram(program) {
	ADD_Handler(builder, "clCreateBuffer",
			std::set<enum CreateBufferFlags> flags = this->getFlags<enum CreateBufferFlags>(node->getArgument(1));

			// check if CL_MEM_USE_HOST_PTR is set
			bool usePtr = flags.find(CreateBufferFlags::CL_MEM_USE_HOST_PTR) != flags.end();
			// check if CL_MEM_COPY_HOST_PTR is set
			bool copyPtr = flags.find(CreateBufferFlags::CL_MEM_COPY_HOST_PTR) != flags.end();

			// extract the size form argument size, relying on it using a multiple of sizeof(type)
			ExpressionPtr size;
			TypePtr type;
			ExpressionPtr hostPtr;

			if(CastExprPtr c = dynamic_pointer_cast<const CastExpr>(node->getArgument(3))) {
				assert(!copyPtr && "When CL_MEM_USE_HOST_PTR is set, host_ptr parameter must be a valid pointer");
				if(c->getSubExpression()->getType() != BASIC.getAnyRef()) {// a scalar (probably NULL) has been passed as hostPtr arg
					hostPtr = builder.callExpr(BASIC.getRefToAnyRef(), builder.callExpr(BASIC.getRefVar(), c->getSubExpression()));
				}
			}
			else
			hostPtr = node->getArgument(3);

			if(usePtr) { // in this case we can just use the host_ptr instead of the cl_mem variable
				return hostPtr;
			}

			ExpressionPtr fun = o2i.getClCreateBuffer(copyPtr);

			bool sizeFound = o2i.extractSizeFromSizeof(node->getArgument(2), size, type);
			assert(sizeFound && "Unable to deduce type from clCreateBuffer call: No sizeof call found, cannot translate to INSPIRE.");

			vector<ExpressionPtr> args;
			args.push_back(BASIC.getTypeLiteral(type));
			args.push_back(node->getArgument(1));
			args.push_back(size);
			if(copyPtr) args.push_back(hostPtr);
			args.push_back(node->getArgument(4));
			ExpressionPtr retVal = builder.callExpr(builder.arrayType(type), fun, args);
			return retVal;
	); ADD_Handler(builder, "clEnqueueWriteBuffer",
			// extract the size form argument size, relying on it using a multiple of sizeof(type)
			ExpressionPtr size;
			TypePtr type;

			bool foundSizeOf = o2i.extractSizeFromSizeof(node->getArgument(4), size, type);

			vector<ExpressionPtr> args;
			args.push_back(node->getArgument(1));
			args.push_back(node->getArgument(2));
			args.push_back(node->getArgument(3));
			args.push_back(foundSizeOf ? size : node->getArgument(4));
			args.push_back(node->getArgument(5));
			return builder.callExpr(foundSizeOf ? o2i.getClWriteBuffer() : o2i.getClWriteBufferFallback(), args);
	);

	ADD_Handler(builder, "clEnqueueReadBuffer",
			// extract the size form argument size, relying on it using a multiple of sizeof(type)
			ExpressionPtr size;
			TypePtr type;

			bool foundSizeOf = o2i.extractSizeFromSizeof(node->getArgument(4), size, type);

			vector<ExpressionPtr> args;
			args.push_back(node->getArgument(1));
			args.push_back(node->getArgument(2));
			args.push_back(node->getArgument(3));
			args.push_back(foundSizeOf ? size : node->getArgument(4));
			args.push_back(node->getArgument(5));
			return builder.callExpr(foundSizeOf ? o2i.getClReadBuffer() : o2i.getClReadBufferFallback(), args);
	);

	ADD_Handler(builder, "clSetKernelArg",
			// arg_index must either be an integer literal or all arguments have to be specified in the right order in the source code
			ExpressionPtr kernel = node->getArgument(0);
			// check if kernel argument is in a struct, if yes, use the struct-variable
			tryStructExtract(kernel, builder);
			ExpressionPtr arg = node->getArgument(3);

			if(isNullPtr(arg, builder)) {
				// in this case arg is a local variable which has to be declared in host code
				// need to read size parameter
				ExpressionPtr size;
				TypePtr type;
				ExpressionPtr hostPtr;

				bool sizeFound = o2i.extractSizeFromSizeof(node->getArgument(2), size, type);
				assert(sizeFound && "Unable to deduce type from clSetKernelArg call when allocating local memory: No sizeof call found, cannot translate to INSPIRE.");

				// declare a new variable to be used as argument
				VariablePtr localMem = builder.variable(builder.refType(builder.arrayType(type)));
				DeclarationStmtPtr localDecl = builder.declarationStmt(localMem, builder.callExpr(BASIC.getRefVar(),
								builder.callExpr(BASIC.getArrayCreate1D(), BASIC.getTypeLiteral(type), size)));
				// should I really have access to private members or HostMapper here or is this a compiler bug?
				localMemDecls[kernel].push_back(localDecl);
				arg = localMem;
			}

			const ExpressionPtr& arg2 = node->getArgument(1);
			// check if the index argument is a (casted) integer literal
			const CastExprPtr& cast = dynamic_pointer_cast<const CastExpr>(arg2);
			if(const LiteralPtr& idx = dynamic_pointer_cast<const Literal>(cast ? cast->getSubExpression() : arg2)) {
				// use the literal as index for the argument
				unsigned int pos = atoi(idx->getValue().c_str());
				if(kernelArgs[kernel].size() <= pos)
				kernelArgs[kernel].resize(pos+1);

				kernelArgs[kernel].at(pos) = arg;
			} else {
				// use one argument after another
				kernelArgs[kernel].push_back(arg);
			}
			return builder.intLit(0); // returning CL_SUCCESS
	);

	ADD_Handler(builder, "oclLoadProgSource",
			//			NodePtr ret = node;
			if(const CallExprPtr& callSaC = dynamic_pointer_cast<const CallExpr>(node->getArgument(0))) {
				if(const LiteralPtr& stringAsChar = dynamic_pointer_cast<const Literal>(callSaC->getFunctionExpr())) {
					if(stringAsChar->getValue() == "string.as.char.pointer") {
						if(const LiteralPtr& path = dynamic_pointer_cast<const Literal>(callSaC->getArgument(0))) {
							kernels = loadKernelsFromFile(path->getValue(), builder);

							// set source string to an empty char array
							//							ret = builder.refVar(builder.literal("", builder.arrayType(BASIC.getChar())));
						}
					}
				}
			}
			if(const VariablePtr& pathVar = dynamic_pointer_cast<const Variable>(node->getArgument(0))) {
				//            std::cout << "PathVariable: " << pathVar << std::endl;
				KernelCodeRetriver kcr(pathVar, node, builder);
				visitAll(mProgram, kcr);
				string kernelFilePath = kcr.getKernelFilePath();
				if(kernelFilePath.size() > 0)
				kernels = loadKernelsFromFile(kernelFilePath, builder);
			}
			// set source string to an empty char array
			return builder.refVar(builder.literal("", builder.arrayType(BASIC.getChar())));
	);

	ADD_Handler(builder, "clEnqueueNDRangeKernel",
			// get argument vector
			ExpressionPtr k = node->getArgument(1);
			tryStructExtract(k, builder);
			std::vector<core::ExpressionPtr> args = kernelArgs[k];
			assert(args.size() > 0u && "Cannot find any arguments for kernel function");
			// adding global and local size to the argument vector
			args.push_back(node->getArgument(4) );
			args.push_back(node->getArgument(5) );

			return node;
	);

	ADD_Handler(builder, "clBuildProgram",
			// return cl_success
			return builder.intLit(0);
	);

	// TODO add flags for profiling and out of order
	ADD_Handler(builder, "clGetEventProfilingInfo",
			// return cl_success
			return builder.intLit(0);
	);

	// TODO add syncronization means when adding asynchronous queue
	ADD_Handler(builder, "clFinish",
			// return cl_success
			return builder.intLit(0);
	);

	ADD_Handler(builder, "clWaitForEvents",
			// return cl_success
			return builder.intLit(0);
	);

	// need to release clMem objects
	ADD_Handler(builder, "clReleaseMemObject",
			return builder.callExpr(BASIC.getRefDelete(), node->getArgument(0));
	);

	// all other clRelease calls can be ignored since the variables are removed
	ADD_Handler(builder, "clRelease",
			// return cl_success
			return builder.intLit(0);
	);

	ADD_Handler(builder, "clRetain",
			// return cl_success
			return builder.intLit(0);
	);

	// TODO implement, may have some semantic meaning
	ADD_Handler(builder, "clGetEventInfo",
			LOG(WARNING) << "Removing clGetEventInfo. Check the semantics!";
			// return cl_success
			return builder.intLit(0);
	);

	ADD_Handler(builder, "clFlush",
			// return cl_success
			return builder.intLit(0);
	);

	// TODO maybe a bit too optimisitc?
	ADD_Handler(builder, "clGet",
			// return cl_success
			return builder.intLit(0);
	);

	// TODO need to add exception this when adding image support
	ADD_Handler(builder, "clCreate",
			// return cl_success
			return builder.intLit(0);
	);

	// DEPRECATED, but used in the NVIDIA examples
	ADD_Handler(builder, "clSetCommandQueueProperty",
			// return cl_success
			return builder.intLit(0);
	);

	// exceptions, will be handled in a later step
	ADD_Handler(builder, "clCreateContext", return node;);
	ADD_Handler(builder, "clCreateCommandQueue", return node;);
	ADD_Handler(builder, "clCreateKernel", return node;);
}
;

void copyAnnotations(const NodePtr& source, NodePtr& sink) {
	unsigned int i = 0; // normal iterator and for_each loop leads to infinity loop, go
	for (auto I = source->getAnnotations().begin(); i < source->getAnnotations().size(); ++I, ++i)
		sink->addAnnotation(I->second);
}

core::ExpressionPtr tryDeref(const core::ExpressionPtr& expr, const ASTBuilder& builder) {
	// core::ExpressionPtr retExpr = expr;
	if (core::RefTypePtr&& refTy = core::dynamic_pointer_cast<const core::RefType>(expr->getType())) {
		return builder.callExpr(refTy->getElementType(), BASIC.getRefDeref(), expr);
	}
	return expr;
}

const core::TypePtr getNonRefType(const ExpressionPtr& refExpr) {
	if (const RefTypePtr& ref = dynamic_pointer_cast<const RefType>(refExpr->getType()))
		return ref->getElementType();
	return refExpr->getType();
}

const core::TypePtr getNonRefType(const TypePtr& refType) {
	if (const RefTypePtr& ref = dynamic_pointer_cast<const RefType>(refType))
		return ref->getElementType();
	return refType;
}

HandlerPtr& HostMapper::findHandler(const string& fctName) {
	// for performance reasons working with function prefixes is only enabled for cl* functions
	if (fctName.substr(0, 2).find("cl") == string::npos)
		return handles[fctName];

	// checking function names, starting from the full name
	for (int i = fctName.size(); i > 2; --i) {
		if (HandlerPtr& h = handles[fctName.substr(0,i)])
			return h;
	}

	return handles["break"]; // function is not in map
}

CallExprPtr HostMapper::checkAssignment(const core::NodePtr& oldCall) {
	CallExprPtr newCall;
	if ((newCall = dynamic_pointer_cast<const CallExpr> (oldCall))) {

		// get rid of deref operations, automatically inserted by the frontend coz _cl_mem* is translated to ref<array<...>>, and refs cannot be
		// rhs of an assignment
		if (const CallExprPtr& rhs = dynamic_pointer_cast<const CallExpr>(newCall->getArgument(1))) {
			if (rhs->getFunctionExpr() == BASIC.getRefDeref()) {
				newCall = dynamic_pointer_cast<const CallExpr> (rhs->getArgument(0));
			}
		}
	}
	return newCall;
}

template<typename Enum>
void HostMapper::recursiveFlagCheck(const ExpressionPtr& flagExpr, std::set<Enum>& flags) {
	if (const CallExprPtr& call = dynamic_pointer_cast<const CallExpr>(flagExpr)) {
		// check if there is an lshift -> flag reached
		if (call->getFunctionExpr() == BASIC.getSignedIntLShift() || call->getFunctionExpr() == BASIC.getUnsignedIntLShift()) {
			if (const LiteralPtr& flagLit = dynamic_pointer_cast<const Literal>(call->getArgument(1))) {
				int flag = atoi(flagLit->getValue().c_str());
				if (flag < Enum::size) // last field of enum to be used must be size
					flags.insert(Enum(flag));
				else LOG(ERROR)
					<< "Flag " << flag << " is out of range. Max value is " << CreateBufferFlags::size - 1;
			}
		} else if (call->getFunctionExpr() == BASIC.getSignedIntOr() || call->getFunctionExpr() == BASIC.getUnsignedIntOr()) {
			// two flags are ored, search flags in the arguments
			recursiveFlagCheck(call->getArgument(0), flags);
			recursiveFlagCheck(call->getArgument(1), flags);
		} else LOG(ERROR)
			<< "Unexpected operation in flag argument: " << call << "\nUnable to deduce flags, using default settings";

	}
}

template<typename Enum>
std::set<Enum> HostMapper::getFlags(const ExpressionPtr& flagExpr) {
	std::set<Enum> flags;

	// remove cast to uint<8>
	if (const CastExprPtr cast = dynamic_pointer_cast<const CastExpr>(flagExpr)) {
		recursiveFlagCheck(cast->getSubExpression(), flags);
	} else LOG(ERROR)
		<< "No flags found in " << flagExpr << "\nUsing default settings";

	return flags;
}

bool HostMapper::handleClCreateKernel(const VariablePtr& var, const ExpressionPtr& call, const ExpressionPtr& fieldName) {
	TypePtr type = getNonRefType(var);
	// if it is a struct we have to check the field
	if (const StructTypePtr st = dynamic_pointer_cast<const StructType>(type)) {
		//TODO if one puts more than one kernel inside a struct (s)he should be hit in the face
		if (fieldName) {
			if (const LiteralPtr& fieldLit = dynamic_pointer_cast<const Literal>(fieldName)) {
for_each			(st->getEntries(), [&](StructType::Entry field) {
						if(field.first->getName() == fieldLit->getValue())
						type = field.second;
					});
		} else
		assert(fieldLit && "If the clKernel variable is inside a struct, the fieldName of it has to be passed to handleClCreateKernel");
	}
}
if(type == builder.arrayType(builder.genericType("_cl_kernel"))) {
	if(const CallExprPtr& newCall = dynamic_pointer_cast<const CallExpr>(call)) {//!
		if(const LiteralPtr& fun = dynamic_pointer_cast<const Literal>(newCall->getFunctionExpr()))
		if(fun->getValue() == "clCreateKernel" ) {

			ExpressionPtr kn = newCall->getArgument(1);
			// usually kernel name is enbedded in a "string.as.char.pointer" call"
			if(const CallExprPtr& sacp = dynamic_pointer_cast<const CallExpr>(kn))
			kn = sacp->getArgument(0);
			if(const LiteralPtr& kl = dynamic_pointer_cast<const Literal>(kn)) {
				string name = kl->getValue().substr(1, kl->getValue().length()-2); // delete quotation marks form name

				kernelNames[name] = var; //!
			}

			return true;
		}
	}
}
return false;
}

bool HostMapper::lookForKernelFilePragma(const core::TypePtr& type, const core::ExpressionPtr& createProgramWithSource) {
if(type == builder.refType(builder.arrayType(builder.genericType("_cl_program")))) {
	if(CallExprPtr cpwsCall = dynamic_pointer_cast<const CallExpr>(createProgramWithSource)) {
		if(iocl::KernelFileAnnotationPtr kfa =
				dynamic_pointer_cast<iocl::KernelFileAnnotation>(cpwsCall->getAnnotation(iocl::KernelFileAnnotation::KEY))) {
			string path = kfa->getKernelPath();
			//std::cerr << "Found OpenCL kernel file path: " << path;
			if(cpwsCall->getFunctionExpr() == BASIC.getRefDeref() && cpwsCall->getArgument(0)->getNodeType() == NT_CallExpr)
			cpwsCall = dynamic_pointer_cast<const CallExpr>(cpwsCall->getArgument(0));
			if(const LiteralPtr& clCPWS = dynamic_pointer_cast<const Literal>(cpwsCall->getFunctionExpr())) {
				if(clCPWS->getValue() == "clCreateProgramWithSource") {
					ProgramPtr kernels = loadKernelsFromFile(path, builder);
					for_each(kernels->getEntryPoints(), [&](ExpressionPtr kernel) {
								kernelEntries.push_back(kernel);
							});
				}
			}
		}
	}
	return true;
}
return false;
}

/* crap
 bool HostMapper::translateClCreateBuffer(const VariablePtr& var, const CallExprPtr& fun, const CallExprPtr& newRhs, NodePtr& ret) {
 if(var->getType() == builder.refType(builder.arrayType(builder.genericType("_cl_mem")))) {
 *        if(const LiteralPtr& funLit = dynamic_pointer_cast<const Literal>(fun->getFunctionExpr())){
 std::cout << "++++++++++++VALUE: " << funLit->getValue() << std::endl;
 if(funLit->getValue() == "clCreateBuffer" ) {
 *              TypePtr newType = builder.refType(newRhs->getType());
 // check if variable has already been put into replacement map with a different type
 if(cl_mems[var] != static_cast<long int>(0))
 assert((cl_mems[var]->getType() == newType) && "cl_mem variable allocated several times with different types.");

 const VariablePtr& newVar = builder.variable(newType);
 //                    cl_mems.insert(std::make_pair(lhs, newVar));
 cl_mems[var] = newVar;

 ret = builder.callExpr(BASIC.getRefAssign(), var, newRhs);
 //                copyAnnotations(callExpr, ret);
 std::cout << "returning true\n";
 return true;
 }
 //  }
 //}
 return false;
 }*/

const NodePtr HostMapper::resolveElement(const NodePtr& element) {
// stopp recursion at type level
if (element->getNodeCategory() == NodeCategory::NC_Type) {
	return element;
}

/*    if(const MarkerExprPtr& marker = dynamic_pointer_cast<const MarkerExpr>(element)){
 std::cout << "MarkerExpr: " << marker << std::endl;
 }

 if(const MarkerStmtPtr& marker = dynamic_pointer_cast<const MarkerStmt>(element)) {
 std::cerr << "MarkerStmt: " << marker << std::endl;
 }*/

if(const CallExprPtr& callExpr = dynamic_pointer_cast<const CallExpr>(element)) {
	const ExpressionPtr& fun = callExpr->getFunctionExpr();

	if(const LiteralPtr& literal = dynamic_pointer_cast<const Literal>(fun)) {
		callExpr->substitute(builder.getNodeManager(), *this);
		if(const HandlerPtr& replacement = findHandler(literal->getValue())) {
			NodePtr ret = replacement->handleNode(callExpr);
			// check if new kernels have been created
			vector<ExpressionPtr> kernels = replacement->getKernels();
			if(kernels.size() > 0)
			for_each(kernels, [&](ExpressionPtr kernel) {
						kernelEntries.push_back(kernel);
					});

			copyAnnotations(callExpr, ret);
			return ret;
		}
	}

	if(fun == BASIC.getRefAssign()) {
		// on the left hand side we'll either have a variable or a struct, probably holding a reference to the global array
		// for the latter case we have to handle it differently
		if(const CallExprPtr& cre = dynamic_pointer_cast<const CallExpr>(callExpr->getArgument(0))) {
			if(cre->getFunctionExpr() == BASIC.getCompositeRefElem()) {
				if(cre->getType() == builder.refType(builder.arrayType(builder.genericType("_cl_mem")))) {
					if(const CallExprPtr& newCall = checkAssignment(callExpr->substitute(builder.getNodeManager(), *this))) {
						TypePtr newType = builder.refType(newCall->getType());

						const VariablePtr& struct_ = dynamic_pointer_cast<const Variable>(cre->getArgument(0));
						assert(struct_ && "First argument of compostite.ref.elem has unexpected type, should be a struct variable");
						VariablePtr newStruct;
						// check if struct is already part of the replacement map
						if(cl_mems.find(struct_) != cl_mems.end()) {
							// get the variable out of the struct

							newStruct = cl_mems[struct_];
						} else
						newStruct = struct_;

						LiteralPtr id = dynamic_pointer_cast<const Literal>(cre->getArgument(1));
						assert(id && "Second argument of composite.ref.elem has unexpected type, should be a literal");
						IdentifierPtr toChange = builder.identifier(id->getValue());
						StructTypePtr structType = dynamic_pointer_cast<const StructType>(getNonRefType(newStruct));
						StructType::Entries entries = structType->getEntries(); // actual fields of the struct
						StructType::Entries newEntries;

						for_each(entries, [&](std::pair<IdentifierPtr, TypePtr> entry) {
									if(entry.first == toChange) {
										newEntries.push_back(std::make_pair(entry.first, newType));
									} else {
										newEntries.push_back(entry);
									}
								});

						// update struct in replacement map
						NodePtr&& replacement = builder.variable(builder.structType(newEntries));

						copyAnnotations(struct_, replacement);
						cl_mems[struct_] = dynamic_pointer_cast<const Variable>(replacement);
					}
				}

				if(const CallExprPtr& newCall = checkAssignment(callExpr->substitute(builder.getNodeManager(), *this)))
				if(const VariablePtr& struct_ = dynamic_pointer_cast<const Variable>(cre->getArgument(0)))
				if(handleClCreateKernel(struct_, newCall, cre->getArgument(1)))
				return BASIC.getNoOp();

				if(lookForKernelFilePragma(cre->getType(), callExpr->getArgument(1))) {
					return BASIC.getNoOp();
				}
			}
		}

		if(const VariablePtr& lhs = dynamic_pointer_cast<const Variable>(callExpr->getArgument(0))) {
			// handling clCreateBuffer
			if(lhs->getType() == builder.refType(builder.arrayType(builder.genericType("_cl_mem")))) {
				NodePtr createBuffer = callExpr->substitute(builder.getNodeManager(), *this);
				// check if data has to be copied to a new array
				if(const CallExprPtr& newCall = checkAssignment(createBuffer)) {

					TypePtr newType = builder.refType(newCall->getType());
					// check if variable has already been put into replacement map with a different type
					if(cl_mems[lhs] != static_cast<long int>(0))
					assert((cl_mems[lhs]->getType() == newType) && "cl_mem variable allocated several times with different types.");
					NodePtr ret;

					if(const VariablePtr& var = dynamic_pointer_cast<const Variable>(newCall->getArgument(0))) {
						// use the host variable because CL_MEM_USE_HOST_PTR wasn't set
						cl_mems[lhs] = var;
						// TODO check if err argument has been passed and set variable to 0
						ret = BASIC.getNoOp();
					} else {
						const VariablePtr& newVar = builder.variable(newType);
						cl_mems[lhs] = newVar;
						ret = builder.callExpr(BASIC.getRefAssign(), lhs, newCall);
					}

					copyAnnotations(callExpr, ret);
					return ret;
				}
				// check if we can simply use the existing array
				if(const VariablePtr clMemReplacement = dynamic_pointer_cast<const Variable>(createBuffer)) {
					TypePtr newType = clMemReplacement->getType();

					if(cl_mems[lhs] != static_cast<long int>(0))
					assert((cl_mems[lhs]->getType() == newType) && "cl_mem variable allocated several times with different types.");

					cl_mems[lhs] = clMemReplacement;
					return BASIC.getNoOp();
				}
			}

			if(const CallExprPtr& newCall = checkAssignment(callExpr->substitute(builder.getNodeManager(), *this)))
			if(handleClCreateKernel(lhs, newCall, NULL))
			return BASIC.getNoOp();

			if(lookForKernelFilePragma(lhs->getType(), callExpr->getArgument(1))) {
				return BASIC.getNoOp();
			}
		}

	}
}

if(const DeclarationStmtPtr& decl = dynamic_pointer_cast<const DeclarationStmt>(element)) {
	const VariablePtr& var = decl->getVariable();
	if(var->getType() == builder.refType(builder.arrayType(builder.genericType("_cl_mem")))) {
		if(const CallExprPtr& initFct = dynamic_pointer_cast<const CallExpr>(decl->getInitialization())) {
			if(const LiteralPtr& literal = core::dynamic_pointer_cast<const core::Literal>(initFct->getFunctionExpr())) {
				if(literal->getValue() == "clCreateBuffer") { // clCreateBuffer is called at definition of cl_mem variable
					const CallExprPtr& newInit = dynamic_pointer_cast<const CallExpr>(this->resolveElement(initFct));

					// check if the variable was created with CL_MEM_USE_HOST_PTR flag and can be removed
					if(const VariablePtr replacement = dynamic_pointer_cast<const Variable>(newInit->getArgument(0))) {
						cl_mems[var] = replacement; // TODO check if error argument has been set and set error to CL_SUCCESS
						return BASIC.getNoOp();
					}

					//DeclarationStmtPtr newDecl = dynamic_pointer_cast<const DeclarationStmt>(decl->substitute(builder.getNodeManager(), *this));
					TypePtr newType = builder.refType(newInit->getType());

					NodePtr newDecl = builder.declarationStmt(var, builder.refNew(newInit));
					const VariablePtr& newVar = builder.variable(newType);

					cl_mems[var] = newVar;

					copyAnnotations(decl, newDecl);
					return newDecl;
				}
			}
		}
		assert(decl->getInitialization()->getNodeType() == NT_CallExpr && "Unexpected initialization of cl_mem variable");
	}

	lookForKernelFilePragma(var->getType(), decl->getInitialization());

	if(handleClCreateKernel(var, decl->getInitialization(), NULL)) {
		return BASIC.getNoOp();
	}

}

NodePtr ret = element->substitute(builder.getNodeManager(), *this);
copyAnnotations(element, ret);
return ret;
}

void Host2ndPass::mapNamesToLambdas(const vector<ExpressionPtr>& kernelEntries)
{
std::map<string, int> checkDuplicates;

for_each(kernelEntries, [&](ExpressionPtr entryPoint) {
			if(const LambdaExprPtr& lambdaEx = dynamic_pointer_cast<const LambdaExpr>(entryPoint))
			if(auto cname = lambdaEx->getLambda()->getAnnotation(c_info::CNameAnnotation::KEY)) {
				assert(checkDuplicates[cname->getName()] == 0 && "Multiple kernels with the same name not supported");
				checkDuplicates[cname->getName()] = 1;

				if(ExpressionPtr clKernel = kernelNames[cname->getName()]) {
					kernelLambdas[clKernel] = lambdaEx;
				}
			}
		});
}

ClmemTable& Host2ndPass::getCleanedStructures() {
for_each(cl_mems, [&](std::pair<const VariablePtr, VariablePtr>& var) {
			if(StructTypePtr type = dynamic_pointer_cast<const StructType>(var.second->getType())) {
				// delete all unneccessary cl_* fields form the struct
				StructType::Entries entries = type->getEntries(); // actual fields of the struct
				StructType::Entries newEntries;

				for_each(entries, [&](std::pair<IdentifierPtr, TypePtr>& entry) {
							if(entry.second->toString().find("_cl_") == string::npos) {
								newEntries.push_back(entry);
							}
						});

				// update struct in replacement map
				NodePtr replacement = builder.variable(builder.structType(newEntries));
				copyAnnotations(var.second, replacement);
				var.second = static_pointer_cast<const Variable>(replacement);
				//			cl_mems[var.first] = var.second;//dynamic_pointer_cast<const Variable>(replacement);
			}
		});

	return cl_mems;
}

void HostMapper3rdPass::getVarOutOfCrazyInspireConstruct(core::ExpressionPtr& arg) {
// remove stuff added by (void*)&
if(const CallExprPtr& refToAnyref = dynamic_pointer_cast<const CallExpr>(arg))
if(refToAnyref->getFunctionExpr() == BASIC.getRefToAnyRef())
if(const CallExprPtr& scalarToArray = dynamic_pointer_cast<const CallExpr>(refToAnyref->getArgument(0)))
if(scalarToArray->getFunctionExpr() == BASIC.getScalarToArray())
arg = scalarToArray->getArgument(0);

}

/* Assumptions:
 * 1. the work dimension is a scalar in the arguments
 * 2. The cast to void* of the local/global size happens in the argument
 */

const ExpressionPtr HostMapper3rdPass::anythingToVec3(ExpressionPtr workDim, ExpressionPtr size) {
const TypePtr vecTy = builder.vectorType(BASIC.getUInt4(), builder.concreteIntTypeParam(static_cast<size_t>(3)));
TypePtr argTy;
VariablePtr param;
ExpressionPtr arg;
unsigned int wd;

if(const CastExprPtr& cast = dynamic_pointer_cast<const CastExpr>(workDim)) {
	workDim = cast->getSubExpression();
}

// check work dimension
const LiteralPtr& dim = dynamic_pointer_cast<const Literal>(workDim);
assert(dim && "Cannot determine work_dim of clEnqueueNDRangeKernel. Should be a literal!");
wd = atoi(dim->getValue().c_str());
//    std::cout << "*****************WorkDim: " << dim->getValue() << std::endl;
assert(workDim < 3u && "Invalid work_dim. Should be 1 - 3!");

// check if there is a x to array called
if(const CallExprPtr& toArray = dynamic_pointer_cast<const CallExpr>(size)) {
	if(toArray->getFunctionExpr() == BASIC.getScalarToArray()) {
		// check consitency with workDim, should be 1
		assert(wd == 1 && "Scalar group size passed to a multi dimensional work_dim");

		argTy = toArray->getArgument(0)->getType();
		param = builder.variable(argTy);
		arg = toArray->getArgument(0);
	} else if(toArray->getFunctionExpr() == BASIC.getRefVectorToRefArray()) {
		argTy = toArray->getArgument(0)->getType();
		param = builder.variable(argTy);
		arg = toArray->getArgument(0);
	} else {
		std::cerr << "Unexpected Function: " << toArray << " of type " << toArray->getArgument(0)->getType() << std::endl;
		assert(false && "Unexpected function in OpenCL size argument");
	}
} else { // the argument is an array
	size = tryDeref(size, builder);
	assert(size->getType()->getNodeType() == NT_ArrayType && "Called clEnqueueNDRangeKernel with invalid group argument");
	argTy = size->getType();
	param = builder.variable(argTy);
	arg = size;
}

ExpressionPtr init = param;

if(RefTypePtr ref = dynamic_pointer_cast<const RefType>(param->getType())) {
	init = builder.deref(param);
	//        argTy = ref->getElementType();
}

TypePtr fieldTy;
if(const ArrayTypePtr& array = dynamic_pointer_cast<const ArrayType>(init->getType()))
fieldTy = array->getElementType();

if(const VectorTypePtr& vector = dynamic_pointer_cast<const VectorType>(init->getType()))
fieldTy = vector->getElementType();

DeclarationStmtPtr vDecl;
if(wd == 1) {
	if(fieldTy)
	init = builder.callExpr(fieldTy, BASIC.getArraySubscript1D(), init, builder.literal(BASIC.getUInt8(), "0"));
	if(init->getType() != BASIC.getUInt4()) {
		init = builder.castExpr(BASIC.getUInt4(), init);
	}
	vDecl = builder.declarationStmt(vecTy,
			builder.vectorExpr(toVector<ExpressionPtr>(init, builder.literal(BASIC.getUInt4(), "1"), builder.literal(BASIC.getUInt4(), "1"))));
} else {
	assert(fieldTy && "Size argument of multidimensional group is no vector or array");

	vector<ExpressionPtr> subscripts;
	subscripts.push_back(builder.callExpr(fieldTy, BASIC.getArraySubscript1D(), init, builder.literal(BASIC.getUInt8(), "0")));
	subscripts.push_back(builder.callExpr(fieldTy, BASIC.getArraySubscript1D(), init, builder.literal(BASIC.getUInt8(), "1")));
	subscripts.push_back(wd == 3 ? (ExpressionPtr)builder.callExpr(fieldTy, BASIC.getArraySubscript1D(), init, builder.literal(BASIC.getUInt8(), "2")) :
			(ExpressionPtr)builder.literal(BASIC.getUInt4(), "1"));

	for_each(subscripts, [&](ExpressionPtr& r) {
				if(r->getType() != BASIC.getUInt4())
				r = builder.castExpr(BASIC.getUInt4(), r);
			});

	vDecl = builder.declarationStmt(vecTy, builder.vectorExpr(subscripts));
}

FunctionTypePtr fctTy = builder.functionType(toVector(argTy), vecTy);
return builder.callExpr(vecTy, builder.lambdaExpr(fctTy, toVector(param) , builder.compoundStmt(vDecl,
						builder.returnStmt(vDecl->getVariable()))), arg);

}

const NodePtr HostMapper3rdPass::resolveElement(const NodePtr& element) {
	// stopp recursion at type level
	if (element->getNodeCategory() == NodeCategory::NC_Type) {
		return element;//->substitute(builder.getNodeManager(), *this);
	}

	if(const VariablePtr& var = dynamic_pointer_cast<const Variable>(element)) {
		if(cl_mems.find(var) != cl_mems.end()) {
			return cl_mems[var];
		}
	}

	if(const DeclarationStmtPtr& decl = dynamic_pointer_cast<const DeclarationStmt>(element)) {
		const VariablePtr& var = decl->getVariable();
		if(cl_mems.find(var) != cl_mems.end()) {
			if(const StructTypePtr& sType = dynamic_pointer_cast<const StructType>(cl_mems[var]->getType())) {
				// throw elements which are not any more in the struct out of the initialization expression
				// look into ref.new
				if(const CallExprPtr& refNew = dynamic_pointer_cast<const CallExpr>(decl->getInitialization())) {
					if(refNew->getFunctionExpr() == BASIC.getRefNew() || refNew->getFunctionExpr() == BASIC.getRefVar()) {
						if(const StructExprPtr& oldInit = dynamic_pointer_cast<const StructExpr>(refNew->getArgument(0))) {
							core::StructExpr::Members newInitMembers;
							core::NamedCompositeType::Entries newMembers = sType->getEntries();
							size_t i = 0;

							for_each(oldInit->getMembers(), [&](core::StructExpr::Member oldInitMember) {
								// assuming that the order of the (exisiting) elements in newMembers and oldMember is the same,
								// we always have to compare only one element
								if(oldInitMember.first == newMembers.at(i).first) {
									// check if the type of the init expression is the same as the type of the field (type of field may changed)
									if(newMembers.at(i).second != oldInitMember.second->getType()) {
										// always init as undefined in this case
										const TypePtr& initType = /*getNonRefType*/(newMembers.at(i).second);
										core::StructExpr::Member newInitMember = std::make_pair(oldInitMember.first,
												builder.callExpr(BASIC.getUndefined(), BASIC.getTypeLiteral(initType)));
										newInitMembers.push_back(newInitMember);
									} else
										newInitMembers.push_back(oldInitMember);
									++i;
								}
							});
							// create a new Declaration Statement which's init expression contains only the remaining fields
							return builder.declarationStmt(cl_mems[var], builder.structExpr(newInitMembers));
						}
					}
				}
			}

			if(const CallExprPtr& initFct = dynamic_pointer_cast<const CallExpr>(this->resolveElement(decl->getInitialization()))) {
				if(initFct->getArgument(0) == builder.callExpr(BASIC.getUndefined(), BASIC.getTypeLiteral(builder.arrayType(builder.genericType("_cl_mem"))))) {
					// overwrite default initialization to cl_mem with default initialization to array<whatever>
					TypePtr newType;
					if(const RefTypePtr& rt = dynamic_pointer_cast<const RefType>(cl_mems[var]->getType()))
					newType = rt->getElementType();
					else
					newType = cl_mems[var]->getType();
					NodePtr ret = builder.declarationStmt(cl_mems[var], builder.refNew(builder.callExpr(BASIC.getUndefined(), BASIC.getTypeLiteral(newType))));
					copyAnnotations(decl, ret);
					return ret;
				}
			}
		} else {
// remove delarations of opencl type variables. Should not be used any more
			if(var->getType()->toString().find("_cl_") != string::npos)
				return BASIC.getNoOp();
		}

	}

	if(const CallExprPtr& callExpr = dynamic_pointer_cast<const CallExpr>(element)) {
		const ExpressionPtr& fun = callExpr->getFunctionExpr();
		if(fun == BASIC.getRefAssign()) {
			// get rid of some not needed functions
			if(CallExprPtr rhs = dynamic_pointer_cast<const CallExpr>(callExpr->getArgument(1))) {
				// check if it is embedded in a ref.deref
				if(const CallExprPtr& deref = dynamic_pointer_cast<const CallExpr>(rhs->getArgument(0)))
					if(rhs->getFunctionExpr() == BASIC.getRefDeref())
						rhs = deref;
				if(rhs->getFunctionExpr()->toString() == "clCreateContext")
					return BASIC.getNoOp();
				if(rhs->getFunctionExpr()->toString() == "clCreateCommandQueue")
					return BASIC.getNoOp();

			}
		}

		if(const CallExprPtr& newCall = dynamic_pointer_cast<const CallExpr>(callExpr->substitute(builder.getNodeManager(), *this))) {
			if(const LiteralPtr& fun = dynamic_pointer_cast<const Literal>(newCall->getFunctionExpr())) {
				if(fun->getValue() == "clEnqueueNDRangeKernel" ) {
					// get kernel function
					ExpressionPtr k = callExpr->getArgument(1);

					// check if argument is a call to composite.ref.elem
					if(const CallExprPtr cre = dynamic_pointer_cast<const CallExpr>(k))
					k = cre->getArgument(0);

					// get corresponding lambda expression
					LambdaExprPtr lambda = kernelLambdas[k];
					assert(lambda && "No lambda expression for kernel call found");
					vector<ExpressionPtr>& args = kernelArgs[k];
					assert(args.size() > 0 && "No arguments for call to kernel function found");

					// make a three element vector out of the global and local size
					const ExpressionPtr global = anythingToVec3(newCall->getArgument(2), newCall->getArgument(4));
					const ExpressionPtr local = anythingToVec3(newCall->getArgument(2), newCall->getArgument(5));

					vector<ExpressionPtr> newArgs;
					// construct call to kernel function
					if(localMemDecls[k].size() == 0) {
						// if there is no local memory in argument, the arguments can simply be copyied
						for_each(args, [&](ExpressionPtr& arg) {
									//global and private memory arguments must be variables
									getVarOutOfCrazyInspireConstruct(arg);

									newArgs.push_back(dynamic_pointer_cast<const Expression>(this->resolveElement(arg)));
								});

						// add global and local size to arguments
						newArgs.push_back(global);
						newArgs.push_back(local);

						NodePtr kernelCall = builder.callExpr(BASIC.getInt4(), lambda, newArgs);
						copyAnnotations(callExpr, kernelCall);

						return kernelCall;
					}
					// add declarations for argument local variables if any, warping a function around it

					vector<StatementPtr> declsAndKernelCall;
					for_each(localMemDecls[k], [&](DeclarationStmtPtr decl) {
								declsAndKernelCall.push_back(decl);
							});

					vector<VariablePtr> params;
					vector<ExpressionPtr> innerArgs;
					vector<TypePtr> wrapperInterface;

					for_each(args, [&](ExpressionPtr& arg) {
								bool local = false;
								//global and private memory arguments must be variables
								getVarOutOfCrazyInspireConstruct(arg);

								// local args are declared in localMemDecls
								for_each(localMemDecls[k], [&](DeclarationStmtPtr decl) {
											if(arg == decl->getVariable()) {
												//                                params.push_back(decl->getVariable());
												// will be declared inside wrapper function
												local = true;
											}
										});
								if(!local) {
									// global and private memory arguments will be passed to the wrapper function as agrument
									ExpressionPtr newArg = dynamic_pointer_cast<const Expression>(this->resolveElement(arg));
									assert(newArg && "Argument of kernel function must be an Expression");
									newArgs.push_back(newArg);
									wrapperInterface.push_back(newArg->getType());

									// kernel funtion will take a new variable as argument
									params.push_back(builder.variable(newArg->getType()));
								}
								// the kernel call will use the params of the outer call as arguments, they must be an expression
								innerArgs.push_back(params.back());
							});

					// add global and local size to arguments
					TypePtr vec3type = builder.vectorType(BASIC.getUInt4(), builder.concreteIntTypeParam(static_cast<size_t>(3)));
					newArgs.push_back(global);
					VariablePtr globalVar = builder.variable(vec3type);
					params.push_back(globalVar);
					innerArgs.push_back(globalVar);
					wrapperInterface.push_back(vec3type);

					newArgs.push_back(local);
					VariablePtr localVar = builder.variable(vec3type);
					params.push_back(localVar);
					innerArgs.push_back(localVar);
					wrapperInterface.push_back(vec3type);

					NodePtr kernelCall = builder.callExpr(BASIC.getInt4(), lambda, innerArgs);
					copyAnnotations(callExpr, kernelCall);

					declsAndKernelCall.push_back(dynamic_pointer_cast<const Statement>(kernelCall));
					const FunctionTypePtr& wrapperType = builder.functionType(wrapperInterface, BASIC.getInt4());
					return builder.callExpr(builder.lambdaExpr(builder.lambda(wrapperType, params, builder.compoundStmt(declsAndKernelCall))), newArgs);
				}
			}

			if(newCall->getFunctionExpr() == BASIC.getCompositeRefElem()) {
				// replace variable with new version if necessary
				const VariablePtr& newStruct = dynamic_pointer_cast<const Variable>(newCall->getArgument(0));
				const VariablePtr& oldStruct = dynamic_pointer_cast<const Variable>(callExpr->getArgument(0));
				assert(oldStruct && newStruct && "First argument of composite.ref.elem must be a struct variable");
				if(newStruct != oldStruct) { // struct variable has been replaced, may need to update type of composite.ref.elem
					const TypePtr& newType = dynamic_pointer_cast<const StructType>(newStruct->getType());
					assert(newType && "First argument of composite.ref.elem must be a struct variable");

					const LiteralPtr& oldIdLit = dynamic_pointer_cast<const Literal>(callExpr->getArgument(1));
					const LiteralPtr& oldTypeLit = dynamic_pointer_cast<const Literal>(callExpr->getArgument(2));
					assert(oldIdLit && oldTypeLit && "Second and third argument of composite.ref.elem must be a literals");
					const IdentifierPtr& oldId = builder.identifier(oldIdLit->getValue());
					const TypePtr& oldType = dynamic_pointer_cast<const GenericType>(oldTypeLit->getType())->getTypeParameter().at(0);

					const core::TypePtr& memberTy = static_pointer_cast<const NamedCompositeType>(newType)->getTypeOfMember(oldId);

					if(!memberTy) { // someone requested a field which has been removed from the struct -> should have been deleted
											return newCall; // TODO remove debug return
						LOG(ERROR) << "Call returning a " << oldType << " has not been removed\n";
						assert(memberTy && "Function tries to store cl_* type in a struct");
					}

					if(memberTy != oldType) { // need to update the type argument of the call

						NodePtr retExpr = builder.callExpr( builder.refType(memberTy), BASIC.getCompositeRefElem(),
								toVector<ExpressionPtr>(newCall->getArgument(0), BASIC.getIdentifierLiteral(oldId), BASIC.getTypeLiteral(memberTy) ));

						copyAnnotations(newCall, retExpr);
						return retExpr;

					}

				}

			}
			return newCall;
		}

	}

	NodePtr ret = element->substitute(builder.getNodeManager(), *this);
	copyAnnotations(element, ret);
	return ret;
}

}

ProgramPtr HostCompiler::compile() {
	//    HostVisitor oclHostVisitor(builder, mProgram);
	HostMapper oclHostMapper(builder, mProgram);

	const ProgramPtr& interProg = dynamic_pointer_cast<const core::Program>(oclHostMapper.mapElement(0, mProgram));
	assert(interProg && "First pass of OclHostCompiler corrupted the program");

	LOG(INFO) << "Adding " << oclHostMapper.getKernelArgs().size() << " kernels to host Program... ";

	const vector<ExpressionPtr>& kernelEntries = oclHostMapper.getKernels();

	const ProgramPtr& progWithKernels = interProg->addEntryPoints(builder.getNodeManager(), interProg, kernelEntries);

	Host2ndPass oh2nd(oclHostMapper.getKernelNames(), oclHostMapper.getClMemMapping(), builder);
	oh2nd.mapNamesToLambdas(kernelEntries);

	ClmemTable cl_mems = oh2nd.getCleanedStructures();
	HostMapper3rdPass ohm3rd(builder, cl_mems, oclHostMapper.getKernelArgs(), oclHostMapper.getLocalMemDecls(), oh2nd.getKernelNames(),
		oh2nd.getKernelLambdas());

	/*	if(core::ProgramPtr newProg = dynamic_pointer_cast<const core::Program>(ohm3rd.mapElement(0, progWithKernels))) {
	 mProgram = newProg;
	 return newProg;
	 } else
	 assert(newProg && "Second pass of OclHostCompiler corrupted the program");
	 */

	NodePtr fu = ohm3rd.mapElement(0, progWithKernels);

	insieme::utils::map::PointerMap<NodePtr, NodePtr> tmp;// = reinterpret_cast<insieme::utils::map::PointerMap<NodePtr, NodePtr> >(oclHostMapper.getClMemMapping());
	for_each(cl_mems, [&](std::pair<const VariablePtr, VariablePtr> t){
		tmp[t.first] = t.second;
		if(dynamic_pointer_cast<const StructType>(t.second->getType())) {
			// replacing the types of all structs with the same type. Should get rid of cl_* stuff in structs
			// HIGHLY experimental and untested
			tmp[t.first->getType()] = t.second->getType();
//			std::cout << "Replacing ALL \n" << t.first << "\nwith\n" << t.second << "\n";
		}
	});


	if(core::ProgramPtr newProg = dynamic_pointer_cast<const core::Program>(core::transform::replaceAll(builder.getNodeManager(), fu, tmp))) {
	mProgram = newProg;
	//			return newProg;
	} else
	assert(newProg && "Second pass of OclHostCompiler corrupted the program");

	return mProgram;
}

} //namespace ocl
} //namespace frontend
} //namespace insieme
