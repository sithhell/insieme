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

#include <algorithm>
#include <functional>
#include <string>
#include <cstring>
#include <sstream>
#include <iterator>

#include "functional_utils.h"

using std::string;

string format(const char* formatString, ...);

template<typename T>
string toString(const T& value) {
	std::stringstream res;
	res << value;
	return res.str();
}

/**
 * A utility method to split a string along its white spaces.
 *
 * @param str the string to be splitted
 * @return the vector of substrings
 */
inline std::vector<string> split(const string& str) {
	using namespace std;
	vector<string> tokens;
	istringstream iss(str);
	copy(istream_iterator<string>(iss),
	         istream_iterator<string>(),
	         back_inserter<vector<string> >(tokens));
	return tokens;
}

/**
 * This functor can be used to print elements to an output stream.
 */
template<typename Extractor>
struct print : public std::binary_function<std::ostream&, const typename Extractor::argument_type&, std::ostream&> {
	Extractor extractor;
	std::ostream& operator()(std::ostream& out, const typename Extractor::argument_type& cur) const {
		return out << extractor(cur);
	}
};

template<typename Iterator, typename Printer>
struct Joinable {

	const string separator;
	const Iterator begin;
	const Iterator end;
	const Printer& printer;

	Joinable(const string& separator, const Iterator& begin, const Iterator& end, const Printer& printer) :
		separator(separator), begin(begin), end(end), printer(printer) {};
};


template<typename Container, typename Printer>
std::ostream& operator<<(std::ostream& out, const Joinable<Container, Printer>& joinable) {
	if (joinable.begin != joinable.end) {
		auto iter = joinable.begin;
		joinable.printer(out, *iter);
		++iter;
		std::for_each(iter, joinable.end, [&](const typename Container::value_type& cur) {
			out << joinable.separator;
			joinable.printer(out, cur);
		});
	}
	return out;
}

/**
 * Joins the values in the collection to the stream separated by a supplied separator.
 **/
template<typename Container, typename Printer>
Joinable<typename Container::const_iterator, Printer> join(const string& separator, const Container& container, const Printer& printer) {
	return Joinable<typename Container::const_iterator ,Printer>(separator, container.cbegin(), container.cend(), printer);
}

template<typename Container>
Joinable<typename Container::const_iterator, print<id<const typename Container::value_type&>>> join(const string& separator, const Container& container) {
	return Joinable<typename Container::const_iterator, print<id<const typename Container::value_type&>>>(separator, container.cbegin(), container.cend(), print<id<const typename Container::value_type&>>());
}

template<typename Iterator, typename Printer>
Joinable<Iterator, Printer> join(const string& separator, const Iterator& begin, const Iterator& end, const Printer& printer) {
	return Joinable<Iterator ,Printer>(separator, begin, end, printer);
}

template<typename Iterator>
Joinable<Iterator, print<id<const typename std::iterator_traits<Iterator>::value_type&>>> join(const string& separator, const Iterator& begin, const Iterator& end) {
	return Joinable<Iterator , print<id<const typename std::iterator_traits<Iterator>::value_type&>>>(separator, begin, end, print<id<const typename std::iterator_traits<Iterator>::value_type&>>());
}

template<typename Iterator, typename Printer>
Joinable<Iterator, Printer> join(const string& separator, const std::pair<Iterator, Iterator>& range, const Printer& printer) {
	return Joinable<Iterator ,Printer>(separator, range.first, range.second, printer);
}

template<typename Iterator>
Joinable<Iterator, print<id<const typename std::iterator_traits<Iterator>::value_type&>>> join(const string& separator, const std::pair<Iterator, Iterator>& range) {
	return Joinable<Iterator , print<id<const typename std::iterator_traits<Iterator>::value_type&>>>(separator, range.first, range.second, print<id<const typename std::iterator_traits<Iterator>::value_type&>>());
}

template<typename SeparatorFunc, typename Container, typename Printer>
void functionalJoin(SeparatorFunc seperatorFunc, Container container, Printer printer) {
	if(container.size() > 0) {
		auto iter = container.cbegin();
		printer(*iter);
		++iter;
		std::for_each(iter, container.cend(), [&](const typename Container::value_type& cur) {
			seperatorFunc();
			printer(cur);
		});
	}
}

template<typename Element, typename Printer>
struct Multiplier {

	const Element& element;
	const unsigned times;
	const Printer& printer;
	const string& separator;

	Multiplier(const Element& element, unsigned times, const Printer& printer, const string& separator) :
		element(element), times(times), printer(printer), separator(separator) {};
};

template<typename Element, typename Printer>
std::ostream& operator<<(std::ostream& out, const Multiplier<Element, Printer>& multiplier) {
	if (multiplier.times > 0) {
		multiplier.printer(out, multiplier.element);
	}
	for (unsigned i = 1; i<multiplier.times; i++) {
		out << multiplier.separator;
		multiplier.printer(out, multiplier.element);
	}

	return out;
}

template<typename Element, typename Printer = print<id<Element>>>
Multiplier<Element, Printer> times(const Element& element, unsigned times, const string& separator = "", const Printer& printer = Printer()) {
	return Multiplier<Element, Printer>(element, times, printer, separator);
}

