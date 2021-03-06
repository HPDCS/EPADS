/////////////////////////////////////////////////////////////////
//	Macro definitions
/////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////
//	Includes 
/////////////////////////////////////////////////////////////////

#include "stats_t.h"
#include  <stdio.h>
#include <math.h>
#include <pthread.h>

/////////////////////////////////////////////////////////////////
//	Global variables
/////////////////////////////////////////////////////////////////


int* running_array;				// Array of integers that defines if a thread should be running
pthread_t* pthread_ids;			// Array of pthread id's to be used with signals
int total_threads;				// Total number of threads that could be used by the transcational operation 
volatile int active_threads;	// Number of currently active threads, reflects the number of 1's in running_array
int nb_cores; 					// Number of cores. Detected at startup and used to set DVFS parameters for all cores
int nb_packages;				// Number of system package. Necessary to monitor energy consumption of all packages in th system
int cache_line_size;			// Size in byte of the cache line. Detected at startup and used to alloc memory cache aligned 
int* pstate;					// Array of p-states initialized at startup with available scaling frequencies 
int max_pstate;					// Maximum index of available pstate for the running machine 
int current_pstate;				// Value of current pstate, index of pstate array which contains frequencies
int total_commits_round; 		// Number of total commits for each heuristics step 
int starting_threads;			// Number of threads running at the start of the heuristic search. Defined in hope_config.txt
int static_pstate;				// Static -state used for the execution with heuristic 8. Defined in hope_config.txt
int steps;						// Number of steps required for the heuristic to converge 
int exploit_steps;				// Number of steps that should be waited until the next exploration is started. Defined in hope_config.txt
int current_exploit_steps;		// Current number of steps since the last completed exploration
double extra_range_percentage;	// Defines the range in percentage over power_limit which is considered valid for the HIGH and LOW configurations. Used by dynamic_heuristic1. Defined in hope_config.txt
int window_size; 				// Defines the lenght of the window, defined in steps, that should achieve a power consumption within power_limit. Used by dynamic_heuristic1. Defined in hope_config.txt 
double hysteresis;				// Defines the amount in percentage of hysteresis that should be applied when deciding the next step in a window based on the current value of window_power. Used by dynamic_heuristic1. Defined in hope_config.txt

stats_t** stats_array;			// Pointer to pointers of struct stats_s, one for each thread 	
volatile int round_completed;   // Defines if round completed and thread 0 should collect stats and call the heuristic function 
double** power_profile; 		// Power consumption matrix of the machine. Precomputed using profiler.c included in root folder.
								// Rows are threads, columns are p-states. It has total_threads+1 rows as first row is filled with 0 for the profile with 0 threads
double power_limit;				// Maximum power that should be used by the application expressed in Watt. Defined in hope_config.txt
double energy_per_tx_limit;		// Maximum energy per tx that should be drawn by the application expressed in micro Joule. Defined in hope_config.txt 
int heuristic_mode;				// Used to switch between different heuristics mode. Can be set from 0 to 14. 
double jump_percentage;			// Used by heuristic mode 2. It defines how near power_limit we expect the optimal configuration to be
volatile int shutdown;			// Used to check if should shutdown
long effective_commits; 		// Number of commits during the phase managed by the heuristics. Necessary due to the delay at the end of execution with less than max threads
int detection_mode; 			// Defines the detection mode. Value 0 means detection is disabled. 1 restarts the exploration from the start. Detection mode 2 resets the execution after a given number of steps. Defined in hope_config.txt and loaded at startup 
double detection_tp_threshold;	// Defines the percentage of throughput variation of the current optimal configuration compared to the results at the moment of convergece that should trigger a new exploration. Defined in hope_config.txt 
double detection_pwr_threshold; // Defines the percentage of power consumption variation of the current optimal configuration compared to the results at the moment of convergece that should trigger a new exploration. Defined in hope_config.txt 

// Barrier detection variables
int barrier_detected; 			// If set to 1 should drop current statistics round, had to wake up all threads in order to overcome a barrier 
int pre_barrier_threads;	    // Number of threads before entering the barrier, should be restored afterwards

// Timeline plot related variables
FILE* timeline_plot_file;		// File used to plot the timeline of execution 
long time_application_startup;	// Application start time 

// Statistics of the last heuristic round
double old_throughput;			
double old_power;			
double old_abort_rate; 		
double old_energy_per_tx;	

// Variables that define the currently best configuration
double best_throughput; 		
int best_threads;				
int best_pstate;	
double best_power;			

// Variables that define the current level best configuration. Used by HEURISTIC_MODE 0, 3, 4
double level_best_throughput; 
int level_best_threads;
int level_best_pstate;
int level_starting_threads;
int level_starting_energy_per_tx;

// Variable to keep track of the starting configuration for phase 1 and 2 of dynamic heuristic 0 and 1 (9 and 10)
int phase0_pstate;
int phase0_threads;

// Variables used to define the state of the search 
int new_pstate;					// Used to check if just arrived to a new p_state in the heuristic search
int decreasing;					// If 0 heuristic should remove threads until it reaches the limit  
int stopped_searching;			// While 1 the algorithm searches for the best configuration, if 0 the algorithm moves to monitoring mode 
int phase;						// The value of phase has different semantics based on the running heuristic mode


// Variables specific to dynamic_heuristic1 
double high_throughput;
int high_pstate;
int high_threads; 
double high_power;

double low_throughput; 
int low_pstate;
int low_threads; 
double low_power;

int current_window_slot;		// Current slot within the window
double window_time;				// Expressed in nano seconds. Defines the current sum of time passed in the current window of configuration fluctuation
double window_power; 			// Expressed in Watt. Current average power consumption of the current fluctuation window

int fluctuation_state;			// Defines the configuration used during the last step, -1 for LOW, 0 for BEST, 1 for HIGH

int boost;					    // Defines if currently boost (such as TurboBoost) is either enabled or disabled 

// Variables specific to LOCK_BASED_TRANSACTIONS. Statistics taken in global variables
pthread_spinlock_t spinlock_variable;	
long lock_start_time;
long lock_end_time;
long lock_start_energy;
long lock_end_energy;
long lock_commits; 

// Variable specific to NET_STATS
long net_time_sum;
long net_energy_sum;
long net_commits_sum;
long net_aborts_sum;

// Variables necessary to compute the error percentage from power_limit, computed once every seconds 
long net_time_slot_start;
long net_energy_slot_start;
long net_time_accumulator;
double net_error_accumulator; 
long net_discard_barrier;

// Variables used by the binary_search heuristic
int min_pstate_search;
int max_pstate_search;

int min_thread_search;
int max_thread_search;
double min_thread_search_throughput;
double max_thread_search_throughput;

/////////////////////////////////////////////////////////////////
//	Function declerations
/////////////////////////////////////////////////////////////////

extern int set_pstate(int);
extern int init_DVFS_management();
extern void sig_func(int);
extern void init_thread_management(int);
extern inline void check_running_array(int);
extern inline int wake_up_thread(int);
extern inline int pause_thread(int);
extern void init_stats_array_pointer(int);
extern stats_t* alloc_stats_buffer(int);
extern void load_profile_file();
extern long get_energy();
extern long get_time();
extern inline void set_threads(int);
extern inline void update_best_config(double, double);
extern inline void stop_searching();
extern int profiler_isoenergy(int, int, int*);
extern void heuristic(double, double, double, double, long);
extern void setup_before_barrier();
extern void set_boost(int);