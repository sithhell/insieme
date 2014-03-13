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

#include <set>
#include <map>
#include <string>

#include "insieme/utils/printable.h"

namespace insieme {
namespace driver {
namespace integration {

	using std::set;
	using std::map;
	using std::string;

	class PropertyView;

	class Properties : public utils::Printable {

		// key / category / value
		map<string,map<string,string>> data;

	public:

		const string& get(const string& key, const string& category = "", const string& def = "") const;

		std::set<string> getKeys() const;

		void set(const string& key, const string& value) {
			set(key,"",value);
 		}

		void set(const string& key, const string& category, const string& value) {
			data[key][category] = value;
		}

		const string& operator[](const string& key) const {
			return get(key);
		}

		bool empty() const {
			return data.empty();
		}

		std::size_t size() const {
			return data.size();
		}

		string mapVars(const string& in) const;

		PropertyView getView(const string& category) const;

		std::ostream& printTo(std::ostream& out) const;

		bool operator==(const Properties& other) const {
			return data == other.data;
		}

		Properties& operator<<=(const Properties& other);

		Properties operator<<(const Properties& other) const {
			return Properties(*this) <<= other;
		}

		/**
		 * Loads properties from the given input stream.
		 */
		static Properties load(std::istream& in);

		/**
		 * Dumps this properties to the output stream.
		 */
		void store(std::ostream& out) const;

	};


	class PropertyView : public utils::Printable {

		const Properties& properties;

		const string category;

	public:

		PropertyView(const Properties& properties, const string& category)
			: properties(properties), category(category) {}

		string operator[](const string& key) const {
			return properties.get(key, category);
		}

		string get(const string& key, const string& def = "") const {
			return properties.get(key, category, def);
		}

		std::ostream& printTo(std::ostream& out) const;
	};


} // end namespace integration
} // end namespace driver
} // end namespace insieme
