#include "HyQSat.h"
#include "Solver.h"
//run $CSAT/cust-u20-85/cust-u20-01.cnf

namespace Minisat{
    // add a learned clause
    BRIMManager::BRIMManager() {

    };

     // add a learned clause
    void BRIMManager::add_clause(const ClauseAllocator& ca, const CRef& cls){
        clauselist.push_back(cls);
        const Clause& c = ca[cls];
        conflict_count[cls] = 1;
        const ClauseAllocator& caloc = ca;
    }; 
    /// @brief Ctor for a new BRIMManager instance
    /// @param clauses list of clause references
    /// @param ca data structure mapping from clause references to clause data
    /// @param numclauses number of clauses (somewhat redundant)
    /// @param numvars number of variables
    /// @param lim limit on hardware clauses
    /// @param seed random seed value
    BRIMManager::BRIMManager(Minisat::vec<CRef>& clauses, ClauseAllocator& ca, unsigned numclauses, unsigned numvars, unsigned lim, int seed): clause_limit(lim), rng(seed), calls(0){
        clauselist.reserve(numclauses);
        conflict_count.reserve(numclauses);
        for (size_t i = 0; i < clauses.size(); i++) {
            CRef r = clauses[i];
            // get the clause info, add to clauselist, initialize conflict
            clauselist.push_back(r);
            conflict_count[r] = 1;
        }
    }
    /// @brief Reinit clauselist and conflict_count after garbage collection invalidates prev CRef values
    /// @param coormap Stores pairs of <new CRef, old CRef>
    void BRIMManager::reinitialize_clauses(const std::vector<std::pair<CRef,CRef>> coormap) {
        // no clauses changed, return
        if (!coormap.size())
            return;
        // Copy old conflict count to prevent overwriting prev data
        auto conflict_copy = conflict_count; // TODO Make more efficient
        clauselist.clear(); // clear data structures
        conflict_count.clear();
        // fill with new CRef values
        for (auto pair : coormap) {

            conflict_count[pair.first] = conflict_copy[pair.second];
            clauselist.push_back(pair.first);
        }

    }
    /// @brief Select clauses for hardware solving w/ BFS
    /// @param ca Structure storing clause data
    /// @param solver Solver instance, needed for watchlist data structure (can be used to map from variables to clauses)
    void BRIMManager::select_clauses(const ClauseAllocator& ca, Solver* solver) {
        // sort the clauselist in order of decreasing conflict count
        std::sort(clauselist.begin(), clauselist.end(), [&](const CRef a, const CRef b) {return this->conflict_count.at(a) > this->conflict_count.at(b);});
        calls++; // increment solver calls
        // reinitialize selection vector
        selected.reserve(clause_limit);
        selected.clear();
        // initialize selection reference vector
        std::vector<size_t> selectedrefs;
        selectedrefs.clear();
        selectedrefs.reserve(clause_limit);
        // select a starting clause randomly from the first 30 entries
        std::sample(clauselist.begin(), clauselist.begin() + 30, std::back_inserter(selectedrefs), 1, rng);
        
        // store previously added clauses
        std::unordered_set<CRef> added;
        added.insert(selectedrefs[0]);

        // max number of clauses that can be embedded (either HW limit or problem size)
        size_t max_clauses = std::min(static_cast<size_t>(clause_limit), clauselist.size());
        // empty/allocate space for variable mapping structures
        varmap_inv.clear();
        varmap_inv.reserve(max_clauses * 3);
        varmap.clear();
        varmap.reserve(max_clauses * 3);
        size_t clause_index = 0;
        // perform BFS to fill the queue
        while( selectedrefs.size() < max_clauses ) {
            const Clause& clause = ca[selectedrefs[clause_index]];
            // iterate over each literal in the clause
            for (size_t i = 0; i < clause.size() && selectedrefs.size() < max_clauses; i++) {
                Lit p = clause[i];
                vec<Solver::Watcher>&  ws1  = solver->watches.lookup(p);
                Solver::Watcher        *wi, *end;
                // add each neighboring clause (sharing variable corresponding to p)
                for (wi = (Solver::Watcher*)ws1, end = wi + ws1.size();  wi != end;){
                    
                    CRef neighborref = wi->cref;
                    // if not already added, insert
                    if (added.find(neighborref) == added.end()) {
                        selectedrefs.push_back(neighborref);
                        added.insert(neighborref);
                        const Clause& neighbor = ca[neighborref];
                        if (selectedrefs.size() >= max_clauses) {
                            break;
                        }
                    }
                    wi++;
                }
                // do the same for the negated literal
                vec<Solver::Watcher>&  ws2  = solver->watches.lookup(~p);
                // add each neighboring clause (sharing variable corresponding to p)
                for (wi = (Solver::Watcher*)ws2, end = wi + ws2.size();  wi != end;){
                    
                    CRef neighborref = wi->cref;
                    // if not already added, insert
                    if (added.find(neighborref) == added.end()) {
                        selectedrefs.push_back(neighborref);
                        added.insert(neighborref);
                        const Clause& neighbor = ca[neighborref];
                        if (selectedrefs.size() >= max_clauses) {
                            break;
                        }
                    }
                    wi++;
                }
            }
            clause_index++;
            assert(clause_index < clause_limit);
        }
        // transform from clause objects to vector<int> objects
        for (CRef ref: selectedrefs) {
            std::vector<int> vec;
            const Clause& c = ca[ref];
            vec.reserve(c.size());
            for (size_t i = 0; i < c.size(); i++) {
                int variable = var(c[i]);
                if (varmap.find(variable) == varmap.end()) {
                    varmap[variable] = varmap_inv.size();
                    varmap_inv.push_back(variable);
                }
                vec.push_back(varmap[variable]);
            }
            selected.push_back(vec);
        }
        // set varcount to number of unique variables seen
        varcount = varmap.size();
    };
    /// @brief Call BRIM subsolver
    /// @param numvars number of problem variables
    void BRIMManager::call_brim (unsigned numvars) {
        backend_type = Scenario::UNCERT;
        // Expected API: Clauselist, number of variables, number of clauses, reference to assignment vector 
        // double energy = brim(selected, varcount, selected.size(), assignment);
        // choose_backend(energy, numvars);


    }
    /// @brief Used for scenario 4 (NEARUNSAT) handling
    /// @return literal from a clause which started a NEARUNSAT run on BRIM 
    Lit BRIMManager::get_decision_variable () {
        if (decisionlist.empty()) {
            return lit_Undef;
        }
        int result = decisionlist.front();
        decisionlist.pop_front();
        return result > 0 ?  mkLit(result) : ~mkLit(result);
    }

};