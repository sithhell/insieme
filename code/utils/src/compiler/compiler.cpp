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

#include "insieme/utils/compiler/compiler.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include <boost/filesystem.hpp>

#include "insieme/utils/container_utils.h"
#include "insieme/utils/config.h"
#include "insieme/common/env_vars.h"

namespace insieme {
namespace utils {
namespace compiler {

	const char* getDefaultCCompilerExecutable() {
		const char* envVar = std::getenv(INSIEME_C_BACKEND_COMPILER);
		if(envVar != nullptr) return envVar;

		return INSIEME_C_BACKEND_COMPILER_CMAKE;
	}

	const char* getDefaultCxxCompilerExecutable() {
		const char* envVar = std::getenv(INSIEME_CXX_BACKEND_COMPILER);
		if(envVar != nullptr) return envVar;

		return INSIEME_CXX_BACKEND_COMPILER_CMAKE;
	}

	namespace fs = boost::filesystem;

	Compiler Compiler::getDefaultC99Compiler() {
		Compiler res(getDefaultCCompilerExecutable());
		res.addFlag("-x c");
		res.addFlag("-Wall");
		res.addFlag("--std=gnu99");
		return res;
	}

	Compiler Compiler::getDefaultCppCompiler() {
		Compiler res(getDefaultCxxCompilerExecutable());
		res.addFlag("-x c++");
		res.addFlag("-lstdc++");
		res.addFlag("-Wall");
		res.addFlag("--std=c++14");
		res.addFlag("-fpermissive");
		res.addFlag("-Wno-write-strings");
		return res;
	}

	Compiler Compiler::getRuntimeCompiler(const Compiler& base) {
		Compiler res = base;
		res.addFlag(string("-I ") + utils::getInsiemeSourceRootDir() + "runtime/include -I " + utils::getInsiemeSourceRootDir() + "common/include -D_XOPEN_SOURCE=700 -D_GNU_SOURCE -ldl -lrt -lpthread -lm");
		return res;
	}

	Compiler Compiler::getOpenCLCompiler(const Compiler& base) {
		Compiler res = getRuntimeCompiler(base);
		res.addFlag("-lOpenCL -DIRT_ENABLE_OPENCL -DIRT_ENABLE_ASSERTS");
		res.addFlag(string("-I ") + utils::getOpenCLRootDir() + "include/");
		res.addFlag(string("-L ") + utils::getOpenCLRootDir() + "lib64/");
		return res;
	}

	Compiler Compiler::getOptimizedCompiler(const Compiler& base, const string& level) {
		Compiler res = base;
		res.addFlag("-O" + level);
		return res;
	}

	Compiler Compiler::getDebugCompiler(const Compiler& base, const string& level) {
		Compiler res = base;
		res.addFlag("-g" + level);
		return res;
	}

	string Compiler::getCommand(const vector<string>& inputFiles, const string& outputFile) const {
		// build up compiler command
		std::stringstream cmd;

		// some flags are known to be required to be place before the source file
		vector<string> before;
		vector<string> after;

		// split up flags
		for(auto cur : flags) {
			// the -x option has to be before the input file
			if(cur[0] == '-' && cur[1] == 'x') {
				before.push_back(cur);
			} else {
				after.push_back(cur);
			}
		}

		cmd << executable;
		cmd << " " << join(" ", before);
		cmd << " " << join(" ", inputFiles);
		cmd << " " << join(" ", after);
		cmd << (incDirs.empty() ? "" : " -I") << join(" -I", incDirs);
		cmd << (libs.getPaths().empty() ? "" : " -L") << join(" -L", libs.getPaths());
		cmd << (libs.getLibs().empty() ? "" : " -l") << join(" -l", libs.getLibs());
		if(outputFile.size()) cmd << " -o " << outputFile;

		// redirect streams if compilation should be 'silent'
		if(silent) { cmd << " > /dev/null 2>&1"; }

		return cmd.str();
	}

	const vector<string> getDefaultIncludePaths(string cmd) {
		vector<string> paths;
		char line[256];
		FILE* file = POPEN_WRAPPER(cmd.c_str(), "r");
		if(file == NULL) { return paths; }
		bool capture = false;
		string input;
		string startPrefix("#include <...> search starts here:");
		string stopPrefix("End of search list.");
		while(fgets(line, 256, file)) {
			input = string(line);
			//#include <...> search starts here:
			if(input.substr(0, startPrefix.length()) == startPrefix) {
				capture = true;
				// leave this line out
				continue;
			}
			// End of Search list.
			if(input.substr(0, stopPrefix.length()) == stopPrefix) {
				capture = false;
				// stop after this line
				break;
			}

			if(capture) {
				input.replace(input.begin(), input.begin() + 1, "");
				input.replace(input.end() - 1, input.end(), "/");
				paths.push_back(input);
			}
		}
		PCLOSE_WRAPPER(file);
		if(paths.empty()) {
			std::cerr << "ATTENTION: No default include paths found.\nEnsure that the currently set backend compiler is working.\nTerminal local language has to be set to en_XX.\n";
			std::cerr << "Failed command was: " << cmd << "\n";
		}
		return paths;
	}

