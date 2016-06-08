#include <string>
#include "RWLock.h"
#include <stdlib.h>
#include <vector>

string death_note;
RWLock mutex;

void* escribir(void *vargp) {
	int miNumero = *((int*)vargp);
	mutex.wlock();
	cout << "Estoy escribiendo" << endl;
	death_note = "Obama Obaca";
	cout << miNumero << ": Escribi Obama Obaca." << endl;
	mutex.wunlock();
	return NULL;
}

void* leer(void *vargp) {
	int miNumero = *((int*)vargp);
	mutex.rlock();
	cout << "Estoy leyendo" << endl;
	cout << miNumero << ": Leí: " << death_note << "." << endl;	
	mutex.runlock();
	return NULL;
}

int main(int argc, char const *argv[]) {
	death_note = "NULL";
	int n = atoi(argv[1]);
	vector<pthread_t> threads(n);
	vector<int> arreglin(n);
	if()
	for(int i = 0; i < n; i += 2){ //creo la mitad de los threads para leer y la otra mitad para escribir
		arreglin[i] = i;
		arreglin[i+1] = i+1;
		pthread_create(&threads[i], NULL, leer, &arreglin[i]);
		pthread_create(&threads[i+1], NULL, escribir, &arreglin[i+1]);
	}

	for(int i = 0; i < n; i += 2){
		pthread_join(threads[i], NULL);
		pthread_join(threads[i+1], NULL);
	}

	return 0;
}
