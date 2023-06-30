#ifndef HYQSAT
#define HYQSAT
#include <vector>
#include <algorithm>
#include <queue>
#include <list>
#include <random>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <numeric>
#include <unordered_set>
#include <iterator>
#include "minisat/core/SolverTypes.h"
#include "minisat/mtl/Alg.h"
#include "minisat/mtl/Sort.h"
#include "minisat/utils/System.h"
/**
 * Implementation of primary HyQSAT Phases
 * 1. 
 * */

namespace Minisat {
// forward declaration of solver
class Solver;

// std::cout printers for vector, set, map
template <typename T>
std::ostream& operator<<(std::ostream& out, std::vector<T>& target) {
    out << "[";
    size_t len = target.size();
    if (len > 0) {
        for (size_t i = 0; i < len - 1; i++) {
            out << target[i] << ", ";
        }
        out << target[len-1];
    }
    out << "]";
    return out;
}
    template <typename T>
std::ostream& operator<<(std::ostream& out, std::unordered_set<T>& target) {
    out << "{";
    size_t len = target.size();
    if (len > 0) {
        size_t iter = 0;
        for (T i : target) {
            out << i;
            if (iter++ < len)
                out << ", ";
        }
    }
    out << "}";
    return out;
}
    template <typename T, typename S>
std::ostream& operator<<(std::ostream& out, std::unordered_map<T, S>& target) {
    out << "{";
    size_t len = target.size();
    if (len > 0) {
        size_t iter = 0;
        for (std::pair<T,S> i : target) {
            out << i.first << ": " << i.second;
            if (iter++ < len)
                out << ", ";
        }
    }
    out << "}";
    return out;
}
    //Describes the HyQSAT-like scenario, depends on subsolver returned energy/problem mapping     
    enum class Scenario {
        SAT,
        NEARSAT,
        UNCERT,
        NEARUNSAT
    };
  
    // MAIN BRIM-RELAVANT CLASS
    class BRIMManager{
        // Default mapping for scenario-> upper energy bounds
        const std::unordered_map<Scenario, double> default_energy_map = {
           { Scenario::SAT, 0.0 },
           { Scenario::NEARSAT, 5 },
           { Scenario::UNCERT, 10 }};
        private:
            unsigned clause_limit = 0; // max clauses to send to HW
            std::vector<CRef> clauselist; // current TOTAL list of problem clauses, sorted by conflict count
            std::vector<std::vector<int>> selected; // selected clauses to send to BRIM
            std::unordered_map<uint32_t, size_t> conflict_count; // mapping of clause references to conflict counts
            std::mt19937 rng; // rng value for sampling initial clause
            unsigned varcount; // number of variables sent to hardware
            unsigned clausecount; // number of clauses sent to hardware
            unsigned calls; // number of total calls
            std::unordered_map<int, int> varmap; // mapping between problem labels and contiguous BRIM labels
            std::vector<int> varmap_inv; // map from contiguous BRIM variables back to problem

            Scenario backend_type; // type of scenario to handle in backend
            std::vector<int> assignment;  // assignment vector: pass to subsolver
            std::unordered_map<Scenario, double> energy_map; // TODO: train a model to recognize correct scenario
            std::list<int> decisionlist; // list of decision variables for CDCL to pull from, filled from scenario 4 (TODO)
            // Choose the backend scenario based on current energy
            inline 
            void choose_backend(double energy, size_t problem_variables) {
                if (energy <= energy_map[Scenario::SAT] && problem_variables == varcount) {
                    backend_type = Scenario::SAT;
                }
                if (energy <= energy_map[Scenario::NEARSAT]) {
                    backend_type = Scenario::NEARSAT;
                }
                if (energy <= energy_map[Scenario::UNCERT]) {
                    backend_type = Scenario::UNCERT;
                }
                backend_type = Scenario::NEARUNSAT;
            }
            void handle_s1();
            void handle_s2();
            void handle_s3() const {return;};
            void handle_s4();

        public:
            // empty ctor
            BRIMManager();
            
            // construct with iterators to the problem clauses
            BRIMManager(Minisat::vec<CRef>& clauses, ClauseAllocator& ca, unsigned numclauses, unsigned numvars, unsigned lim, int seed = 0);

            void add_clause(const ClauseAllocator& ca, const CRef& cls); // add a learned clause

            void inc_conflict(const CRef cr) {conflict_count.at(cr) = conflict_count.at(cr) + 1;}; // bump the conflict score for cr
            void call_brim(unsigned numvars); // call the BRIM solver and set the backend scenario
            void reinitialize_clauses (const std::vector<std::pair<CRef,CRef>> coormap); // reinitialize clause structures, needed after garbage collection (TODO: make more efficient)
            Lit get_decision_variable () ; // return decision variable from decisionlist
            
            // ACCESSORS
            unsigned get_limit () const {return clause_limit;}; // return the clause limit
            Scenario get_backend_type () const  {return backend_type;} // return the backend scenario
            const std::vector<int> get_invmap() const {return varmap_inv;};
            const std::vector<int> get_assignment() const {return assignment;};
            
            // Select clauses for embedding using BFS
            void select_clauses(const ClauseAllocator& ca, Solver* solver);
            
            
            ~BRIMManager() {};
            BRIMManager& operator=(const BRIMManager& other) {
                clause_limit = other.clause_limit;
                clauselist = other.clauselist;
                conflict_count = other.conflict_count;
                rng = other.rng;
                calls = other.calls;
            }
    };
};
#endif