	/**
	 * Calls the backend compiler for C (gcc) to determine the include paths
	 * @ return vector with the include paths
	 */
	const vector<string> getDefaultCIncludePaths() {
		auto compiler = Compiler::getDefaultC99Compiler();
		std::stringstream ss;
		ss << "echo | ";
		ss << compiler.getExecutable();
		ss << " -v -xc -E - 2>&1";
		return getDefaultIncludePaths(ss.str());
	}

	/**
	 * Calls the backend compiler for C++ (g++) to determine the include paths
	 * @ return vector with the include paths
	 */
	const vector<string> getDefaultCppIncludePaths() {
		auto compiler = Compiler::getDefaultCppCompiler();
		std::stringstream ss;
		ss << "echo | ";
		ss << compiler.getExecutable();
		ss << " -v -xc++ -E - 2>&1";
		return getDefaultIncludePaths(ss.str());
	}

	namespace {

		fs::path getTemporaryFile() {
			// create temporary target file name
			fs::path targetFile = fs::unique_path(fs::temp_directory_path() / "insieme-trg-%%%%%%%%");
			LOG(DEBUG) << "Using temporary file " << targetFile << " as a target file for compilation.";
			return targetFile;
		}


	}

	bool compile(const vector<string>& sourcefile, const string& targetfile, const Compiler& compiler) {
		string&& cmd = compiler.getCommand(sourcefile, targetfile);
		LOG(INFO) << "Compiling with: " << cmd << std::endl;
		int res = system(cmd.c_str());
		if(res) {
			std::cerr << "Command line:\n\t" << cmd << std::endl;
			std::cerr << "Failure with exit status " << res << std::endl;
		}
		return res == 0;
	}

	bool compile(const string& sourcefile, const string& targetfile, const Compiler& compiler) {
		vector<string> files(1);
		files[0] = sourcefile;
		return compile(files, targetfile, compiler);
	}

	string compile(const string& sourcefile, const Compiler& compiler) {
		auto targetFile = getTemporaryFile();
		if (compile(sourcefile, targetFile.string(), compiler)) {
			return targetFile.string();
		}
		assert_fail() << "Unable to compile given input file!";
		return "";
	}


	bool compile(const VirtualPrintable& source, const Compiler& compiler) {
		string target = compileToBinary(source, compiler);
		if(target.empty()) { return false; }

		// delete target file
		if(boost::filesystem::exists(target)) { boost::filesystem::remove(target); }

		return true;
	}

	string compileToBinary(const VirtualPrintable& source, const Compiler& compiler) {
		auto targetFile = getTemporaryFile();
		if(compileToBinary(source, targetFile.string(), compiler)) { return targetFile.string(); }
		return string();
	}


	bool compileToBinary(const VirtualPrintable& source, const string& targetFile, const Compiler& compiler) {
		// create a temporary source file
		fs::path sourceFile = fs::unique_path(fs::temp_directory_path() / "insieme-src-%%%%%%%%");
		LOG(DEBUG) << "Using temporary file " << sourceFile << " as a source file for compilation.";

		// write source to file
		std::fstream srcFile(sourceFile.string(), std::fstream::out);
		srcFile << source << "\n";
		srcFile.close();

		// perform compilation
		bool success = compile(sourceFile.string(), targetFile, compiler);

		// delete source file - only if compilation was a success
		if(boost::filesystem::exists(sourceFile)) {
			if(success) {
				boost::filesystem::remove(sourceFile);
			} else {
				std::cerr << "Offending source code can be found in " << sourceFile << std::endl;
			}
		}

		return success;
	}

	bool isOpenCLAvailable() {
		// -1 not available
		//  0 unknown, check is pending
		// +1 available
		static int result = 0;
		// do we need to check for the first time?
		if (result == 0) {
			// assume that it is not available
			result = -1;
			// obtain an instance of the ocl compiler and set it to silent mode as
			// we do not want to nag the user with the output
			auto compiler = Compiler::getOpenCLCompiler();
			compiler.addFlag("-E");
			compiler.setSilent();
			// generate a unique file in the temp directory and let the compiler resolve cl.h
			auto path = boost::filesystem::unique_path(boost::filesystem::temp_directory_path() / "insieme-ocl-%%%%%%%%.h").string();
			std::fstream file(path, std::fstream::out);
			file << "#include <CL/cl.h>";
			file.close();
			// pass in an empty string as we do not want to generate an additional file
			auto retVal = system(compiler.getCommand({path}, "").c_str());
			if (retVal >= 0 && WEXITSTATUS(retVal) == 0) result = 1;
			// remove the temporary file as it is not needed anymore
			boost::filesystem::remove(path);
		}
		return result > 0;
	}

} // end namespace compiler
} // end namespace utils
} // end namespace insieme
