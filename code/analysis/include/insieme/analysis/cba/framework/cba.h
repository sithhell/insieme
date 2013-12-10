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
#include <tuple>
#include <unordered_map>

#include <boost/noncopyable.hpp>

#include "insieme/analysis/cba/framework/_forward_decl.h"
#include "insieme/analysis/cba/framework/analysis_type.h"
#include "insieme/analysis/cba/framework/context.h"
#include "insieme/analysis/cba/framework/constraint_generator.h"
#include "insieme/analysis/cba/framework/call_site_manager.h"
#include "insieme/analysis/cba/framework/call_string_filter.h"

#include "insieme/analysis/cba/framework/entities/callable.h"
#include "insieme/analysis/cba/framework/entities/location.h"
#include "insieme/analysis/cba/framework/entities/data_path.h"

#include "insieme/analysis/cba/utils/cba_utils.h"

#include "insieme/core/ir.h"
#include "insieme/core/ir_address.h"
#include "insieme/core/ir_visitor.h"
#include "insieme/core/printer/pretty_printer.h"

#include "insieme/utils/typed_map.h"
#include "insieme/utils/lazy.h"

namespace insieme {
namespace analysis {
namespace cba {

	using std::tuple;
	using std::map;

	namespace sc = insieme::utils::constraint;



	// TODO: find a better place for this too

	// ----------------- imperative analysis ---------------

	// forward declaration
	class StateSetType;

	template<typename Context, typename ElementSetType>
	class ImperativeInStateConstraintGenerator;

	template<typename Context, typename ElementSetType>
	class ImperativeOutStateConstraintGenerator;


	// since every state set type is a global constant we can use plain pointer
	typedef const StateSetType* StateSetTypePtr;

	/**
	 * The state set type is a special type of set type referencing
	 * sets attached to pairs of statements and locations (standard
	 * sets are only attached to statements).
	 */
	class StateSetType : public utils::Printable {

		/**
		 * The name of this set for printing and debugging issues.
		 */
		const string name;

	public:

		StateSetType(const string& name) : name(name) {}

	public:

		const string& getName() const {
			return name;
		}

		bool operator==(const StateSetType& other) const {
			// the identity of a set type is fixed by its address
			return this == &other;
		}

		bool operator!=(const StateSetType& other) const {
			return !(*this == other);
		}

		std::ostream& printTo(std::ostream& out) const {
			return out << name;
		}
	};

	// TODO: make those an enum
	extern const StateSetType Sin;		// in-state of statements
	extern const StateSetType Sout;		// out-state of statements
	extern const StateSetType Stmp;		// temporary states of statements (assignment only)


	template<typename A> struct location_data_in_analysis;
	template<typename A> struct location_data_out_analysis;
	template<typename A> struct location_data_tmp_analysis;



	// fix the types of indices utilized for fields, elements and tuple-components
	// TODO: make index types generic!
	typedef NominalIndex<
			core::StringValuePtr,
			compare_target<core::StringValuePtr>,
			equal_target<core::StringValuePtr>,
			hash_target<core::StringValuePtr>,
			print<deref<core::StringValuePtr>>
	> FieldIndex;

	typedef SingleIndex ElementIndex; 		// for arrays / vectors
	typedef FieldIndex ComponentIndex; 		// for tuples

	typedef sc::ValueID ValueID;

	class CBA : public boost::noncopyable {

		typedef sc::LazySolver Solver;

		struct ContainerBase {
			virtual ~ContainerBase() {}
		};

		template<typename Context>
		class Container : public ContainerBase {

			utils::Lazy<std::vector<Context>> contexts;
			utils::Lazy<std::vector<Location<Context>>> locations;
			map<std::size_t, std::vector<Callable<Context>>> callables;

		public:

			const std::vector<Context>& getContexts(CBA& cba) {
				if (contexts) return contexts;

				// create list of valid call contexts
				vector<typename Context::call_context> callContexts = generateSequences<Context::call_ctxt_size>(cba.getDynamicCallLabels(),
						[&](const std::array<Label,Context::call_ctxt_size>& cur) {
							return cba.isValid(cur);
						}
				);

				contexts = std::vector<Context>();
				for(const auto& cur : callContexts) {
					contexts->push_back(Context(cur));
				}

				return contexts;
			}

