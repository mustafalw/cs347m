#include  <stdio.h>
#include  <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include  <sys/types.h>
#include <fcntl.h>	/* For O_* constants */
#include <sys/stat.h>	/* For mode constants */
#include <semaphore.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>

#define SHMSZ 200	// shared memory segment for queries
#define SHMSZINT 4	// shared memory segment for int pointer
struct timeval mytime;
pthread_mutex_t tcurr_mutex;
pthread_cond_t tcurr_cv;
int tcurr = 0;

struct arg_handler{
	sem_t* arg_sem;
	int* max_threads;
	char query[11];
};

int* shared_memory_int(int shmid, key_t key, int size);
char* shared_memory_char(int shmid, key_t key, int size);

int* shared_memory_int(int shmid, key_t key, int size){
	if((shmid = shmget(key, size, IPC_CREAT | 0666)) < 0){
		perror("shmget");
	}

	int* res;
	if((res = shmat(shmid, NULL, 0)) == (int *) -1){
		perror("shmat");
	}

	return res;
}

char* shared_memory_char(int shmid, key_t key, int size){
	if((shmid = shmget(key, size, IPC_CREAT | 0666)) < 0){
		perror("shmget");
	}

	char* res;
	if((res = shmat(shmid, NULL, 0)) == (char *) -1){
		perror("shmat");
	}

	return res;
}

void query_maker(FILE* inp, char* shmq, int* shmm, int qnum, sem_t* done_sem[], sem_t* mem_full_sem, sem_t* query_sem, sem_t* shmm_mutex_sem);

void query_maker(FILE* inp, char* shmq, int* shmm, int qnum, sem_t* done_sem[], sem_t* mem_full_sem, sem_t* query_sem, sem_t* shmm_mutex_sem){
	
	char query[10];
	int temp;
	int rc;
	int z, i; // iterators
	int dummy;
	int sem_value;
	
	for(z = 0; z < 5; z++){
		if(fscanf(inp, "%s", query) != EOF){
			if((rc = sem_wait(mem_full_sem)) != 0){
				printf("sem_post error mem_full_sem\n");
			}
			/* mutex block on writing shared memory starts */
			sem_wait(shmm_mutex_sem);
				temp = *shmm; // index of the location to write the query in
				if(*shmm == 10*(qnum-1)){
					*shmm = 0; // wrap around the last value
				}
				else{
					*shmm = *shmm + 10; // increment the index for future writes
				}
				// write and signal is done in mutex so that it doesn't happen that one process writes and
				// another process signals the server that it has written the query
				for(i = temp; i < temp+10; i++){
					shmq[i] = query[i - temp];
				}
				sem_getvalue(query_sem, &dummy);
				if((rc = sem_post(query_sem)) == 0){
					// printf("signal sent to server by PID = %i\n", getpid());
				}
				else{
					printf("sem_post error query_sem\n");
				}
			sem_post(shmm_mutex_sem);
			/* mutex block on writing shared memory ends */
			gettimeofday(&mytime, NULL);
			printf("%ld %s %i %i in\n",  mytime.tv_usec, query, getpid(), temp/10);
		}

		if((rc = sem_wait(done_sem[temp/10])) != 0){
			printf("sem_wait error done_sem[%i]\n", temp/10);
		}

		// unblock any one of the processes blocked due to space constraint or increment the semaphore value
		if((rc = sem_post(mem_full_sem)) != 0){
			printf("sem_post error mem_full_sem\n");
		}
		
		// arbit computations
		int j = 0;
		for(i = 0; i < 100; i++){
			j = j+i;	
		}
		
		sleep(1);	// sleep for one second

		// sem_close(done_sem[temp/10]);
		gettimeofday(&mytime, NULL);
		printf("%ld %s %i out\n",  mytime.tv_usec, query, getpid());
	}
}

void* query_handler(struct arg_handler* data);

void* query_handler(struct arg_handler* data){

	(data->query)[10] = '\0';
		
	// arbit computations
	int i;
	int j = 0;
	for(i = 0; i < 100; i++){
		j = j+i;	
	}
	
	sleep(2); // sleep for one second
		
	int rc, sem_value;
	if((rc = sem_post(data->arg_sem)) != 0){
		printf("sem_post error\n");
	}
		
	pthread_mutex_lock(&tcurr_mutex);
	tcurr--;
	if(tcurr == ((*(data->max_threads)) - 1)) pthread_cond_signal(&tcurr_cv);
	pthread_mutex_unlock(&tcurr_mutex);
	pthread_exit((void*)0);
}

