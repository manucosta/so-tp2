CC = g++
CFLAGS = -g -Wall -Werror --std=c++11
LDFLAGS = -pthread

BINARIES = backend RWLockTest

all: $(BINARIES)

backend: Backend_multi.cpp Backend_multi.h Enviar_recibir.o Enviar_recibir.h Encabezado.h Casillero.h RWLock.h RWLock.o
	$(CC) $(LDFLAGS) $(CFLAGS) Backend_multi.cpp Enviar_recibir.o RWLock.o -o backend

Enviar_recibir.o: Enviar_recibir.cpp Enviar_recibir.h Encabezado.h
	$(CC) $(CFLAGS) -c Enviar_recibir.cpp

RWLock.o: RWLock.cpp RWLock.h
	$(CC) $(CFLAGS) -c RWLock.cpp

RWLockTest: RWLock.cpp RWLock.h RWLockTest.cpp
	$(CC) $(LDFLAGS) $(CFLAGS) RWLockTest.cpp RWLock.cpp -o RWLockTest

clean:
	rm -f *.o
	rm -f $(BINARIES)
