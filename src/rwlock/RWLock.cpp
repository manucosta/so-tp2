#include "RWLock.h"

RWLock :: RWLock() {
	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&cv_write, NULL);
	pthread_cond_init(&cv_read, NULL);
	cant_reads = 0;
	write = false;
}

void RWLock :: rlock() {
	pthread_mutex_lock(&mutex);
	while(write == true){
		pthread_cond_wait(&cv_read, &mutex);
	}
	cant_reads++;
	pthread_mutex_unlock(&mutex);
}

void RWLock :: wlock() {
	pthread_mutex_lock(&mutex);
	while(write == true){
		pthread_cond_wait(&cv_read, &mutex);
	}
	write = true;
	while(cant_reads != 0) {
		pthread_cond_wait(&cv_write, &mutex);
	}
}

void RWLock :: runlock() {
	pthread_mutex_lock(&mutex);
	cant_reads--;
	if(cant_reads == 0) {
		pthread_cond_broadcast(&cv_write);
	}
	pthread_mutex_unlock(&mutex);
}

void RWLock :: wunlock() {
	write = false;
	pthread_cond_broadcast(&cv_read);
	pthread_mutex_unlock(&mutex);
}