			const std::vector<Location<Context>>& getLocations(CBA& cba) {
				if (locations) return locations;

				locations = std::vector<Location<Context>>();

				// collect all memory location constructors
				core::visitDepthFirst(cba.getRoot(), [&](const core::ExpressionAddress& cur) {

					// TODO: filter contexts - not all locations may occur in all contexts
					// (this will reduce the number of sets / constraints)

					if (isMemoryConstructor(cur)) {
						for(const auto& ctxt : this->getContexts(cba)) {
							// TODO: move location creation utility to location constructor
							auto loc = getLocation<Context>(cur, ctxt);
							if (!contains(*locations, loc)) {
								locations->push_back(loc);
							}
						}
					}
				});
				return locations;
			}

			const std::vector<Callable<Context>>& getCallables(CBA& cba, std::size_t numParams) {
				auto pos = callables.find(numParams);
				if (pos != callables.end()) {
					return pos->second;
				}

				vector<Callable<Context>>& res = callables[numParams];

				for(const auto& fun : cba.getCallSiteManager().getFreeCallees(numParams)) {

					if (fun.isLambda() || fun.isLiteral()) {

						res.push_back(Callable<Context>(fun));

					} else if (fun.isBind()) {

						for(const auto& ctxt : getContexts(cba)) {
							res.push_back(Callable<Context>(fun, ctxt));
						}

					} else {

						assert_fail() << "Encountered unexpected function type: " << fun.getDefinition()->getNodeType();
					}
				}

				return res;
			}

		};

		typedef utils::TypedMap<Container, ContainerBase> index_map_type;

		core::StatementAddress root;

		Solver solver;

		// a counter to be incremented for generating fresh set ids
		int setCounter;

		// the container for all contexts, call sites and locations for individual context types
		index_map_type indices;

		std::map<ValueID, ConstraintGenerator*> value2generator;				// maps value IDs to their associated generator instances
		std::map<std::type_index, ConstraintGenerator*> generatorIndex;			// to prevent the same type of generator being used multiple times

		// two caches for resolving labels and variables
		int idCounter;
		std::unordered_map<core::StatementAddress, Label> labels;
		std::unordered_map<core::VariableAddress, Variable> vars;

		// a reverse lookup structure for labels
		std::unordered_map<Label, core::StatementAddress> reverseLabels;
		std::unordered_map<Variable, core::VariableAddress> reverseVars;

		// a utility deducing caller <=> callee relations
		CallSiteManager callSiteMgr;

		// a utility helping to reduce the list of allowed call contexts
		CallStringFilter callStringFilter;

	public:

		CBA(const core::StatementAddress& root);

		~CBA() {
			for(auto cur : generatorIndex) delete cur.second;
		}

		// basic functionality

		const core::StatementAddress& getRoot() const {
			return root;
		}

		CallSiteManager& getCallSiteManager() {
			return callSiteMgr;
		}

		// -- main entry point for running analysis --

		template<typename A, typename Context = DefaultContext>
		const typename lattice<A,analysis_config<Context>>::type::value_type&
		getValuesOf(const core::StatementAddress& expr, const A& a, const Context& ctxt = Context()) {
			auto id = getSet(a, getLabel(expr), ctxt);
			return solver.solve(id)[id];
		}

		template<typename A, typename Context, typename ... Rest>
		const typename lattice<A,analysis_config<Context>>::type::value_type&
		getValuesOf(const core::StatementAddress& expr, const A& a, const Context& ctxt, const Rest& ... rest) {
			auto id = getSet(a, getLabel(expr), ctxt, rest...);
			return solver.solve(id)[id];
		}


		// -- set management --

	private:

		struct ValueMapBase {
			virtual ~ValueMapBase() {}
		};

		template<typename T>
		struct ValueMap : public ValueMapBase {
			map<std::type_index,map<T,ValueID>> values;
			map<ValueID,T> data;
		};

		utils::TypedMap<ValueMap, ValueMapBase> valueMap;


