#include  <stdio.h>
#include  <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include  <sys/types.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>

const int NUM_THREADS = 15;
const int tnum = 5;
pthread_mutex_t tcurr_mutex;
pthread_cond_t tcurr_cv;
int tcurr = 0;

void* PrintHello(void* arg);

void* PrintHello(void* arg){
	sleep(8);
	pthread_mutex_lock(&tcurr_mutex);
	printf("thread %i in mutex, tcurr = %i\n", pthread_self(), tcurr);
	tcurr--;
	if(tcurr == (tnum - 1)) pthread_cond_signal(&tcurr_cv);
	pthread_mutex_unlock(&tcurr_mutex);
	pthread_exit((void*)0);
} 

int main(){
	pthread_t threads[NUM_THREADS];
	pthread_mutexattr_t tcurr_mutex_attr;
	pthread_condattr_t tcurr_cv_attr;
	pthread_mutexattr_init(&tcurr_mutex_attr);
	pthread_condattr_init(&tcurr_cv_attr);
	pthread_mutexattr_setpshared(&tcurr_mutex_attr, PTHREAD_PROCESS_SHARED);
	pthread_condattr_setpshared(&tcurr_cv_attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&tcurr_mutex, &tcurr_mutex_attr);
	pthread_cond_init(&tcurr_cv, &tcurr_cv_attr);
	int rc;
	long i;
	for(i = 0; i < NUM_THREADS; i++){
		rc = pthread_create(&threads[i], NULL, PrintHello, (void*)i);
		if(rc){
			printf("ERROR; return code from pthread_create() is %d\n", rc);
			return 1;
		}
		else{
			pthread_mutex_lock(&tcurr_mutex);
			tcurr++;
			printf("parent in mutex, tcurr = %i\n", tcurr);
			if(tcurr == tnum) pthread_cond_wait(&tcurr_cv, &tcurr_mutex);
			pthread_mutex_unlock(&tcurr_mutex);
		}
		sleep(1);
	}

	pthread_mutex_destroy(&tcurr_mutex);
	pthread_mutexattr_destroy(&tcurr_mutex_attr);
	pthread_exit(NULL);
	return 0;
}
