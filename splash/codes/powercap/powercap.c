#include "powercap.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>

// Function just used for debug
void powercap_print(){
	
	#ifdef DEBUG_HEURISTICS
		printf("Lock taken from function in powercap.c\n");
	#endif
}

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
void sig_func(int sig){}

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


// Executed inside stm_init
void check_running_array(int threadId){
	
	while(running_array[threadId] == 0){
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
	if (fscanf(config_file, "STATIC_PSTATE=%d POWER_LIMIT=%lf COMMITS_ROUND=%d HEURISTIC_MODE=%d DETECTION_MODE=%d EXPLOIT_STEPS=%d POWER_UNCORE=%lf", 
			 &static_pstate, &power_limit, &total_commits_round, &heuristic_mode, &detection_mode, &exploit_steps, &power_uncore)!=7) {
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


void powercap_init(int threads){
	
	init_DVFS_management();
	init_thread_management(threads);

	printf("Energy: %ld\n", get_energy());
	
	/*init_stats_array_pointer(threads);
	load_config_file();

	if(heuristic_mode == 15)
		init_model_matrices();

	init_global_variables();
	set_boost(1);

	#ifdef DEBUG_HEURISTICS
		printf("Heuristic mode: %d\n", heuristic_mode);
	#endif

	if(starting_threads > total_threads){
		printf("Starting threads set higher than total threads. Please modify this value in hope_config.txt\n");
		exit(1);
	}
	
	// Set active_threads to starting_threads
	for(i = starting_threads; i<total_threads;i++){
		pause_thread(i);
	}

	*/
}