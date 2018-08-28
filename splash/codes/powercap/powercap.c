#include "powercap.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>

int set_pstate(int input_pstate){
	
	int i;
	char fname[64];
	FILE* frequency_file;
	
	if(input_pstate > max_pstate)
		return -1;
		
	if(current_pstate != input_pstate){
		int frequency = pstate[input_pstate];

		for(i=0; i<nb_cores; i++){
			sprintf(fname, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_setspeed", i);
			frequency_file = fopen(fname,"w+");
			if(frequency_file == NULL){
				printf("Error opening cpu%d scaling_setspeed file. Must be superuser\n", i);
				exit(0);		
			}		
			fprintf(frequency_file, "%d", frequency);
			fflush(frequency_file);
			fclose(frequency_file);
		}
		current_pstate = input_pstate;
	}
	return 0;
}

// Sets the governor to userspace and sets the highest frequency
int init_DVFS_management(){
	
	char fname[64];
	char* freq_available;
	int frequency, i;
	FILE* governor_file;

	//Set governor to userspace
	nb_cores = sysconf(_SC_NPROCESSORS_ONLN);
	for(i=0; i<nb_cores;i++){
		sprintf(fname, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", i);
		governor_file = fopen(fname,"w+");
		if(governor_file == NULL){
			printf("Error opening cpu%d scaling_governor file. Must be superuser\n", i);
			exit(0);		
		}		
		fprintf(governor_file, "userspace");
		fflush(governor_file);
		fclose(governor_file);
	}

	// Init array of available frequencies
	FILE* available_freq_file = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies","r");
	if(available_freq_file == NULL){
		printf("Cannot open scaling_available_frequencies file\n");
		exit(0);
	}
	freq_available = malloc(sizeof(char)*256);
	fgets(freq_available, 256, available_freq_file);
	
	pstate = malloc(sizeof(int)*32);
	i = 0; 
	char * end;

	for (frequency = strtol(freq_available, &end, 10); freq_available != end; frequency = strtol(freq_available, &end, 10)){
		pstate[i]=frequency;
		freq_available = end;
  		i++;
	}
  	max_pstate = --i;

	#ifdef DEBUG_HEURISTICS
  		printf("Found %d p-states in the range from %d MHz to %d MHz\n", max_pstate, pstate[max_pstate]/1000, pstate[0]/1000);
  	#endif
  	fclose(available_freq_file);

  	current_pstate = -1;
  	set_pstate(max_pstate);

	return 0;
}

// SIGUSR1 handler. Doesn't need to execute any code
void sig_func(int sig){
	//DEBUG
	printf("Thread %d received SIGUSR1\n", thread_number);
	//DEBUG
}

// Executed inside stm_init
void init_thread_management(int threads){

	char* filename;
	FILE* numafile;
	int package_last_core;
	int i;

	// Init total threads and active threads
	total_threads = threads;

	#ifdef DEBUG_HEURISTICS
		printf("Set total_threads to %d\n", threads);
	#endif

	active_threads = total_threads;

	// Init running array with all threads running 	
	running_array = malloc(sizeof(int)*total_threads);
	for(i=0; i<total_threads; i++)
		running_array[i] = 1;	

	// Allocate memory for pthread_ids
	pthread_ids = malloc(sizeof(pthread_t)*total_threads);

	//Registering SIGUSR1 handler
	signal(SIGUSR1, sig_func);

	//init number of packages
	filename = malloc(sizeof(char)*64); 
	sprintf(filename,"/sys/devices/system/cpu/cpu%d/topology/physical_package_id", nb_cores-1);
	numafile = fopen(filename,"r");
	if (numafile == NULL){
		printf("Cannot read number of packages\n");
		exit(1);
	} 
	fscanf(numafile ,"%d", &package_last_core);
	nb_packages = package_last_core+1;

	#ifdef DEBUG_HEURISTICS
		printf("Number of packages detected: %d\n", nb_packages);
	#endif
}


void check_running_array(int threadId){
	
	while(running_array[threadId] == 0){
		#ifdef DEBUG_HEURISTICS
			printf("Pausing thread %d\n", thread_number);
		#endif
		pause();
	}
}

// Used by the heuristics to tune the number of active threads 
int wake_up_thread(int thread_id){
	
	if(running_array[thread_id] == 1){
		printf("Waking up a thread already running\n");
		return -1;
	}

	running_array[thread_id] = 1;
	pthread_kill(pthread_ids[thread_id], SIGUSR1);
	active_threads++;
	return 0;
}

// Used by the heuristics to tune the number of active threads 
int pause_thread(int thread_id){

	if(running_array[thread_id] == 0 ){
		
		#ifdef DEBUG_HEURISTICS
			printf("Pausing a thread already paused\n");
		#endif

		return -1;
	}

	running_array[thread_id] = 0;
	active_threads--;
	return active_threads;
}

// Function used to set the number of running threads. Based on active_threads and threads might wake up or pause some threads 
void set_threads(int to_threads){

	int i;
	int starting_threads = active_threads;

	if(starting_threads != to_threads){
		if(starting_threads > to_threads){
			for(i = to_threads; i<starting_threads; i++)
				pause_thread(i);
		}
		else{
			for(i = starting_threads; i<to_threads; i++)
				wake_up_thread(i);
		}
	}
}

// Executed inside stm_init
void init_stats_array_pointer(int threads){

	// Allocate memory for the pointers of stats_t
	stats_array = malloc(sizeof(stats_t*)*threads); 

	cache_line_size = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);

	#ifdef DEBUG_HEURISTICS
		printf("D1 cache line size: %d bytes\n", cache_line_size);
	#endif
}

// Executed by each thread inside stm_pre_init_thread
stats_t* alloc_stats_buffer(int thread_number){
	
	stats_t* stats_ptr = stats_array[thread_number];

	int ret = posix_memalign(((void**) &stats_ptr), cache_line_size, sizeof(stats_t));
	if ( ret != 0 ){ printf("Error allocating stats_t for thread %d\n", thread_number);
		exit(0);
	}

	stats_ptr->total_commits = total_commits_round/active_threads;
	stats_ptr->commits = 0;
	stats_ptr->aborts = 0;
	stats_ptr->nb_tx = 0;
	stats_ptr->start_energy = 0;
	stats_ptr->end_energy = 0;
	stats_ptr->start_time = 0;
	stats_ptr->end_time = 0;

	stats_array[thread_number] = stats_ptr;

	return stats_ptr;
}


void load_config_file(){
	
	// Load config file 
	FILE* config_file;
	if ((config_file = fopen("powercap_config.txt", "r")) == NULL) {
		printf("Error opening powercap_config configuration file.\n");
		exit(1);
	}
	if (fscanf(config_file, "STARTING_THREADS=%d STATIC_PSTATE=%d POWER_LIMIT=%lf COMMITS_ROUND=%d HEURISTIC_MODE=%d DETECTION_MODE=%d EXPLOIT_STEPS=%d POWER_UNCORE=%lf", 
			 &starting_threads, &static_pstate, &power_limit, &total_commits_round, &heuristic_mode, &detection_mode, &exploit_steps, &power_uncore)!=8) {
		printf("The number of input parameters of the configuration file does not match the number of required parameters.\n");
		exit(1);
	}

  	// Necessary for the static execution in order to avoid running for the first step with a different frequency than manually set in hope_config.txt
  	if(heuristic_mode == 8){
  		if(static_pstate >= 0 && static_pstate <= max_pstate)
  			set_pstate(static_pstate);
  		else 
  			printf("The parameter manual_pstate is set outside of the valid range for this CPU. Setting the CPU to the slowest frequency/voltage\n");
  	}else if(heuristic_mode == 12 || heuristic_mode == 13 || heuristic_mode == 15){
  		set_pstate(max_pstate);
  		set_threads(1);
  	}

	fclose(config_file);
}


// Returns energy consumption of package 0 cores in micro Joule
long get_energy(){
	
	long energy;
	int i;
	FILE* energy_file;
	long total_energy = 0;
	char fname[64];

	for(i = 0; i<nb_packages; i++){

		// Package energy consumtion
		sprintf(fname, "/sys/class/powercap/intel-rapl/intel-rapl:%d/energy_uj", i);
		energy_file = fopen(fname, "r");
		
		// Cores energy consumption
		//FILE* energy_file = fopen("/sys/class/powercap/intel-rapl/intel-rapl:0/intel-rapl:0:0/energy_uj", "r");	

		// DRAM module, considered inside the package
		//FILE* energy_file = fopen("/sys/class/powercap/intel-rapl/intel-rapl:0/intel-rapl:0:1/energy_uj", "r");	

		if(energy_file == NULL){
			printf("Error opening energy file\n");		
		}
		fscanf(energy_file,"%ld",&energy);
		fclose(energy_file);
		total_energy+=energy;
	}

	return total_energy;
}


// Return time as a monotomically increasing long expressed as nanoseconds 
long get_time(){
	
	long time = 0;
	struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    time += (ts.tv_sec*1000000000);
    time += ts.tv_nsec;

	return time;
}

void init_model_matrices(){

	int i;

	// Allocate the matrices
	power_model = (double**) malloc(sizeof(double*) * (max_pstate+1));
	throughput_model = (double**) malloc(sizeof(double*) * (max_pstate+1)); 

	// Allocate the validation matrices
	power_validation = (double**) malloc(sizeof(double*) * (max_pstate+1));
	throughput_validation = (double**) malloc(sizeof(double*) * (max_pstate+1)); 

	// Allocate matrices to store real values during validation
	power_real = (double**) malloc(sizeof(double*) * (max_pstate+1));
	throughput_real = (double**) malloc(sizeof(double*) * (max_pstate+1)); 

	for (i = 0; i <= max_pstate; i++){
   		power_model[i] = (double *) malloc(sizeof(double) * (total_threads));
   		throughput_model[i] = (double *) malloc(sizeof(double) * (total_threads));

   		power_validation[i] = (double *) malloc(sizeof(double) * (total_threads));
   		throughput_validation[i] = (double *) malloc(sizeof(double) * (total_threads));

   		power_real[i] = (double *) malloc(sizeof(double) * (total_threads));
   		throughput_real[i] = (double *) malloc(sizeof(double) * (total_threads));
   	}

   	// Init first row with all zeros 
   	for(i = 0; i <= max_pstate; i++){
   		power_model[i][0] = 0;
   		throughput_model[i][0] = 0;

   		power_validation[i][0] = 0;
   		throughput_validation[i][0] = 0;

   		power_real[i][0] = 0;
   		throughput_real[i][0] = 0;
   	}
}

// Initialization of global variables 
void init_global_variables(){

	#ifdef DEBUG_HEURISTICS
		printf("Initializing global variables\n");
	#endif
}


// Used to either enable or disable boosting facilities such as TurboBoost. Boost is disabled whenever the current config goes out of the powercap 
void set_boost(int value){

	int i;
	char fname[64];
	FILE* boost_file;

	if(value != 0 && value != 1){
		printf("Set_boost parameter invalid. Shutting down application\n");
		exit(1);
	}
	
	boost_file = fopen("/sys/devices/system/cpu/cpufreq/boost", "w+");
	fprintf(boost_file, "%d", value);
	fflush(boost_file);
	fclose(boost_file);

	return;
}


void powercap_init_thread(){

	thread_number_init = 1;
	int id = __atomic_fetch_add(&thread_counter, 1, __ATOMIC_SEQ_CST);
	stats_ptr = alloc_stats_buffer(id);
	thread_number = id;
	pthread_ids[id]=pthread_self();

	#ifdef DEBUG_HEURISTICS
		printf("Allocated thread %d with tid %lu\n", id, pthread_ids[id]);
	#endif

	// Wait for all threads to get initialized
	while(thread_counter != total_threads){}

	#ifdef DEBUG_HEURISTICS
		if(id == 0){
			printf("Initialized all thread ids\n");
		}
	#endif

	// Thread 0 sets itself as a collector and inits global variables or init global variables if lock based
	if( id == 0){
		stats_ptr->collector = 1;
		net_time_slot_start = get_time();
		net_energy_slot_start = get_energy();
	}

}

/////////////////////////////////////////////////////////////
// EXTERNAL API
/////////////////////////////////////////////////////////////

void powercap_init(int threads){
	
	#ifdef DEBUG_HEURISTICS	
		printf("CREATE called\n");
	#endif

	init_DVFS_management();
	init_thread_management(threads);
	init_stats_array_pointer(threads);
	load_config_file();
	init_global_variables();
	set_boost(1);
	if(heuristic_mode == 15)
		init_model_matrices();

	#ifdef DEBUG_HEURISTICS
		printf("Heuristic mode: %d\n", heuristic_mode);
	#endif

	if(starting_threads > total_threads){
		printf("Starting threads set higher than total threads. Please modify this value in hope_config.txt\n");
		exit(1);
	}
	
	// Set active_threads to starting_threads
	for(int i = starting_threads; i<total_threads;i++){
		pause_thread(i);
	}
} 


// Function called before taking a lock
void powercap_lock_taken(){
	
	// At first run should initialize thread and get thread number
	if(thread_number_init == 0){
		powercap_init_thread();
	}

	if(thread_number == 0)
		printf("Lock counter: %ld\n", lock_counter++);

	#ifdef DEBUG_HEURISTICS
		if(thread_number_init == 1 && thread_number == 0)
				printf("Lock\n");
	#endif

	check_running_array(thread_number);	
}

void powercap_alock_taken(){
	// At first run should initialize thread and get thread number
	if(thread_number_init == 0){
		powercap_init_thread();
	}

	if(thread_number == 0)
		printf("Lock counter: %ld\n", lock_counter++);

	#ifdef DEBUG_HEURISTICS
		if(thread_number_init == 1 && thread_number == 0)
				printf("ALock\n");
	#endif

	check_running_array(thread_number);	
}

// Called before a barrier, must wake-up all threads to avoid a deadlock
void powercap_before_barrier(){

	#ifdef DEBUG_HEURISTICS
		if(thread_number_init == 1 && thread_number == 0){
			printf("Barrier\n");
			printf("Active thread %d\n", active_threads);
		}
	#endif

	if(thread_number_init == 1 && thread_counter == total_threads && thread_number == 0 && active_threads!=total_threads) {
	
		#ifdef DEBUG_HEURISTICS
			printf("Thread 0 detected a barrier\n");
		#endif
			
		// Next decision phase should be dropped
		barrier_detected = 1;

		// Dont consider next slot for power_limit error measurements
		net_discard_barrier = 1;

		// Save number of threads that should be restored after the barrier
		pre_barrier_threads = active_threads;

		// Wake up all threads
		for(int i=active_threads; i< total_threads; i++){
  			wake_up_thread(i);
  		}
	}
}

void powercap_after_barrier(){

	//DEBUG
	printf("Powercap_after_barrier - thread_number_init %d - thread_number %d - active_thread %d - pre_barrier_threads %d \n", thread_number_init, thread_number, active_threads, pre_barrier_threads);
	//

	if(thread_number_init == 1 && thread_number == 0 && pre_barrier_threads != 0 && active_threads!=pre_barrier_threads){
		set_threads(pre_barrier_threads);
		
		#ifdef DEBUG_HEURISTICS
			printf("Setting threads back to %d\n", pre_barrier_threads);
		#endif
	}
}