#ifndef RWLock_h
#define RWLock_h
#include <iostream>
#include <pthread.h>

using namespace std;

class RWLock {
    public:
      RWLock();
      void rlock();
      void wlock();
      void runlock();
      void wunlock();

    private:
    	pthread_mutex_t mutex;
    	pthread_cond_t cv_write, cv_read;
    	bool write;
    	int cant_reads; // Guarda la cantidad de "gente" leyendo.
};

#endif