		template<typename G>
		ConstraintGenerator* getGenerator() {
			auto& gen = generatorIndex[typeid(G)];
			return (gen) ? gen : (gen = new G(*this));
		}


		template<typename A, typename Config>
		sc::TypedValueID<typename lattice<A,Config>::type> getValueIDInternal(const typename params<A,Config>::type& key) {
			typedef typename params<A,Config>::type params_type;

			// try looking previously assigned value ID
			auto& entry = valueMap.get<params_type>();
			auto& forward = entry.values[typeid(A)];
			auto pos = forward.find(key);
			if (pos != forward.end()) {
				return pos->second;
			}

			// create new value ID
			sc::TypedValueID<typename lattice<A,Config>::type> res(++setCounter);		// reserve 0
			forward[key] = res;
			entry.data.insert(std::make_pair(res,key));

			// fix constraint generator
			value2generator[res] = getGenerator<typename generator<A,Config>::type>();

			// done
			return res;
		}

	public:

		template<typename A, typename Config, typename ... Params>
		sc::TypedValueID<typename lattice<A,Config>::type> getValueID(const Params& ... params) {
			typedef std::tuple<AnalysisType,Params...> params_type;
			return getValueIDInternal<A,Config>(params_type(typeid(A), params...));
		}

		template<typename ... Params>
		const std::tuple<AnalysisType,Params...>& getValueParameters(const ValueID& id) const {
			typedef std::tuple<AnalysisType,Params...> params_type;

			// navigate through two-level index to obtain parameters
			auto& map = valueMap.get<params_type>().data;
			auto pos = map.find(id);
			assert_true(pos != map.end())
					<< " No entry for value ID " << id << " found.\n"
					<< " Current set-counter: " << setCounter << "\n";
			return pos->second;
		}

		template<typename A, typename Context = DefaultContext>
		sc::TypedValueID<typename lattice<A,analysis_config<Context>>::type> getSet(int id, const Context& context = Context()) {
			return getValueID<A,analysis_config<Context>,int,Context>(id, context);
		}

		template<typename A, typename Context = DefaultContext>
		sc::TypedValueID<typename lattice<A,analysis_config<Context>>::type> getSet(const A& type, int id, const Context& context = Context()) {
			return getValueID<A,analysis_config<Context>,int,Context>(id, context);
		}

		template<typename A, typename Address, typename Context = DefaultContext>
		sc::TypedValueID<typename lattice<A,analysis_config<Context>>::type> getSet(const A& type, const Address& stmt, const Context& context = Context()) {
			return getSet(type, getLabel(stmt), context);
		}

		template<typename A, typename Context, typename ... Rest>
		sc::TypedValueID<typename lattice<A,analysis_config<Context>>::type> getSet(const A& type, int id, const Context& ctxt, const Rest& ... rest) {
			return getValueID<A,analysis_config<Context>,int,Context,Rest...>(id, ctxt, rest...);
		}

		template<typename A, typename Address, typename Context, typename ... Rest>
		sc::TypedValueID<typename lattice<A,analysis_config<Context>>::type> getSet(const A& type, const Address& stmt, const Context& ctxt, const Rest& ... rest) {
			return getSet(type, getLabel(stmt), ctxt, rest...);
		}

		template<typename A, typename Context = DefaultContext>
		sc::TypedValueID<typename lattice<A,analysis_config<Context>>::type> getLocationDataSet(const StateSetType& type, Label label, const Context& context, const Location<Context>& loc) {

			// TODO: get rid of the enumeration
			if (type == Sin) {
				typedef location_data_in_analysis<A> in_state;
				return getValueID<in_state,analysis_config<Context>,Label,Context,Location<Context>>(label, context, loc);
			}

			if (type == Stmp) {
				typedef location_data_tmp_analysis<A> tmp_state;
				return getValueID<tmp_state,analysis_config<Context>,Label,Context,Location<Context>>(label, context, loc);
			}

			if (type == Sout) {
				typedef location_data_out_analysis<A> out_state;
				return getValueID<out_state,analysis_config<Context>,Label,Context,Location<Context>>(label, context, loc);
			}

			assert_fail() << "Unsupported state-set type requested: " << type << "\n";
			return 0;
		}

