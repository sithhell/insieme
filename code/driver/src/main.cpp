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

#include <iostream>
#include <memory>
#include <algorithm>
#include <fstream>

#define MIN_CONTEXT 40

#include "insieme/core/ast_statistic.h"
#include "insieme/core/checks/ir_checks.h"
#include "insieme/core/printer/pretty_printer.h"

#include "insieme/simple_backend/simple_backend.h"
#include "insieme/opencl_backend/opencl_convert.h"
#include "insieme/simple_backend/rewrite.h"

#include "insieme/analysis/cfg.h"

#include "insieme/c_info/naming.h"

#include "insieme/utils/container_utils.h"
#include "insieme/utils/string_utils.h"
#include "insieme/utils/cmd_line_utils.h"
#include "insieme/utils/logging.h"
#include "insieme/utils/timer.h"

#include "insieme/frontend/program.h"
#include "insieme/frontend/omp/omp_sema.h"

#include "insieme/driver/dot_printer.h"

#include "insieme/xml/xml_utils.h"

using namespace std;
using namespace insieme::utils::log;

namespace fe = insieme::frontend;
namespace core = insieme::core;
namespace xml = insieme::xml;
namespace analysis = insieme::analysis;


bool checkForHashCollisions(const ProgramPtr& program);


int main(int argc, char** argv) {

	CommandLineOptions::Parse(argc, argv);
	Logger::get(std::cerr, DEBUG);

	LOG(INFO) << "Insieme compiler";

	core::NodeManager manager;
	core::ProgramPtr program = core::Program::create(manager);
	try {
		if(!CommandLineOptions::InputFiles.empty()) {
			auto inputFiles = CommandLineOptions::InputFiles;
			// LOG(INFO) << "Parsing input files: ";
			// std::copy(inputFiles.begin(), inputFiles.end(), std::ostream_iterator<std::string>( std::cout, ", " ) );
			fe::Program p(manager);
			insieme::utils::Timer clangTimer("Frontend.load [clang]");
			p.addTranslationUnits(inputFiles);
			clangTimer.stop();
			LOG(INFO) << clangTimer;

			// do the actual clang to IR conversion
			insieme::utils::Timer convertTimer("Frontend.convert ");
			program = p.convert();
			convertTimer.stop();
			LOG(INFO) << convertTimer;

			// perform some performance benchmarks
			if (CommandLineOptions::BenchmarkCore) {

				// Benchmark pointer-based visitor
				insieme::utils::Timer visitPtrTime("Benchmark.IterateAll.Pointer ");
				int count = 0;
				core::visitAll(program, core::makeLambdaPtrVisitor([&](const NodePtr& cur) {
					count++;
				}, true));
				visitPtrTime.stop();
				LOG(INFO) << visitPtrTime;
				LOG(INFO) << "Number of nodes: " << count;

				// Benchmark address based visitor
				insieme::utils::Timer visitAddrTime("Benchmark.IterateAll.Address ");
				count = 0;
				core::visitAll(core::ProgramAddress(program), core::makeLambdaAddressVisitor([&](const NodeAddress& cur) {
					count++;
				}, true));
				visitAddrTime.stop();
				LOG(INFO) << visitAddrTime;
				LOG(INFO) << "Number of nodes: " << count;

				// Benchmark empty-substitution operation
				insieme::utils::Timer substitutionTime("Benchmark.NodeSubstitution ");
				count = 0;
				NodeMapping* h;
				auto mapper = makeLambdaMapper([&](unsigned, const NodePtr& cur)->NodePtr {
					count++;
					return cur->substitute(manager, *h);
				});
				h = &mapper;
				mapper.map(0,program);
				substitutionTime.stop();
				LOG(INFO) << substitutionTime;
				LOG(INFO) << "Number of modifications: " << count;

				// Benchmark empty-substitution operation (non-types only)
				insieme::utils::Timer substitutionTime2("Benchmark.NodeSubstitution.Non-Types ");
				count = 0;
				NodeMapping* h2;
				auto mapper2 = makeLambdaMapper([&](unsigned, const NodePtr& cur)->NodePtr {
					if (cur->getNodeCategory() == NC_Type) {
						return cur;
					}
					count++;
					return cur->substitute(manager, *h2);
				});
				h2 = &mapper2;
				mapper2.map(0,program);
				substitutionTime2.stop();
				LOG(INFO) << substitutionTime2;
				LOG(INFO) << "Number of modifications: " << count;
			}

			// Dump CFG
			if(!CommandLineOptions::CFG.empty()) {
				insieme::utils::Timer timer("Build.CFG");
				std::fstream dotFile(CommandLineOptions::CFG.c_str(), std::fstream::out | std::fstream::trunc);
				analysis::CFGPtr graph = analysis::CFG::buildCFG(program);
				timer.stop();
				dotFile << *graph;
				LOG(INFO) << timer;
			}

			// perform checks
			MessageList errors;
			auto checker = [&]() {
				LOG(INFO) << "=========================== IR Semantic Checks ==================================";
				insieme::utils::Timer timer("Checks");
				errors = check(program, insieme::core::checks::getFullCheck());
				std::sort(errors.begin(), errors.end());
				for_each(errors, [](const Message& cur) {
					LOG(INFO) << cur << std::endl;
					NodeAddress address = cur.getAddress();
					stringstream ss;
					unsigned contextSize = 1;
					do {
						ss.str(""); 
						ss.clear();
						NodePtr context = address.getParentNode(min((unsigned)contextSize, address.getDepth()-contextSize));
						ss << insieme::core::printer::PrettyPrinter(context, insieme::core::printer::PrettyPrinter::OPTIONS_SINGLE_LINE, 1+2*contextSize);
					} while(ss.str().length() < MIN_CONTEXT && contextSize++ < 5);
					LOG(INFO) << "\t Context: " << ss.str();
				});
				timer.stop();
				LOG(INFO) << timer;
				LOG(INFO) << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~";
			};

			// used for verifying quality of node-hashing
//			if (!checkForHashCollisions(program)) {
//				return -1;
//			}


			if(CommandLineOptions::CheckSema) {
				checker();
			}

			// run OMP frontend
			if(CommandLineOptions::OMPSema) {
				LOG(INFO) << "============================= OMP conversion ====================================";
				insieme::utils::Timer ompTimer("OMP");
				program = fe::omp::applySema(program,  manager);
				ompTimer.stop();
				LOG(INFO) << ompTimer;
				LOG(INFO) << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~";
				// check again
				if(CommandLineOptions::CheckSema) {
					checker();
				}
			}

			// IR statistics
			LOG(INFO) << "============================ IR Statistics ======================================";
			LOG(INFO) << "\n" << ASTStatistic::evaluate(program);
			LOG(INFO) << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~";

			// Creates dot graph of the generated IR
			if(!CommandLineOptions::ShowIR.empty()) {
				insieme::utils::Timer timer("Show.graph");
				std::fstream dotFile(CommandLineOptions::ShowIR.c_str(), std::fstream::out | std::fstream::trunc);
				insieme::driver::printDotGraph(program, errors, dotFile);
				timer.stop();
				LOG(INFO) << timer;
			}

			// XML dump
			if(!CommandLineOptions::DumpXML.empty()) {
				LOG(INFO) << "================================== XML DUMP =====================================";
				insieme::utils::Timer timer("Xml.dump");
				xml::XmlUtil::write(program, CommandLineOptions::DumpXML);
				timer.stop();
				LOG(INFO) << timer;
				LOG(INFO) << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~";
			}
		}
		if(!CommandLineOptions::LoadXML.empty()) {
			LOG(INFO) << "================================== XML LOAD =====================================";
			insieme::utils::Timer timer("Xml.load");
			NodePtr&& xmlNode= xml::XmlUtil::read(manager, CommandLineOptions::LoadXML);
			program = core::dynamic_pointer_cast<const Program>(xmlNode);
			assert(program && "Loaded XML doesn't represent a valid program");
			timer.stop();
			LOG(INFO) << timer;
			LOG(INFO) << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~";
		}

		if(CommandLineOptions::PrettyPrint || !CommandLineOptions::DumpIR.empty()) {
			// a pretty print of the AST
			LOG(INFO) << "========================= Pretty Print INSPIRE ==================================";
			if(!CommandLineOptions::DumpIR.empty()) {
				// write into the file
				std::fstream fout(CommandLineOptions::DumpIR,  std::fstream::out | std::fstream::trunc);
				fout << "// -------------- Pretty Print Inspire --------------" << std::endl;
				fout << insieme::core::printer::PrettyPrinter(program);
				fout << std::endl << std::endl << std::endl;
				fout << "// --------- Pretty Print Inspire - Detail ----------" << std::endl;
				fout << insieme::core::printer::PrettyPrinter(program, insieme::core::printer::PrettyPrinter::OPTIONS_DETAIL);
			} else
				LOG(INFO) << insieme::core::printer::PrettyPrinter(program);
			// LOG(INFO) << "====================== Pretty Print INSPIRE Detail ==============================";
			// LOG(INFO) << insieme::core::printer::PrettyPrinter(program, insieme::core::printer::PrettyPrinter::OPTIONS_DETAIL);
			// LOG(INFO) << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~";
		}

		if (CommandLineOptions::OpenCL) {
            insieme::utils::Timer timer("OpenCL.Backend");

            LOG(INFO) << "========================= Converting to OpenCL ===============================";

//TODO find the OpenCLChecker
//			insieme::opencl_backend::OpenCLChecker oc;
//			LOG(INFO) << "Checking OpenCL compatibility ... " << (oc.check(program) ? "OK" : "ERROR\nInput program cannot be translated to OpenCL!");

            if(!CommandLineOptions::Output.empty()) {
                insieme::backend::Rewriter::writeBack(program, insieme::backend::ocl::convert(program), CommandLineOptions::Output);
            } else {
                auto converted = insieme::backend::ocl::convert(program);
                LOG(INFO) << *converted;
            }

            timer.stop();
            LOG(INFO) << timer;
		} else {
			insieme::utils::Timer timer("Simple.Backend");

			LOG(INFO) << "========================== Converting to C++ =================================";

			if(!CommandLineOptions::Output.empty()) {
				insieme::backend::Rewriter::writeBack(program, insieme::simple_backend::convert(program), CommandLineOptions::Output);
			} else {
				auto converted = insieme::simple_backend::convert(program);
				LOG(INFO) << *converted;
			}

			timer.stop();
			LOG(INFO) << timer;
		}

	} catch (fe::ClangParsingError& e) {
		cerr << "Error while parsing input file: " << e.what() << endl;
	}

}

