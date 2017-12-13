# EPADS: Exploration-based Power-capping for Applications with Diverse Scalability

### Introduction

The software in this repository defines a power capping solution that maximizes application performance while operating within power consumption constraints. This solution is based on the results of a preliminary analysis that shows how the throughput curve, when varying the number of cores assigned to a multithreaded applications, preserves the same shape, and the same value for the maximum, at different performance states (P-state). Based on this result, the software presented in this repository performs an online exploration of the configurations of P-state and assigned cores/threads with the goal of adaptively allocating the power budget (power cap) to maximize application performance. The code in this repository was partially exploited to produce all the experimental results presented in the scientific paper "Adaptive Performance Optimization under Power Constraint in Multi-thread Applications with Diverse Scalability", presented at the 9th ACM/SPEC International Conference on Performance Engineering (ICPE 2018). Further indications on how to replicate and validate the results presented in the paper are given below. 

### Requirements

This software is developed for Linux operating systems in c programming language. The minimum kernel version required is system specific since it relies on the "powercap" Linux subsystem which is supported by different processors since different kernel releases. The "powercap" framework is only supported by Intel processors following the Sandy Bridge architecture. This facility is only used to obtaining energy consumption measurements with low overhead and it is never exploited to set hardware-level power constraints.  
It might be necessary to disable the cpufreq governor "intel_pstate" (setting "intel_pstate=disable" in the kernel boot line) since it doesn't support user space P-state management. 

### Compilation Macros

Compilation macros can be set in the file "powercap/macros.h".

**DEBUG_HEURISTICS**: if defined sets the software in debug mode during which runtime information is printed to standard output;
**DEBUG_OVERHEAD**: if defined prints the time interval spent in different portions of code that perform operations of measurement, startup or configuration of data necessary for the proper execution of the presented power capping strategy, but that are not necessary for the unmanaged execution of applications;
**LOCK_BASED_TRANSACTIONS**: if defined changes the synchronization paradigm used by the STAMP applications from transactions to coarse-grained locking implemented with spinlocks; 
**TIMELINE_PLOT**: if defined produces in the working directory a file that shows for each step (either explorative or exploitative) the selected configuration of threads and P-state, the average power consumption, the value of the powercap at the given time and the average throughput. 


### Compile

You can use "compile.sh" to compile the power capping solution, TinySTM and the STAMP benchmark suite. 

### Configuration file

The configuration file named "hope_config.txt" is loaded at the start of the execution and defines different parameters read at runtime. 
The semantics of these parameters is presented below.

**STARTING_THREADS**: number of threads used to start the first exploration procedure;  
**STATIC_PSTATE**: deprecated;
**POWER_LIMIT**: power cap value in Watt; 
**COMMITS_ROUND**: number of commits (transactional) or critical sections (lock-based) executed for each exploration step;
**ENERGY_PER_TX_LIMIT**: deprecated;
**HEURISTIC_MODE**: select between different exploration strategies and techniques, defined as an integer ranging from 0 to 14. Further information can be found below or in the file "powercap/heuristics.c";
**JUMP_PERCENTAGE**: deprecated;
**DETECTION_MODE**: defines when the exploration procedure is restarted. Value 0 means the exploration procedure is only executed once, value 1 restarts the exploration when a workload variation is detected, mode 2 restarts the exploration procedure after EXPLOIT_STEPS steps starting from the end of the previous exploration procedure;
**DETECTION_TP_THRESHOLD**: minimum variation of throughput compared to the selected configuration, expressed as a percentage, that triggers the restart of the exploration when DETECTION_MODE is set to 1;
**DETECTION_PWR_THRESHOLD**: minimum variation of power consumption compared to the selected configuration, expressed as a percentage, that triggers the restart of the exploration when DETECTION_MODE=1 is set to 1;
**EXPLOIT_STEPS**: number of steps during which the configuration selected by the latest exploration procedure is exploited before restarting the exploration procedure; 
**EXTRA_RANGE_PERCENTAGE**: used only when HEURISTIC_MODE is set to 10, it defines the range in percentage over POWER_LIMIT which is considered valid for selecting the HIGH and LOW configurations;
**WINDOW_SIZE**: used only when HEURISTIC_MODE is set to 10, it defines the lenght in steps of the window during which the average power consumption should be as close as possible to POWER_LIMIT;
**HYSTERESIS**: used only when HEURISTIC_MODE is set to 10, it defines the amount of hysteresis (in percentage) that should be considered when deciding the next step in a window;