		template<typename A, typename Context = DefaultContext>
		sc::TypedValueID<typename lattice<A,analysis_config<Context>>::type> getLocationDataSet(const StateSetType& type, Label label, const Context& context, const Location<Context>& loc, const A& type_loc) {
			return getLocationDataSet<A, Context>(type, label, context, loc);
		}

		template<typename A, typename Address, typename Context = DefaultContext>
		sc::TypedValueID<typename lattice<A,analysis_config<Context>>::type> getLocationDataSet(const StateSetType& type, const Address& stmt, const Context& context, const Location<Context>& loc, const A& type_loc) {
			return getLocationDataSet(type, getLabel(stmt), context, loc, type_loc);
		}

		// -- label management --

		Label getLabel(const core::StatementAddress& expr) {
			auto pos = labels.find(expr);
			if (pos != labels.end()) {
				return pos->second;
			}
			Label l = ++idCounter;		// reserve 0 for the empty set
			labels[expr] = l;
			reverseLabels[l] = expr;
			return l;
		}

		Label getLabel(const core::StatementAddress& expr) const {
			auto pos = labels.find(expr);
			return (pos != labels.end()) ? pos->second : 0;
		}

		core::StatementAddress getStmt(Label label) const {
			auto pos = reverseLabels.find(label);
			return (pos != reverseLabels.end()) ? pos->second : core::StatementAddress();
		}


		// -- variable management --

		Variable getVariable(const core::VariableAddress& var) {
			auto pos = vars.find(var);
			if (pos != vars.end()) {
				return pos->second;
			}

			// get the definition point
			core::VariableAddress def = getDefinitionPoint(var);

			Variable res;
			if (def == var) {
				res = getLabel(def);		// use label of definition point
				reverseVars[res] = def;
			} else {
				res = getVariable(def);
			}
			vars[var] = res;
			return res;
		}

		Variable getVariable(const core::VariableAddress& var) const {
			auto pos = vars.find(var);
			return (pos != vars.end()) ? pos->second : 0;
		}

		core::VariableAddress getVariable(const Variable& var) const {
			auto pos = reverseVars.find(var);
			return (pos != reverseVars.end()) ? pos->second : core::VariableAddress();
		}


		// -------------- static analysis data input code -----------------

		template<typename Context>
		const std::vector<Location<Context>>& getLocations() {
			return getContainer<Context>().getLocations(*this);
		}

		template<typename Context>
		const std::vector<Callable<Context>>& getCallables(std::size_t numParams) {
			return getContainer<Context>().getCallables(*this, numParams);
		}


		// -------------- Static Context Filter -----------------

		const std::vector<Label>& getDynamicCallLabels() {
			return callStringFilter.getAllCallStringEntries();
		}

		bool isValid(const std::array<Label, 0>& call_ctxt) { return true; }
		bool isValid(const std::array<Label, 1>& call_ctxt) { return callStringFilter.isValidCallStringEntry(call_ctxt.front()); }

		template<std::size_t size>
		bool isValid(const std::array<Label, size>& seq) {
			assert(size >= 2);

			// check sequence
			for(std::size_t i=0; i<size-1; i++) {
				if (!callStringFilter.isValidPredecessor(seq[i], seq[i+1])) return false;
			}
			return true;
		}

		template<unsigned size>
		bool isValid(const Sequence<Label, size>& seq) {
			return isValid(seq.getSequence());
		}

		template<unsigned a, unsigned b>
		bool isValid(const Context<a,b>& ctxt) {
			return isValid(ctxt.callContext);
		}

		template<typename Context>
		const vector<Context>& getValidContexts() {
			return getContainer<Context>().getContexts(*this);
		}

		// --- surrounding contexts ---

		template<typename Context>
		void addSurroundingContexts(const Context& ctxt, set<Context>& res) {

			// special case for empty context
			if (Context::call_context::empty) {
				res.insert(ctxt);
				return;
			}

			// create a list of valid contexts
			for(auto l : callStringFilter.getAllPotentialPredecessors(ctxt.callContext.front())) {
				Context cur = ctxt;
				cur.callContext >>= l;
				res.insert(cur);
			}
		}

