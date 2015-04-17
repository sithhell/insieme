/**
 * Copyright (c) 2002-2015 Distributed and Parallel Systems Group,
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

#include <map>
#include <string>
#include <vector>
#include <list>
#include <memory>

#include <boost/filesystem/path.hpp>

#include "insieme/core/forward_decls.h"
#include "insieme/core/ir_program.h"

#include "insieme/frontend/tu/ir_translation_unit.h"

#include "insieme/frontend/extensions/frontend_extension.h"

#include "insieme/utils/printable.h"

namespace insieme {

namespace driver {
namespace cmd {
namespace detail {
    class OptionParser;
}
}
}

namespace frontend {

	using std::map;
	using std::set;
	using std::list;
	using std::shared_ptr;
	using std::vector;
	using std::string;


	// a definition for the kind of path to be utilized by the following translation unit
	typedef boost::filesystem::path path;

	/**
	 * An entity describing a unit of work for the clang frontend conversion process.
	 */
	class ConversionSetup {
	public:

		/**
		 * A list of options to adjust the translation unit conversion.
		 */
		enum Option {
			PrintDiag		= 1<<0,
			WinCrossCompile	= 1<<1,
			TAG_MPI			= 1<<2,
			ProgressBar		= 1<<3,
			NoWarnings		= 1<<4,
			NoDefaultExtensions = 1<<5
		};

		/**
		 * A list of supported standards.
		 */
		enum Standard {
			Auto, C99, Cxx98, Cxx03, Cxx11
		};

		/**
		 * The default frontend configuration.
		 */
		static const unsigned DEFAULT_FLAGS;

	private:

		/**
		 * A list of include directories to be considered.
		 */
		vector<path> includeDirs;

		/**
		 * A list of include directories containing system headers.
		 */
		vector<path> systemHeaderSearchPath;

		/**
		 * The C standard to be followed.
		 */
		Standard standard;

		/**
		 * A list of definitions to be passed to the preprocessor. Each
		 * entry maps an identifier to a value.
		 */
		map<string,string> definitions;

		/**
		 * A list of string representing the regular expression to be intercepted
		 * by default "std::.*" and "__gnu_cxx::.*" are intercepted
		 */
		set<string> interceptedNameSpacePatterns;

		/**
		 * A list of include directories containing intercepted headers.
		 */
		vector<path> interceptedHeaderDirs;

		/**
		 * A list of include directories containing system headers for a cross compilation.
		 */
		string crossCompilationSystemHeadersDir;

        /**
         * A list of optimization flags (-f flags) that need to be used at least in the
         * backend compiler
         */
        set<string> fflags;

		/**
		 * Additional flags - a bitwise boolean combination of Options (see Option)
		 */
		unsigned flags;

        /**
		 * A vector of pairs. Each pair contains a frontend extension pointer and a
		 * lambda that was retrieved from the extension. This lambda will decide
		 * if the plugin gets registered and the plugin will be configured by the
		 * lambda.
		 */
        typedef std::shared_ptr<extensions::FrontendExtension> FrontendExtensionPtr;
        std::vector<std::pair<FrontendExtensionPtr, extensions::FrontendExtension::flagHandler>> extensions;


	public:

        /**
         *  A list that contains all user extensions that have been registered
         */
         std::list<FrontendExtensionPtr> extensionList;

		/**
		 * Creates a new setup covering the given include directories.
		 */
		ConversionSetup(const vector<path>& includeDirs = vector<path>());

		/**
		 * Allows to check for an option.
		 */
		bool hasOption(const Option option) const {
			return flags & option;
		}

		/**
		 * Updates the state of an option.
		 */
		void setOption(const Option option, bool status = true) {
			flags = (status)?(flags | option):( flags & ~option);
		}

		/**
		 * Updates the options set for the conversion process.
		 */
		void setOptions(unsigned options) {
			flags = options;
		}

		/**
		 * Obtains the standard to be used for parsing input files.
		 */
		const Standard& getStandard() const {
			return standard;
		}

		/**
		 * Updates the standard to be used for parsing input files.
		 */
		void setStandard(const Standard& standard);

		/**
		 * Obtains a reference to the currently defined definitions.
		 */
		const map<string,string>& getDefinitions() const {
			return definitions;
		}

		/**
		 * Updates the definitions to be used by the conversion process.
		 */
		void setDefinitions(const map<string,string>& definitions) {
			this->definitions = definitions;
		}

		/**
		 * Adds a pre-processor definition to this conversion job.
		 */
		void setDefinition(const string& name, const string& value = "") {
			this->definitions[name] = value;
		}

		/**
		 * Obtains a reference to the covered set of include directories.
		 */
		const vector<path>& getIncludeDirectories() const {
			return includeDirs;
		}

		/**
		 * Updates the set of considered include directories.
		 */
		void setIncludeDirectories(const vector<path>& includeDirectories) {
			this->includeDirs = includeDirectories;
		}

		/**
		 * Adds an additional include directory.
		 */
		void addIncludeDirectory(const path& directory) {
			this->includeDirs.push_back(directory);
		}

		/**
		 * Obtains a reference to the covered set of std-library include directories.
		 */
		const vector<path>& getSystemHeadersDirectories() const {
			return systemHeaderSearchPath;
		}

		/**
		 * Updates the set of considered std-library include directories.
		 */
		void setSystemHeadersDirectories(const vector<path>& includeDirectories) {
			this->systemHeaderSearchPath = includeDirectories;
		}

		/**
		 * Adds an additional user defined header serach path
		 */
		void addSystemHeadersDirectory(const path& directory) {
			this->systemHeaderSearchPath.push_back(directory);
		}

		/**
		 * Adds a single regular expression string to the interception set
		 */
		void addInterceptedNameSpacePattern(const string& pattern) {
			this->interceptedNameSpacePatterns.insert(pattern);
		}

		/**
		 * Adds a single regular expression string to the interception set
		 */
		template<typename List>
		void addInterceptedNameSpacePatterns(const List& patterns) {
			for(const auto& cur : patterns) {
				addInterceptedNameSpacePattern(cur);
			}
		}

		/**
		 * Obtains a reference to the currently defined interceptions.
		 */
		const set<string>& getInterceptedNameSpacePatterns() const {
			return interceptedNameSpacePatterns;
		}

		/**
		 * Obtains a reference to the covered set of include directories.
		 */
		const vector<path>& getInterceptedHeaderDirs() const {
			return interceptedHeaderDirs;
		}

		/**
		 * Updates the set of considered include directories.
		 */
		void setInterceptedHeaderDirs(const vector<path>& interceptedHeaderDirs) {
			this->interceptedHeaderDirs = interceptedHeaderDirs;
		}

		/**
		 * Adds an additional include directory.
		 */
		void addInterceptedHeaderDir(const path& directory) {
			this->interceptedHeaderDirs.push_back(directory);
		}

		/**
		 * Obtains a reference to the covered set of include directories.
		 */
		const string& getCrossCompilationSystemHeadersDir() const {
			return crossCompilationSystemHeadersDir;
		}

		/**
		 * Updates the set of considered include directories.
		 */
		void setCrossCompilationSystemHeadersDir(const string& crossCompilationSystemHeadersDir) {
			this->crossCompilationSystemHeadersDir = crossCompilationSystemHeadersDir;
		}

        /**
         * Adds a single optimization flag
         */
        void addFFlag(const string& flag) {
            this->fflags.insert(flag);
        }

        /**
         * Obtains a reference to the currently defined f flags
         */
        const set<string>& getFFlags() const {
            return fflags;
        }

		/**
		 * A utility method to determine whether the given file should be
		 * considered a C++ file or not. This decision will be influenced
		 * by the standard set within this setup. Only if set to auto
		 * (default) the extension will be evaluated.
		 */
		bool isCxx(const path& file) const;

        /**
         *  Frontend extension initialization method
         */
        void frontendExtensionInit(const ConversionJob& job);

        /**
         *  Insert a new frontend extension. This DOES NOT mean that the
         *  extension gets registered. The registerFlag method will return a
         *  lambda that is called in frontendExtensionInit. This lambda decides
         *  if the extension is registered. If the extension does not override the
         *  registerFlag method it will be registered by default.
         */
        template <class T>
        void registerFrontendExtension(driver::cmd::detail::OptionParser& optParser) {
            auto extensionPtr = std::make_shared<T>();
			extensions.push_back( { extensionPtr, extensionPtr->registerFlag(optParser) } );
        };

        /**
         *  Return the list of all registered frontend extensions
         */
        const std::list<FrontendExtensionPtr>& getExtensions() const {
            return extensionList;
        };
	};


	class ConversionJob : public ConversionSetup, public insieme::utils::Printable {

		/**
		 * The translation units to be converted.
		 */
		vector<path> files;

		/**
		 * Extra libraries to be considered for the conversion.
		 */
		vector<tu::IRTranslationUnit> libs;

	public:

		/**
		 * Creates an empty conversion job covering no files.
		 */
		ConversionJob() : ConversionSetup(vector<path>()) {}

		/**
		 * Creates a new conversion job covering a single file.
		 */
		ConversionJob(const path& file, const vector<path>& includeDirs = vector<path>())
			: ConversionSetup(includeDirs), files(toVector(file)) {
        }

		/**
		 * Creates a new conversion job covering the given files.
		 */
		ConversionJob(const vector<path>& files, const vector<path>& includeDirs = vector<path>())
			: ConversionSetup(includeDirs), files(files) {
			assert_false(files.empty());

            // The user defined headers path is extended with c source files directories
            auto inc = ConversionSetup::getIncludeDirectories();
            for(auto cur : files) {
                inc.push_back(cur.parent_path());
            }
            ConversionSetup::setIncludeDirectories(inc);
		}

		/**
		 * Obtains the one input files covered by this conversion job.
		 */
		const vector<path>& getFiles() const {
			return files;
		}

		/**
		 * Adds an additional file to this conversion job.
		 */
		void addFile(const path& file) {
			files.push_back(file);
		}

		/**
		 * Exchanges the files covered by this conversion job by the given files.
		 */
		void setFiles(const vector<path>& files) {
			this->files = files;
		}

		/**
		 * Obtains a reference to the libs to be considered by this conversion job.
		 */
		const vector<tu::IRTranslationUnit>& getLibs() const {
			return libs;
		}

		/**
		 * Sets the libs to be considered by this conversion job.
		 */
		void setLibs(const vector<tu::IRTranslationUnit>& libs) {
			this->libs = libs;
		}

		/**
		 * Appends a library to the list of libraries considered by this conversion job.
		 */
		void addLib(const tu::IRTranslationUnit& unit) {
			libs.push_back(unit);
		}

		/**
		 * Determines whether this conversion job is processing a C++ file or not.
		 */
		bool isCxx() const;

		/**
		 * Triggers the actual conversion. The previously set up parameters will be used to attempt a conversion.
		 * Automatically determines whether the supplied program contains multiple entry points.
		 *
		 * @param manager the node manager to be used for building the IR
		 * @return the resulting, converted program
		 * @throws an exception if the conversion fails.
		 */
		core::ProgramPtr execute(core::NodeManager& manager) const;

		/**
		 * Triggers the actual conversion. The previously set up parameters will be used to attempt a conversion.
		 *
		 * @param manager the node manager to be used for building the IR
		 * @param fullApp a flag determining whether the result is expected to be a full application (entered via
		 * 				a main function) or a list of multiple entry points.
		 * @return the resulting, converted program
		 * @throws an exception if the conversion fails.
		 */
		core::ProgramPtr execute(core::NodeManager& manager, bool fullApp) const;

		/**
		 * Triggers the actual conversion. The previously set up parameters will be used to attempt a conversion.
		 *
		 * @param manager the node manager to be used for building the IR
		 * @param program the partially processed program without any post-processing steps
		 * @param the conversion setup holding any post-processing steps to be applied (i.e. extensions)
		 * @return the resulting, converted program
		 * @throws an exception if the conversion fails.
		 */
		core::ProgramPtr execute(core::NodeManager& manager, core::ProgramPtr& program, ConversionSetup& setup) const;

		/**
		 * Triggers the conversion of the files covered by this job into a translation unit.
		 *
		 * @param manager the node manager to be used for building the IR
		 * @return the resulting, converted program
		 * @throws an exception if the conversion fails.
		 */
		tu::IRTranslationUnit toIRTranslationUnit(core::NodeManager& manager) const;

        void registerExtensionFlags(driver::cmd::detail::OptionParser& optParser);

		/**
		 *  Prints the conversion setup
		 **/
		 std::ostream& printTo(std::ostream& out) const;
	};


	/**
	 * Used to report a parsing error occurred during the parsing of the input file
	 */
	struct ClangParsingError: public std::logic_error {
		ClangParsingError(const path& file_name): std::logic_error(file_name.string()) { }
	};


} // end namespace frontend
} // end namespace insieme
