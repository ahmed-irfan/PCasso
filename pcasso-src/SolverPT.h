/*
 * File:   SolverPT.h
 * Author: ahmed
 *
 * Created on December 24, 2012, 4:04 PM
 */

#ifndef SOLVERPT_H
#define SOLVERPT_H

#include "mtl/Vec.h"
#include "core/SolverTypes.h"
#include "pcasso-src/SplitterSolver.h"

// Davide> Beginning of my includes
#include "utils/LockCollection.h"
#include "pcasso-src/LevelPool.h"
#include "pcasso-src/PartitionTree.h"

#include "utils/Statistics-mt.h"

// for the verifier
#include <sys/types.h>

namespace Pcasso
{
class SolverPT : public SplitterSolver
{

    CoreConfig& coreConfig;
    
    /// file handle used
    fstream* clauses_file = 0;

    /** to check whether clauses are entailed during search*/
    class Verifier
    {
      vector<Lit> dummy;
      vector< vector<Lit> > formula;
    public:
      /** add a clause to the base formula */
      void addClause(const Clause& c)
      {
	dummy.clear();
	for(int i = 0; i < c.size(); ++i ) dummy.push_back(c[i]);
	formula.push_back(dummy);
      }
      
      /** add a unit clause to the base formula */
      void addClause(const Lit& l)
      {
	dummy.clear();
	dummy.push_back(l);
	formula.push_back(dummy);
      }
      
      /** check whether adding the given clause is ok (c is entailed) */
      bool verify(const vec<Lit>& c)
      {
	uint64_t pid = (uint64_t)getpid();
	uint64_t tid = (uint64_t)pthread_self();
	std::stringstream filename;
	filename << "/tmp/tmp_" << pid << "_" << tid << ".cnf";
	printf("open file %s\n", filename.str().c_str());
	std::ofstream f(filename.str().c_str());
	// add the formula to a file
	for(int i = 0 ; i < formula.size(); ++i)
	{
	  f << formula[i] << " 0" << endl; 
	}
	// add the current clause to a file
	for(int i = 0 ; i < c.size(); ++i )
	  f << ~c[i] << " 0" << endl;
	f.close();
	
	// solve the new formula, and check the return code for 20
	int ret = system( (std::string("minisat ") + filename.str() + " &> /dev/null").c_str() );
	printf("command returned with %d\n",ret);
	if(!(WEXITSTATUS(ret) == 20))
	  cerr << "c clause with failed verify step from level: " << c << endl;
        return WEXITSTATUS(ret) == 20;
      }
      
      bool initialized() const { return formula.size() > 0; }
    };
    
    Verifier verifier;
    
  public:
    SolverPT(CoreConfig& config);
    ~SolverPT();

    void dummy()
    {
    }
    vector<unsigned> shared_indeces;
    
    bool set_log_file(const char* filename)
    {
        if( clauses_file ) return false;
	if(!filename) return false;
	clauses_file = new fstream();
	if(!clauses_file) return false;
	clauses_file->open(filename, ios::out);
	if(! *clauses_file) { delete clauses_file; return false; }
	return true;
    }

    std::string position; /** Davide> Position of the solver in the Partition tree */
    unsigned curPTLevel; // Davide> Contains the pt_level of curNode

    int infiniteCnt;  /** Davide> Temporary solution */

    TreeNode* tnode; // Davide>

    unsigned int PTLevel; // Davide> Used by analyze and litRedundant, and search
    unsigned int max_bad_literal; // Davide> as above

    bool rndDecLevel0;//Ahmed> choose random variable at decision level zero

    vector< vector<unsigned> > learnts_indeces; // Davide> attempt
    unsigned int level0UnitsIndex; // ahmed> saving the last index of shared level0 units
    enum ScoreType {NONE, SIZE, LBD_LT, PSM, PSM_FALSE}; // Davide>
    ScoreType receiver_filter_enum;

    // Davide> Statistics
    unsigned diversification_stop_nodes_ID;
    unsigned n_pool_duplicatesID;
    unsigned n_threads_blockedID;
    unsigned n_import_shcl_unsatID;
    unsigned sum_clauses_pools_lv1ID;
    unsigned sum_clauses_pools_lv1_effID; // Davide> What effectively is in the pool
    unsigned sum_clauses_pools_lv0ID;
    unsigned sum_clauses_pools_lv0_effID; // Davide> What effectively is in the pool
    unsigned n_unary_shclausesID;
    unsigned n_binary_shclausesID;
    unsigned n_lbd2_shclausesID;
    unsigned n_clsent_curr_lvID;
    unsigned n_clsent_prev_lvID;
    unsigned n_clcanbeccminID;
    unsigned n_ccmin_worseningID;
    unsigned n_tot_sharedID;
    unsigned n_tot_shared_minus_delID;
    unsigned n_acquired_clausesID;
    unsigned n_tot_forced_restarts_ID;
    unsigned n_tot_reduceDB_calls_ID;
    //unsigned sharing_time_ID;