// ------------------------------------------------------------------------------------------------------------------
//                                     Hash code evaluation
// ------------------------------------------------------------------------------------------------------------------
typedef std::size_t hash_t;

void hash_node(hash_t& seed, const NodePtr& cur) {
	boost::hash_combine(seed, cur->getNodeType());
	switch(cur->getNodeType()) {
		case insieme::core::NT_GenericType:
		case insieme::core::NT_ArrayType:
		case insieme::core::NT_VectorType:
		case insieme::core::NT_ChannelType:
		case insieme::core::NT_RefType:  {
			const GenericTypePtr& type = static_pointer_cast<const GenericType>(cur);
			boost::hash_combine(seed, type->getFamilyName().getName());
			for_each(type->getIntTypeParameter(), [&](const core::IntTypeParam& cur) {
				boost::hash_combine(seed, insieme::core::hash_value(cur));
			});
			break;
		}
		case insieme::core::NT_Variable:     { boost::hash_combine(seed, static_pointer_cast<const Variable>(cur)->getId()); break; }
		case insieme::core::NT_TypeVariable: { boost::hash_combine(seed, static_pointer_cast<const TypeVariable>(cur)->getVarName()); break; }
		case insieme::core::NT_Literal:      { boost::hash_combine(seed, static_pointer_cast<const Literal>(cur)->getValue()); break; }
		case insieme::core::NT_MemberAccessExpr:      { boost::hash_combine(seed, static_pointer_cast<const MemberAccessExpr>(cur)->getMemberName().getName()); break; }
		default: {}
	}
}

