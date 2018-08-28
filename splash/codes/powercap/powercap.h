#ifndef __POWERCAP_HEADER
#define __POWERCAP_HEADER

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
volatile int thread_counter;	// Global variable used for assigning an increasing counter to threads
volatile int running_token;		// Token used to manage sleep after barrier. Necessary to manage situations where signals arrive before thread is actually back to pausing
volatile int cond_waiters;		// Used to manage phread conditional wait. It's incremented and decremented atomically

// powercap_config.txt variables
int starting_threads;			// Number of threads running at the start of the exploration
int static_pstate;				// Static -state used for the execution with heuristic 8
double power_limit;				// Maximum power that should be used by the application expressed in Watt
int total_commits_round; 		// Number of total commits for each heuristics step 
int heuristic_mode;				// Used to switch between different heuristics mode. Check available values in heuristics.  
int detection_mode; 			// Defines the detection mode. Value 0 means detection is disabled. 1 restarts the exploration from the start. Detection mode 2 resets the execution after a given number of steps
int exploit_steps;				// Number of steps that should be waited until the next exploration is started
double power_uncore;			// System specific parameter that defines the amount of power consumption used by the uncore part of the system, which we consider to be constant

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

// Model-based variables
// Matrices of predicted power consumption and throughput for different configurations. 
// Rows are p-states, columns are threads. It has total_threads+1 column as first column is filled with 0s 
// since it is not meaningful to run with 0 threads.
double** power_model; 
double** throughput_model;
double** power_validation; 
double** throughput_validation;
double** power_real; 
double** throughput_real;
int validation_pstate;	// Variable necessary to validate the effectiveness of the models

// Barrier detection variables
int barrier_detected; 			// If set to 1 should drop current statistics round, had to wake up all threads in order to overcome a barrier 
int pre_barrier_threads;	    // Number of threads before entering the barrier, should be restored afterwards

// Debug variables
long lock_counter; 

////////////////////////////////////////////////////////////////////////
// THREAD LOCAL VARIABLES
////////////////////////////////////////////////////////////////////////

static volatile __thread int thread_number;			// Number from 0 to Max_thread to identify threads inside the application
static volatile __thread int thread_number_init; 	// Used at each lock request to check if thread id of the current thread is already registered
static volatile __thread stats_t* stats_ptr;		// Pointer to stats struct for the current thread. This allows faster access

////////////////////////////////////////////////////////////////////////
// FUNCTIONS
////////////////////////////////////////////////////////////////////////

void powercap_lock_taken(void);
void powercap_alock_taken(void);
void powercap_init(int);
void powercap_before_barrier(void);
void powercap_after_barrier(void);
void powercap_before_cond_wait(void);
void powercap_after_cond_wait(void);


#endif