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

#include "insieme/core/ast_node.h"

#include "insieme/core/transform/node_replacer.h"

#include "insieme/frontend/ocl/ocl_host_compiler.h"
#include "insieme/frontend/ocl/ocl_host_1st_pass.h"
#include "insieme/frontend/ocl/ocl_host_2nd_pass.h"
#include "insieme/frontend/ocl/ocl_host_3rd_pass.h"

namespace ba = boost::algorithm;

//#include "insieme/core/ast_visitor.h"
#include "insieme/annotations/ocl/ocl_annotations.h"
#include "insieme/annotations/c/naming.h"

namespace insieme {
namespace frontend {
namespace ocl {
using namespace insieme::core;


/*class fuVisitor: public core::ASTVisitor<void> {
	void visitNode(const NodePtr& node) {
		if(insieme::annotations::ocl::KernelFileAnnotationPtr kfa =
				dynamic_pointer_cast<insieme::annotations::ocl::KernelFileAnnotation>(node->getAnnotation(insieme::annotations::ocl::KernelFileAnnotation::KEY))) {
			std::cout << "Found kernel file Pragma at node \n" << node << std::endl;
		}
	}
public:
	fuVisitor(): ASTVisitor<void>(true) {}
};
*/
ProgramPtr HostCompiler::compile() {
	//    HostVisitor oclHostVisitor(builder, mProgram);
	HostMapper oclHostMapper(builder, mProgram);

//	fuVisitor FAKK;
//	visitDepthFirst(mProgram,FAKK);

	const ProgramPtr& interProg = dynamic_pointer_cast<const core::Program>(oclHostMapper.mapElement(0, mProgram));
	assert(interProg && "First pass of OclHostCompiler corrupted the program");

	if(oclHostMapper.getnKernels() == 0) {
		LOG(INFO) << "No OpenCL kernel functions found";
		return mProgram;
	}
	LOG(INFO) << "Adding " << oclHostMapper.getnKernels() << " OpenCL kernels to host Program... ";

	const vector<ExpressionPtr>& kernelEntries = oclHostMapper.getKernels();

	const ProgramPtr& progWithEntries = interProg->addEntryPoints(builder.getNodeManager(), interProg, kernelEntries);
	const ProgramPtr& progWithKernels = core::Program::remEntryPoints(builder.getNodeManager(), progWithEntries, kernelEntries);

	Host2ndPass oh2nd(oclHostMapper.getKernelNames(), oclHostMapper.getClMemMapping(), builder);
	oh2nd.mapNamesToLambdas(kernelEntries);

	ClmemTable cl_mems = oh2nd.getCleanedStructures();
	HostMapper3rdPass ohm3rd(builder, cl_mems, oclHostMapper.getKernelArgs(), oclHostMapper.getLocalMemDecls(), oh2nd.getKernelNames(),
		oh2nd.getKernelLambdas(), oclHostMapper.getReplacements());

	/*	if(core::ProgramPtr newProg = dynamic_pointer_cast<const core::Program>(ohm3rd.mapElement(0, progWithKernels))) {
	 mProgram = newProg;
	 return newProg;
	 } else
	 assert(newProg && "Second pass of OclHostCompiler corrupted the program");
	 */

	NodePtr fu = ohm3rd.mapElement(0, progWithKernels);

	utils::map::PointerMap<NodePtr, NodePtr>& tmp = oclHostMapper.getReplacements();
/*	for_each(cl_mems, [&](std::pair<const VariablePtr, VariablePtr> t){
		tmp[t.first] = t.second;
		if(dynamic_pointer_cast<const StructType>(t.second->getType())) {
			// replacing the types of all structs with the same type. Should get rid of cl_* stuff in structs
			// HIGHLY experimental and untested
			tmp[t.first->getType()] = t.second->getType();
//			std::cout << "Replacing ALL \n" << t.first << "\nwith\n" << t.second << "\n";
		}
	});
*/
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
