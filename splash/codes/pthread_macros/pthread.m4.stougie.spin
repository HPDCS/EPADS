dnl Found on the web.  Written by Bastiaan Stougie, October 2003
dnl Modified by Alberto Ros and Christos Sakalis, 2015

divert(-1)
define(NEWPROC,) dnl

dnl Empty ROI markers
define(SPLASH3_ROI_BEGIN, `')
define(SPLASH3_ROI_END, `')
dnl
dnl Sniper ROI markers
dnl define(SPLASH3_ROI_BEGIN, `SimRoiStart()')
dnl define(SPLASH3_ROI_END, `SimRoiEnd()')
dnl
dnl Gem5 ROI markers
dnl define(SPLASH3_ROI_BEGIN, `m5_dumpreset_stats(0, 0)')
dnl define(SPLASH3_ROI_END, `m5_dumpreset_stats(0, 0)')

define(BARRIER,
`{
powercap_before_barrier();
pthread_mutex_lock(&(($1).bar_mutex));
($1).bar_teller++;
if (($1).bar_teller == ($2)) {
	($1).bar_teller = 0;
	pthread_cond_broadcast(&(($1).bar_cond));
} else {
	pthread_cond_wait(&(($1).bar_cond), &(($1).bar_mutex));
}
pthread_mutex_unlock(&(($1).bar_mutex));
powercap_after_barrier();}
')
define(BARDEC, `struct { pthread_mutex_t bar_mutex; pthread_cond_t bar_cond; unsigned bar_teller; } $1;')
define(BARINIT,
`{
	pthread_mutex_init(&(($1).bar_mutex), NULL);
	pthread_cond_init(&(($1).bar_cond), NULL);
	($1).bar_teller=0;
}')

define(LOCKDEC, `pthread_spinlock_t $1;')
define(LOCKINIT, `{pthread_spin_init(&($1),PTHREAD_PROCESS_SHARED);}')
define(LOCK, `{powercap_lock_taken(); pthread_spin_lock(&($1));}')
define(UNLOCK, `{pthread_spin_unlock(&($1));}')

define(ALOCKDEC, `pthread_spinlock_t ($1)[$2];')
define(ALOCKINIT, `{ int i; for(i = 0; i < ($2); i++) pthread_spin_init(&(($1)[i]), PTHREAD_PROCESS_SHARED); }')
define(ALOCK, `{powercap_alock_taken(); pthread_spin_lock(&(($1)[($2)]));}')
define(AGETL, `(($1)[$2])')
define(AULOCK, `{pthread_spin_unlock(&(($1)[($2)]));}')

define(PAUSEDEC, `sem_t $1;')
define(PAUSEINIT, `{sem_init(&($1),0,0);}')
define(CLEARPAUSE, `{;}')
define(SETPAUSE, `{sem_post(&($1));}')
define(WAITPAUSE, `{sem_wait(&($1));}')

define(CONDVARDEC, `int $1;')
define(CONDVARINIT, `$1 = 0;')
define(CONDVARWAIT,`{ powercap_before_cond_wait(); pthread_spin_unlock(&($2)); while($1 == 0){}; pthread_spin_lock(&($1)); powercap_after_cond_wait();}')
define(CONDVARSIGNAL,`$1 = 1;')
define(CONDVARBCAST,`$1 = 1;')

define(RELEASE_FENCE, `{ atomic_thread_fence(memory_order_release);}')
define(ACQUIRE_FENCE, `{ atomic_thread_fence(memory_order_acquire);}')
define(FULL_FENCE,    `{ atomic_thread_fence(memory_order_seq_cst);}')

define(CREATE,
`{
	long	i, Error;

	assert(__threads__<__MAX_THREADS__);
	powercap_init($2);
	pthread_mutex_lock(&__intern__);
	for (i = 0; i < ($2) - 1; i++) {
		Error = pthread_create(&__tid__[__threads__++], NULL, (void * (*)(void *))($1), NULL);
		if (Error != 0) {
			printf("Error in pthread_create().\n");
			exit(-1);
		}
	}
	pthread_mutex_unlock(&__intern__);
	$1();
}')
define(WAIT_FOR_END, `{int aantal=$1; while (aantal--) pthread_join(__tid__[aantal], NULL);}')

define(MAIN_INITENV, `{__tid__[__threads__++]=pthread_self();}')
define(MAIN_END, `{exit(0);}')

define(INCLUDES,`
#include <pthread.h>
#include <stdlib.h>
#include <semaphore.h>
#include <assert.h>
#include "/home/stefano/git/EPADS/splash/codes/powercap/powercap.h"
#if __STDC_VERSION__ >= 201112L
#include <stdatomic.h>
#endif
#include <stdint.h>
#define PAGE_SIZE 4096
#define __MAX_THREADS__ 256
')

define(MAIN_ENV,`
INCLUDES
pthread_t __tid__[__MAX_THREADS__];
unsigned __threads__=0;
pthread_mutex_t __intern__;
')

define(EXTERN_ENV, `
INCLUDES
extern pthread_t __tid__[__MAX_THREADS__];
extern unsigned __threads__;
extern pthread_mutex_t __intern__;
')

define(G_MALLOC, `malloc($1);')
define(NU_MALLOC, `malloc($1);')
define(CLOCK, `{long time(); ($1) = time(0);}')
divert(0)