hash_t computeHash(const NodePtr& cur) {
	hash_t seed = 0;
	hash_node(seed, cur);
	for_each(cur->getChildList(), [&](const NodePtr& child) {
		boost::hash_combine(seed, computeHash(child));
	});
	return seed;
}


bool checkForHashCollisions(const ProgramPtr& program) {

	// create a set of all nodes
	insieme::utils::set::PointerSet<NodePtr> allNodes;
	insieme::core::visitAllOnce(program, insieme::core::makeLambdaPtrVisitor([&allNodes](const NodePtr& cur) {
		allNodes.insert(cur);
	}, true));

	// evaluate hash codes
	LOG(INFO) << "Number of nodes: " << allNodes.size();
	std::map<std::size_t, NodePtr> hashIndex;
	int collisionCount = 0;
	for_each(allNodes, [&](const NodePtr& cur) {
		// try inserting node
		std::size_t hash = cur->hash();
		//std::size_t hash = boost::hash_value(cur->toString());
		//std::size_t hash = ::computeHash(cur);

		auto res = hashIndex.insert(std::make_pair(hash, cur));
		if (!res.second) {
			LOG(INFO) << "Hash Collision detected: \n"
					  << "   Hash code:     " << hash << "\n"
					  << "   First Element: " << *res.first->second << "\n"
					  << "   New Element:   " << *cur << "\n"
					  << "   Equal:         " << ((*cur==*res.first->second)?"true":"false") << "\n";
			collisionCount++;
		}
	});
	LOG(INFO) << "Number of Collisions: " << collisionCount;

	// terminate main program
	return false;

}