		template<typename Context>
		set<Context> getSurroundingContexts(const set<Context>& ctxts) {
			set<Context> res;
			for(const auto& cur : ctxts) {
				addSurroundingContexts(cur, res);
			}
			return res;
		}

		template<typename Context>
		set<Context> getSurroundingContexts(const set<Context>& ctxts, unsigned levels) {
			set<Context> res = ctxts;
			for(unsigned i=0; i<levels; i++) {
				res = getSurroundingContexts(res);
			}
			return res;
		}

		template<typename Context>
		set<Context> getSurroundingContexts(const Context& ctxt, unsigned levels = 1) {
			set<Context> in; in.insert(ctxt);
			return getSurroundingContexts(in, levels);
		}

		// --- nested contexts ---

		template<typename Context>
		void addNestedContexts(const Context& ctxt, set<Context>& res) {

			// special case for empty context
			if (Context::call_context::empty) {
				res.insert(ctxt);
				return;
			}

			// extend call context by valid labels
			auto last = ctxt.callContext.back();
			for (auto l : getDynamicCallLabels()) {
				if (contains(getAllStaticPredecessors(l), last)) {
					Context cur = ctxt;
					cur.callContext <<= l;
					res.insert(cur);
				}
			}

		}

		template<typename Context>
		set<Context> getNestedContexts(const set<Context>& ctxts) {
			set<Context> res;
			for (const auto& cur : ctxts) {
				addNestedContexts(cur, res);
			}
			return res;
		}

		template<typename Context>
		set<Context> getNestedContexts(const set<Context>& ctxts, unsigned levels = 1) {
			set<Context> res;
			for(unsigned i = 0; i<levels; i++) {
				res = getNestedConexts(res);
			}
			return res;
		}

		template<typename Context>
		set<Context> getNestedContexts(const Context& ctxt, unsigned levels = 1) {
			set<Context> in; in.insert(ctxt);
			return getNestedContexts(in, levels);
		}


		// ------------------------ data manager handling -----------------------------

	private:

		utils::HeterogenousContainer dataManagers;

	public:

		template<typename L>
		typename L::manager_type& getDataManager() {
			bool fresh = !dataManagers.contains<typename L::manager_type>();
			typename L::manager_type& res = dataManagers.getInstance<typename L::manager_type>();

			if (fresh) {
				// register the utilized index types within the given manager
				res.template registerIndexType<FieldIndex>();
				res.template registerIndexType<ElementIndex>();
				res.template registerIndexType<ComponentIndex>();
			}

			return res;
		}

		template<typename L>
		typename L::manager_type& getDataManager(const TypedValueID<L>& value) {
			return getDataManager<L>();
		}

		// ----------------------- some debugging utilities ---------------------------

		void plot(std::ostream& out = std::cout) const;

		std::size_t getNumSets() const {
			return value2generator.size();
		}

		std::size_t getNumConstraints() const {
			return solver.getConstraints().size();
		}

	private:

		template<typename T>
		Container<T>& getContainer() { return indices.get<T>(); }

		void addConstraintsFor(const ValueID& value, Constraints& res);


	private:

		// --------------------- Data Storages ---------------------------------------

		// a set of utility functions to attach arbitrary data to a CBA instance that
		// can be shared by constraint generators.

		utils::HeterogenousContainer datastore;

	public:

		template<typename T, typename ... Args>
		T& getDataStore(const Args& ... args) {
			return datastore.getInstance<T>(args...);
		}

		template<typename T, typename ... Args>
		const T& getDataStore(const Args& ... args) const {
			return datastore.getInstance<T>(args...);
		}

	};

	template<typename Context>
	bool isValidContext(CBA& cba, const Context& context) {
		return cba.isValid(context);
	}


} // end namespace cba
} // end namespace analysis
} // end namespace insieme

/**
 * This include has to follow the CBA class definition due to dependencies and needs to
 * be always included whenever the CBA class is included. That is why it is located at
 * the end of the file and must not be moved to the top.
 *
 * TODO: remove this by creating analysis tokens for Sin/Sout/Stmp
 */
#include "insieme/analysis/cba/framework/generator/mutable_data.h"
