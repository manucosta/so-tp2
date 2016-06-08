#include "RWLock.h"

RWLock :: RWLock() {
	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&cv_write, NULL);
	pthread_cond_init(&cv_read, NULL);
	cant_reads = 0; //no hay nadie leyendo
	write = false; //no llego nadie a escribir
}

void RWLock :: rlock() {
	pthread_mutex_lock(&mutex); //tomo el mutex
	while(write == true){ //me ijo que no haya llegado nadie para escribir
		//si alguien llego dejo de aceptar lecturas, hasta que escriba, ya que sino podria generarse inanición
		pthread_cond_wait(&cv_read, &mutex);
	}
	cant_reads++; //lo sumo a los que ya estaban leyendo
	pthread_mutex_unlock(&mutex); //libero el mutex para que lo pueda tomar otro mientras que yo leo
}

void RWLock :: wlock() {
	pthread_mutex_lock(&mutex); //tomo el mutex
	while(write == true){ //Espero a que termine de escribir el anterior para avisar que llego una escritura
		pthread_cond_wait(&cv_read, &mutex);
	}
	write = true; //aviso que llego una escritura
	while(cant_reads != 0) { //espero a que terminen los que estaban leyendo
		pthread_cond_wait(&cv_write, &mutex);
	}
	//no libero el mutex ya que mientras escribo no quiero que nadie lea ni escriba
}

void RWLock :: runlock() {
	pthread_mutex_lock(&mutex); //tomo el mutex
	cant_reads--; //dejo de leer por lo que me quito de los que estan leyendo
	if(cant_reads == 0) { //si soy el ultimo leyendo le aviso a los demas que pueden escribir
		pthread_cond_broadcast(&cv_write);
	}
	pthread_mutex_unlock(&mutex); //libero el mutex
}

void RWLock :: wunlock() {
	write = false; //reinicio la variable para que otro pueda avisar que quiere escribir
	pthread_cond_broadcast(&cv_read); //le aviso a las lecturas que ya termine de escribir
	pthread_mutex_unlock(&mutex); //libero el mutex
}