    Var newVar(bool polarity = true, bool dvar = true, char type = 'o');
    /** Davide> adds a clause, toghether with its pt_level, to the clauses
    of the current formula. False literals of level zero will be
    removed from the clause only if their pt_level is equal to zero,
               i.e. the clause is "safe"
           **/
    bool    addClause(const vec<Lit>& ps, unsigned int pt_level = 0, bool from_shpool = false);
    bool    addClause_(vec<Lit>& ps, unsigned int level = 0);
    void     analyze(CRef confl, vec<Lit>& out_learnt, int& out_btlevel, unsigned int& nblevels);             // (bt = backtrack)
    bool     litRedundant(Lit p, uint32_t abstract_levels);                            // (helper method for 'analyze()')
    void     uncheckedEnqueue(Lit p, CRef from = CRef_Undef, unsigned int pt_level = 0);                          // Enqueue a literal. Assumes value of literal is undefined.
    Lit      pickBranchLit();                                                          // Return the next decision variable.
    CRef    propagate();
    lbool    search(int nof_conflicts);                                                // Search for a given number of conflicts.
    lbool    solve_();                                                                 // Main solve method (assumptions given in 'assumptions').
    void reduceDB();
    /**
        Davide> Returns the level of the current node in the Partition Tree
    **/
    inline unsigned int getNodePTLevel(void) const
    {
        return curPTLevel;
    }

    // Davide> Returns the pt_level associated to the literal l
    unsigned getLiteralPTLevel(const Lit& l) const;//get PT level of literal
    void setLiteralPTLevel(const Lit& l, unsigned pt);

    // Davide> Shares all the learnts that have to be shared
    void push_units();
    void push_learnts(void);
    void pull_learnts(int curr_restarts);
    bool addSharedLearnt(vec<Lit>& ps, unsigned int pt_level);

    /** Transforms a chunk of shared pool into clauses that
     *  will be added to the learnts database **/
    void addChunkToLearnts(vec<Lit>& chunk, unsigned int pt_level, int, int);

    // Davide> Options related to clause sharing
    //
    bool learnt_unary_res;
    int addClause_FalseRemoval;
    int sharedClauseMaxSize;
    int LBD_lt;
    bool learnt_worsening;
    bool pools_filling;
    bool dynamic_PTLevels;
    bool random_sharing;
    int random_sh_prob;
    bool shconditions_relaxing;
    bool every_shpool;
    bool disable_stats;
    bool disable_dupl_check;
    bool disable_dupl_removal;
    int sharing_delay;
    bool flag_based;
    int  receiver_filter;
    int receiver_score;
    int update_act_pol;
    unsigned int lastLevel;

    // Timeout in seconds, initialize to 0 (no timeout)
    double tOut;
    double cpuTime_t() const ; // CPU time used by this thread
    /// return true, if the run time of the solver exceeds the specified limit
    bool timedOut() const;

    /// specify a number of seconds that is allowed to be executed
    void setTimeOut(double timeout);

    /// local version of the statistics object
    Statistics localStat; // Norbert> Local Statistics

    /// return the number of top level units from that are on the trail
    unsigned int getTopLevelUnits() const;

    /// return a specifit literal from the trail
    Lit trailGet(const unsigned int index);


    // LCM modifications
    int performSimplificationNext;

    uint64_t nbLCM, nbLitsLCM, nbConflLits, nbLCMattempts, nbLCMsuccess, npLCMimpDrop, nbRound1Lits, nbRound2Lits, nbLCMfalsified;
    Clock LCMTime;

    bool simplifyLCM(); // learned clause minimization style inprocessing inside the solver, based on vivificatoin

    /** run all the simplification that is necessary for one lause
     * Note: this method assumes the clause is detached before being called
     * @param cr index of the clause to be processed (might be learned or original clause)
     * @param LCMconfig 5x5 number of what to do in forward and backward iteration of LCM (3*5+5 seems to be strongest)
     * @param fullySimplify allow to actually run vivification on that clause, otherwise, only falsified literals are removed
     * @return true, if the clause reference should be kept. In case false is returned, the clause is already removed from the solver
     */
    bool simplifyClause_viviLCM(const CRef cr, int LCMconfig, bool fullySimplify = true);

    int simplifyLearntLCM(Clause& c, int vivificationConfig); // simplify a non-watched clause, and perform vivification on it

    void analyzeFinal(const CRef& conflictingClause, vec< Lit >& out_conflict, const Lit otherLit = lit_Undef);

  private:
    vec<unsigned> varPT; //storing the PT level of each variable
    //CMap<unsigned> clausePT; //storing the PT level of each clause
    //int lbd_lt(vec<Lit>&);
    //different restart strategy settings for sat and unsat
    void satRestartStrategy();
    void unsatRestartStrategy();
};

inline bool     SolverPT::addClause(const vec<Lit>& ps, unsigned int pt_level, bool from_shpool)    { ps.copyTo(add_tmp); return from_shpool ? addSharedLearnt(add_tmp, pt_level) : addClause_(add_tmp, pt_level); }        // Davide> Added pt_level info;
}
#endif  /* SOLVERPT_H */