int main()
{
	FILE* inp;
	 
	int pnum = 6;
	int tnum = 5;
	int qnum = 3;
	
	int rc;
	int shmidq, shmidm, shmidh;
	key_t keyq, keym, keyh;
	char* shmq;
	int* shmm;
	int* shmh;
	char name_init[10];
	 
	keyq = 101; // arbitrary key for the shared memory segments
	keym = 202;
	keyh = 303;

	shmq = shared_memory_char(shmidq, keyq, SHMSZ);
	shmm = shared_memory_int(shmidm, keym, SHMSZINT);
	shmh = shared_memory_int(shmidh, keyh, SHMSZINT);
	 
	// initialise all the locations
	*shmm = 0;
	*shmh = 0;
	int i; // iterator
	for(i = 0; i < SHMSZ; i++){
		// initialise all the shared memory with zeroes
		shmq[i] = '\0';
	}

	pthread_mutexattr_t tcurr_mutex_attr;
	pthread_condattr_t tcurr_cv_attr;
	pthread_mutexattr_init(&tcurr_mutex_attr);
	pthread_condattr_init(&tcurr_cv_attr);
	pthread_mutexattr_setpshared(&tcurr_mutex_attr, PTHREAD_PROCESS_SHARED);
	pthread_condattr_setpshared(&tcurr_cv_attr, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&tcurr_mutex, &tcurr_mutex_attr);
	pthread_cond_init(&tcurr_cv, &tcurr_cv_attr);

	sem_t* done_sem[qnum]; // to wait for the submitted query to get completed
	sem_t* mem_full_sem; // to check if the shared memory is full
	sem_t* query_sem; // to signal the server process that a query is made
	sem_t* shmm_mutex_sem; // to access shmm variable mutually exclusively

	struct arg_handler data[5*pnum];

	for(i = 0; i < qnum; i++){
		sprintf(name_init, "done_sem_sem%02d", i);
		if((done_sem[i] = sem_open(name_init, O_CREAT, 0644, 0)) == SEM_FAILED){
			perror("semaphore initilization done_sem");
			return 1;
		}
	}
	for(i = 0; i < qnum; i++){
		sprintf(name_init, "done_sem_sem%02d", i);
		if((rc = sem_unlink(name_init)) != 0){
			perror("semaphore unlink done_sem");
		}
	}
	
	if((mem_full_sem = sem_open("mem_full_sem", O_CREAT, 0644, qnum)) == SEM_FAILED){
		perror("semaphore initilization mem_full_sem");
		return 1;
	}
	
	if((rc = sem_unlink("mem_full_sem")) != 0){
			perror("semaphore unlink done_sem");
	}
	
	if((query_sem = sem_open("query_sem", O_CREAT, 0644, 0)) == SEM_FAILED){
	perror("semaphore initilization query_sem");
	return 1;
	}
	
	if((rc = sem_unlink("query_sem")) != 0){
			perror("semaphore unlink done_sem");
	}

	if((shmm_mutex_sem = sem_open("shmm_mutex_sem", O_CREAT, 0644, 1)) == SEM_FAILED){
	perror("semaphore initilization shmm_mutex_sem");
	return 1;
	}
	if((rc = sem_unlink("shmm_mutex_sem")) != 0){
			perror("semaphore unlink done_sem");
	}
	
	pid_t pid = fork();

	if(pid == 0){ // mother process
		for(i = 0; i < pnum; i++){
			pid = fork();
			if(pid == 0){
				if(i == 0) inp = fopen("input1", "r");
				else if(i == 1) inp = fopen("input2", "r");
				else if(i == 2) inp = fopen("input3", "r");
				else if(i == 3) inp = fopen("input4", "r");
				else if(i == 4) inp = fopen("input5", "r");
				else if(i == 5) inp = fopen("input6", "r");
				query_maker(inp, shmq, shmm, qnum, done_sem, mem_full_sem, query_sem, shmm_mutex_sem);
				return 0;
			}
			else if(pid > 0){
				// printf("Maker process %i forked, PID = %i\n", i, pid);
			}
			else{
				perror("Error in fork()\n");
			}
		}
	}
	
	else if(pid > 0){ // server process
		int count = 0;
		pthread_t threads[tnum];
		while(count < (5*pnum)){
			sem_wait(query_sem);
			char query[10];
			char print_query[11];
			int i;
			int temp = *shmh;
			// printf("server is reading from %d\n",temp);
			if(*shmh == 10*(qnum-1)){
				*shmh = 0; // wrap around the last value
			}
			else{
				*shmh = *shmh + 10; // increment the index for future writes
			}
			
			if(shmq[temp] != '\0'){
				for(i = temp; i < temp+10; i++){
					query[i - temp] = shmq[i];
					print_query[i - temp] = shmq[i];
					shmq[i] = '\0'; // write NULL in place of queries already read
				}
				print_query[10] = '\0';
			}

			strcpy(data[count].query, print_query);
			data[count].arg_sem = done_sem[temp/10];
			data[count].max_threads = &tnum;
			rc = pthread_create(&threads[count], NULL, query_handler, (void*)&(data[count]));
			if(rc){
			 printf("ERROR; return code from pthread_create() is %d\n", rc);
			 return -1;
			}
			else{
				pthread_mutex_lock(&tcurr_mutex);
				tcurr++;
				if(tcurr == tnum) pthread_cond_wait(&tcurr_cv, &tcurr_mutex);
				pthread_mutex_unlock(&tcurr_mutex);
			}
			count++;
		}
	}
	pthread_mutex_destroy(&tcurr_mutex);
	pthread_mutexattr_destroy(&tcurr_mutex_attr);
	pthread_exit(NULL);
	wait(NULL);
	return 0;
}
