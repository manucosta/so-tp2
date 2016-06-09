#include <string>
#include "RWLock.h"
#include <stdlib.h> /* srand, rand */
#include <vector>
#include <assert.h> 
#include <unistd.h> /* sleep */ 

RWLock RWmutex;
//Test 1
int leyendo = 0;
bool escribiendo = false;
pthread_mutex_t mutex_leyendo;
pthread_mutex_t mutex_escribiendo;
//Test 2
pthread_mutex_t mutex;

void* escribir1(void *vargp) {
	RWmutex.wlock();
	pthread_mutex_lock(&mutex_leyendo);
		assert(leyendo == 0);
	pthread_mutex_unlock(&mutex_leyendo);
	
	pthread_mutex_lock(&mutex_escribiendo);
		assert(escribiendo == false);
		escribiendo = true;
	pthread_mutex_unlock(&mutex_escribiendo);
	
	sleep(0.5);
	
	pthread_mutex_lock(&mutex_escribiendo);
		escribiendo = false;
	pthread_mutex_unlock(&mutex_escribiendo);
	RWmutex.wunlock();
	return NULL;
}

void* leer1(void *vargp) {
	RWmutex.rlock();
	pthread_mutex_lock(&mutex_escribiendo);
		assert(escribiendo == false);
	pthread_mutex_unlock(&mutex_escribiendo);
	
	pthread_mutex_lock(&mutex_leyendo);
		leyendo++;
		cout << "#Leyendo actualmente: " << leyendo << endl;
	pthread_mutex_unlock(&mutex_leyendo);
	
	sleep(0.5);
	
	pthread_mutex_lock(&mutex_leyendo);
		leyendo--;
	pthread_mutex_unlock(&mutex_leyendo);
	RWmutex.runlock();
	return NULL;
}

void* escribir2(void *vargp) {
	int id = *((int*) vargp);
	RWmutex.wlock();
		pthread_mutex_lock(&mutex);
		cout << "Escribo: " << id << endl;
		pthread_mutex_unlock(&mutex);
		
		sleep(0.5);
	
	RWmutex.wunlock();
	return NULL;
}

void* leer2(void *vargp) {
	int id = *((int*) vargp);
	RWmutex.rlock();
		pthread_mutex_lock(&mutex);
			cout << "Leo: " << id << endl;
		pthread_mutex_unlock(&mutex);
		
		sleep(0.5);
	
	RWmutex.runlock();
	return NULL;
}


//Comprueba que el comportamiento de los RWlocks sea el esperado
void test1(int n) {
	pthread_mutex_init(&mutex_leyendo, NULL);
	pthread_mutex_init(&mutex_escribiendo, NULL);

	vector<pthread_t> threads(n);
	vector<int> aux(n);
	srand (time(NULL));

	for(int i = 0; i < n; i++){
		if(rand() % 2 == 0) {
			pthread_create(&threads[i], NULL, leer1, NULL);
		} else {
			pthread_create(&threads[i], NULL, escribir1, NULL);
		}
	}

	for(int i = 0; i < n; i++){
		pthread_join(threads[i], NULL);
	}
}

//Sirve para ver que no se produzca inanición
void test2(int n) {
	pthread_mutex_init(&mutex, NULL);
	vector<pthread_t> threads(n);
	vector<int> aux(n);


	//Comprobamos que los threads de lectura no le produzcan
	//inanición a los de escritura
	cout << "### Inanición 1 ###" << endl;
	
	//Pongo la mitad de los threads a leer	
	for(int i = 0; i < n/2; i++){
		aux[i] = i;
		pthread_create(&threads[i], NULL, leer2, &aux[i]);
	}
	
	//Mando un thread a escribir, mientras otros están leyendo
	aux[n/2] = n/2;
	pthread_create(&threads[n/2], NULL, escribir2, &aux[n/2]);
	
	//Mando el resto de los thread a leer
	for(int i = n/2 + 1; i < n; i++){
		aux[i] = i;
		pthread_create(&threads[i], NULL, leer2, &aux[i]);
	}

	for(int i = 0; i < n; i++){
		pthread_join(threads[i], NULL);
	}

	//Comprobamos que los threads de escritura no le produzcan
	//inanición a los de lectura	
	cout << "\n### Inanición 2 ###" << endl;

	//Pongo la mitad de los threads a escribir
	for(int i = 0; i < n/2; i++){
		aux[i] = i;
		pthread_create(&threads[i], NULL, escribir2, &aux[i]);
	}

	//Mando un thread a leer, mientras otros están escribiendo
	aux[n/2] = n/2;
	pthread_create(&threads[n/2], NULL, leer2, &aux[n/2]);

	//Mando el resto de los thread a escribir
	for(int i = n/2 + 1; i < n; i++){
		aux[i] = i;
		pthread_create(&threads[i], NULL, escribir2, &aux[i]);
	}

	for(int i = 0; i < n; i++){
		pthread_join(threads[i], NULL);
	}
}

int main(int argc, char const *argv[]) {
	//argv[1] = tipo de test (1 o 2)
	//argv[2] = cantidad de threads que se quieren correr
	if(argc != 3) {
		cout << "ENTRADA INCORRECTA!" << endl;
		cout << "El formato es ./RWLockTest <tipo> <#threads>, donde tipo puede ser 1 o 2" << endl;
		return -1;
	}
	
	int test = atoi(argv[1]);
	int n = atoi(argv[2]);

	if(test == 1) {
		test1(n);
	}
	else if(test == 2) {
		test2(n);
	}

	return 0;
}
