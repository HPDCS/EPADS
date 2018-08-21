#include "stats_t.h"
#include "macros.h"
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

////////////////////////////////////////////////////////////////////////
// GLOBAL VARIABLES
////////////////////////////////////////////////////////////////////////

int* running_array;				// Array of integers that defines if a thread should be running
pthread_t* pthread_ids;			// Array of pthread id's to be used with signals
int total_threads;				// Total number of threads that could be used by the transcational operation 
volatile int active_threads;	// Number of currently active threads, reflects the number of 1's in running_array
int nb_cores; 					// Number of cores. Detected at startup and used to set DVFS parameters for all cores
int nb_packages;				// Number of system package. Necessary to monitor energy consumption of all packages in the system
int cache_line_size;			// Size in byte of the cache line. Detected at startup and used to alloc memory cache aligned 
int* pstate;					// Array of p-states initialized at startup with available scaling frequencies 
int max_pstate;					// Maximum index of available pstate for the running machine 
int current_pstate;				// Value of current pstate, index of pstate array which contains frequencies
int steps;						// Number of steps required for the heuristic to converge 
stats_t** stats_array;			// Pointer to pointers of struct stats_s, one for each thread 	
volatile int round_completed;   // Defines if round completed and thread 0 should collect stats and call the heuristic function

// powercap_config.txt variables
int static_pstate;				// Static -state used for the execution with heuristic 8
double power_limit;				// Maximum power that should be used by the application expressed in Watt
int total_commits_round; 		// Number of total commits for each heuristics step 
int heuristic_mode;				// Used to switch between different heuristics mode. Can be set from 0 to 14. 
int detection_mode; 			// Defines the detection mode. Value 0 means detection is disabled. 1 restarts the exploration from the start. Detection mode 2 resets the execution after a given number of steps
int exploit_steps;				// Number of steps that should be waited until the next exploration is started
double power_uncore;			// System specific parameter that defines the amount of power consumption used by the uncore part of the system, which we consider to be constant

////////////////////////////////////////////////////////////////////////
// FUNCTIONS
////////////////////////////////////////////////////////////////////////

void powercap_print(void);
void powercap_init(int);