### Run

To run an application please refer to the "README" file included in the application path. The configuration file (hope_config.txt) must be present in the working directory of the application. To avoid duplicating the same configuration for different applications, we suggest to execute applications from the root folder of these repository exploiting the provided "hope_config.txt" modified as necessary. 

### Code organization

The software that constitutes the power capping solution is in the "powercap/" folder. In "powercap.h" and "powercap.c" resides the code that accounts for data structures and global variables initialization, DVFS and thread management, statistics gathering, time and energy sampling. The code in "heuristics.c" implements different exploration strategies based on either a power consumption limit or energy-per-transaction limit. 
The code in "stats_t.h" presents the struct used for statistics gathering. 

The folder "tinySTM" contains the TinySTM transactional memory framework. The different functions and global variables offered by the files in the "powercap/" folder are called and referenced by the file "TinySTM/src/stm.c" at the proper point of the transaction lifecycle. 
The code in "stamp/" contains the applications of the STAMP benchmark suite, modified to support lock-based synchronization which is enabled if the macro LOCK_BASED_TRANSACTIONS is defined at compile time. 
The code in "bench/" contains the .sh scripts used for testing.
The code in "utils/ contains multiple utility programs used to parse test results. 
The code in "rapl-power/" was used to obtain energy measurements in systems not supporting the "powercap" linux framework and is currently deprecated   

### Adaptive Performance Optimization under Power Constraint in Multi-thread Applications with Diverse Scalability

In the paper presented in the 9th ACM/SPEC International Conference on Performance Engineering (ICPE 2018), 4 different exploration strategies are evaluated and compared. The implementation of the decision trees used by those exploration strategies can be found in "powercap/heuristics.c". The relations between the HEURISTIC_MODE values, the functions in "powercap/heuristics.c" and the strategies referred in the paper are the following: 

**HEURISTIC_MODE 9**: the function "dynamic_heuristic0" implements the exploration strategy referred as "basic strategy";
**HEURISTIC_MODE 10**: the function "dynamic_heuristic1" implements the exploration strategy referred as "enhanced strategy";
**HEURISTIC_MODE 11**: the function "heuristic_highest_threads implements the exploration strategy referred as "baseline technique";
**HEURISTIC_MODE 13**: the function "heuristic_two_step_search" implements the exploration strategy referred as "dual-phase technique";

For all executions and different values of HEURISTIC_MODE, DETECTION_MODE was set to 2. The preliminary analysis was performed with default implementations of both TinySTM and STAMP. The "bench/ICPE2018" folder contains .sh scripts used for running the experimental analysis presented in the paper. These scripts also set the configuration file to the same settings used during the experimental evaluation. The lock-based implementation of the STAMP benchmark suite is contained in the same source files of its transactional-based implementation. All code related to the lock-based implementation is delimited by the macro LOCK_BASED_TRANSACTION, thus can be located with "grep -r LOCK_BASED_TRANSACTION" executed from the root folder of the repository. 

The results presented in the paper are obtained on a dual-socket system with 20 physical cores in total. We expect better results for basic and enhanced strategy, compared to the baseline strategy, when executed on systems with an higher number of physical cores, and a lower results for systems with a lower number of cores. Moreover, the power efficiency at different P-state or the range of supported CPU frequencies for the specific system can also affects the effectiveness of the proposed solution.
