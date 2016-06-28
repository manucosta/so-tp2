#include "Backend_multi.h"

using namespace std;


// variables globales de la conexión
int socket_servidor = -1;

// variables globales del juego
vector<vector<char> > tablero_equipo1; // tiene las fichas del equipo1
vector<vector<char> > tablero_equipo2; // tiene las fichas del equipo2

bool peleando = false;
unsigned int ancho = -1;
unsigned int alto = -1;

//Agregado por nosotros
RWLock mutex_casillero_equipo1; //RWLook para el tablero del equipo 1
RWLock mutex_casillero_equipo2; //RWLook para el tablero del equipo 2
vector<pthread_t> clientes; //vector de threads
int cant_clientes = 0; //la cantidad de jugadores totales
pthread_mutex_t mutex_clientes; //mutex para proteger cant_clientes
pthread_mutex_t mutex_equipos; //mutex para proteger los nombres de los equipos
string nombre_equipo1 = "";
string nombre_equipo2 = "";

bool cargar_int(const char* numero, unsigned int& n) {
	char *eptr;
	n = static_cast<unsigned int>(strtol(numero, &eptr, 10));
	if(*eptr != '\0') {
		cerr << "error: " << numero << " no es un número: " << endl;
		return false;
	}
	return true;
}

int main(int argc, const char* argv[]) {
  //inicializamos los mutex.
  pthread_mutex_init(&mutex_clientes, NULL);
  pthread_mutex_init(&mutex_equipos, NULL);

  // manejo la señal SIGINT para poder cerrar el socket cuando cierra el programa
  signal(SIGINT, cerrar_servidor);

  // parsear argumentos
  if (argc < 3) {
	  cerr << "Faltan argumentos, la forma de uso es:" << endl <<
	  argv[0] << " N M" << endl << "N = ancho del tablero , M = alto del tablero" << endl;
	  return 3;
  }
  else {
	  if (!cargar_int(argv[1], ancho)) {
		  cerr << argv[1] << " debe ser un número" << endl;
		  return 5;
	  }
	  if (!cargar_int(argv[2], alto)) {
		  cerr << argv[2] << " debe ser un número" << endl;
		  return 5;
	  }
  }

  // inicializar ambos tableros, se accede como tablero[fila][columna]
  tablero_equipo1 = vector<vector<char> >(alto);
  for (unsigned int i = 0; i < alto; ++i) {
	  tablero_equipo1[i] = vector<char>(ancho, VACIO);
  }

  tablero_equipo2 = vector<vector<char> >(alto);
  for (unsigned int i = 0; i < alto; ++i) {
	  tablero_equipo2[i] = vector<char>(ancho, VACIO);
  }  

  int socketfd_cliente, socket_size;
  struct sockaddr_in local, remoto;

  // crear un socket de tipo INET con TCP (SOCK_STREAM)
  if ((socket_servidor = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
	  cerr << "Error creando socket" << endl;
  }

  // permito reusar el socket para que no tire el error "Address Already in Use"
  int flag = 1;
  setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

  // crear nombre, usamos INADDR_ANY para indicar que cualquiera puede conectarse aquí
  local.sin_family = AF_INET;
  local.sin_addr.s_addr = INADDR_ANY;
  local.sin_port = htons(PORT);
  if (bind(socket_servidor, (struct sockaddr *)&local, sizeof(local)) == -1) {
	  cerr << "Error haciendo bind!" << endl;
	  return 1;
  }

  // escuchar en el socket
  if (listen(socket_servidor, 1) == -1) {
	  cerr << "Error escuchando socket!" << endl;
	  return 1;
  }

  // aceptar conexiones entrantes.
  socket_size = sizeof(remoto);
  while (!peleando) { //recibo jugadores mientras todavía estoy en la fase de crear barcos.
  	if ((socketfd_cliente = accept(socket_servidor, (struct sockaddr*) &remoto, (socklen_t*) &socket_size)) == -1)
      /*
      accept es bloqueante, eso genera una situacion medio rara.
      Preguntar despues
      */
  		cerr << "Error al aceptar conexion" << endl;
  	else {
  		if(peleando) break;
  		pthread_t cliente; //Por cada nuevo jugador creamos un nuevo thread
  		pthread_mutex_lock(&mutex_clientes);
  		clientes.push_back(cliente);
  		cant_clientes++;
  		pthread_create(&clientes[clientes.size()-1], NULL, atendedor_de_jugador, &socketfd_cliente); //El nuevo thread empieza en el atendedor_de_jugador
  		pthread_mutex_unlock(&mutex_clientes);
  	}
  }
  close(socket_servidor);
  //Espero a todos los threads para irme
  for(unsigned int i = 0; i < clientes.size(); i++){
    pthread_join(clientes[i], NULL);
  }

  return 0;
}


void* atendedor_de_jugador(void* p_socket_fd) {
	bool listo = false; //Variable para indicar si el jugador ya esta listo para pelear o no
	int socket_fd = *((int*) p_socket_fd);
	// variables locales del jugador
	char nombre_equipo[21];

	 // lista de casilleros que ocupa el barco actual (aún no confirmado)
	list<Casillero> barco_actual;

	if (recibir_nombre_equipo(socket_fd, nombre_equipo) != 0) {
		// el cliente cortó la comunicación, o hubo un error. Cerramos todo.
		terminar_servidor_de_jugador(socket_fd, barco_actual, tablero_equipo1, true, listo);
		pthread_exit(NULL); //como terminamos el servidor del jugador devolvemos NULL para que el thread termine
	}

	if (enviar_dimensiones(socket_fd) != 0) {
		// se produjo un error al enviar. Cerramos todo.
		terminar_servidor_de_jugador(socket_fd, barco_actual, tablero_equipo1, true, listo);
		pthread_exit(NULL); //como terminamos el servidor del jugador devolvemos NULL para que el thread termine
	}

	bool soy_equipo_1; //variable para saber si soy equipo 1 o equipo 2
	if(nombre_equipo1 == "") { //si equipo 1 no tiene nombre, creamos el equipo 1 con el nombre que introdujo el jugador
	  nombre_equipo1 = nombre_equipo;
	  soy_equipo_1 = true;
	} 
	else if(nombre_equipo1 == nombre_equipo) { //soy equipo 1
	  soy_equipo_1 = true;
	}
	else if(nombre_equipo2 == "") { //no soy del equipo 1 y todavia no estaba creado el equipo 2. Así que los creo 
	  nombre_equipo2 = nombre_equipo;
	  soy_equipo_1 = false;
	}
	else if(nombre_equipo2 == nombre_equipo) { //soy del equipo 2
	  soy_equipo_1 = false;
	}
	else{ //introduje un tercer nombre de equipo
	  cout << "Nombre inválido" << endl;
    terminar_servidor_de_jugador(socket_fd, barco_actual, tablero_equipo1, true, listo);
	  pthread_exit(NULL);
	}
  cout << "Esperando que juegue " << nombre_equipo << endl;

	vector<vector<char> > *tablero_jugador;
	vector<vector<char> > *tablero_rival;

	//Veo que tablero usar dependiendo el equipo que soy
	if(!soy_equipo_1){
		tablero_jugador = &tablero_equipo2;
		tablero_rival = &tablero_equipo1;
	}else{
		tablero_jugador = &tablero_equipo1;
		tablero_rival = &tablero_equipo2;
	}

	while (true) {

		// espera un barco o confirmación de juego
		char mensaje[MENSAJE_MAXIMO+1];
		int comando = recibir_comando(socket_fd, mensaje);

		if (comando == MSG_PARTE_BARCO) {
			Casillero ficha;

			//Si estoy peleando, no acepto barcos ya
			if(listo){
				if (enviar_error(socket_fd) != 0) {
					// se produjo un error al enviar. Cerramos todo.
					terminar_servidor_de_jugador(socket_fd, barco_actual, *tablero_jugador, soy_equipo_1, listo);
					pthread_exit(NULL); //como terminamos el servidor del jugador devolvemos NULL para que el thread termine
				}

				continue;
			}

			if (parsear_barco(mensaje, ficha) != 0) {
				// no es un mensaje PARTE_BARCO bien formado, hacer de cuenta que nunca llegó
				continue;
			}

			// ficha contiene el nuevo barco a colocar
			// verificar si es una posición válida del tablero

			if (es_ficha_valida(ficha, barco_actual, *tablero_jugador, soy_equipo_1)) {
				barco_actual.push_back(ficha);
        //agrego al tablero una b para diferenciar las artes de barcos de los barcos terminados.
				if(soy_equipo_1){
					mutex_casillero_equipo1.wlock();
					(*tablero_jugador)[ficha.fila][ficha.columna] = 'b';
					mutex_casillero_equipo1.wunlock();
				}else{
					mutex_casillero_equipo2.wlock();
					(*tablero_jugador)[ficha.fila][ficha.columna] = 'b';
					mutex_casillero_equipo2.wunlock();
				}

        if (enviar_ok(socket_fd) != 0) {
					// se produjo un error al enviar. Cerramos todo.
					terminar_servidor_de_jugador(socket_fd, barco_actual, *tablero_jugador, soy_equipo_1, listo);
					pthread_exit(NULL); //como terminamos el servidor del jugador devolvemos NULL para que el thread termine
				}
				// OK
			}
			else {
				// ERROR
				if (enviar_error(socket_fd) != 0) {
					// se produjo un error al enviar. Cerramos todo.
					terminar_servidor_de_jugador(socket_fd, barco_actual, *tablero_jugador, soy_equipo_1, listo);
					pthread_exit(NULL); //como terminamos el servidor del jugador devolvemos NULL para que el thread termine
				}
				quitar_partes_barco(barco_actual, *tablero_jugador, soy_equipo_1);
			}
		}
		else if (comando == MSG_LISTO) {
			// El único cliente terminó de ubicar sus barcos

			//Si ya había terminado, enviar error
			if(listo){
				if (enviar_error(socket_fd) != 0) {
					// se produjo un error al enviar. Cerramos todo.
					terminar_servidor_de_jugador(socket_fd, barco_actual, *tablero_jugador, soy_equipo_1, listo);
					pthread_exit(NULL);
				}

			}else{
				// Estamos listos para la pelea

				pthread_mutex_lock(&mutex_clientes);
        cant_clientes--; //Hay un jugador menos en la fase de crear barcos
        listo = true; //El jugador ya termino de poner sus barcos
        if(cant_clientes == 0){ //si soy el ultimo en salir de la fase de crear barcos pongo la variable global en true
        	peleando = true;
        }
        pthread_mutex_unlock(&mutex_clientes);

				if (enviar_ok(socket_fd) != 0){
				 	terminar_servidor_de_jugador(socket_fd, barco_actual, *tablero_jugador, soy_equipo_1, listo);
				 	pthread_exit(NULL); //como terminamos el servidor del jugador devolvemos NULL para que el thread termine
				}
			}

		}
		else if (comando == MSG_BARCO_TERMINADO) {


			//Si estoy listo para pelear, no acepto barcos ya
			if(listo){
				if (enviar_error(socket_fd) != 0) {
					// se produjo un error al enviar. Cerramos todo.
					terminar_servidor_de_jugador(socket_fd, barco_actual, *tablero_jugador, soy_equipo_1, listo);
					pthread_exit(NULL); //como terminamos el servidor del jugador devolvemos NULL para que el thread termine
				}
				continue;
			}

			// las partes acumuladas conforman un barco completo, escribirlas en el tablero del jugador y borrar las partes temporales

			for (list<Casillero>::const_iterator casillero = barco_actual.begin(); casillero != barco_actual.end(); casillero++) {
				if(soy_equipo_1) {
	      	mutex_casillero_equipo1.wlock();
					(*tablero_jugador)[casillero->fila][casillero->columna] = casillero->contenido;
	      	mutex_casillero_equipo1.wunlock();
	      } else {
	      	mutex_casillero_equipo2.wlock();
					(*tablero_jugador)[casillero->fila][casillero->columna] = casillero->contenido;
	      	mutex_casillero_equipo2.wunlock();
	      }
			}

			if (enviar_ok(socket_fd) != 0) {
				// se produjo un error al enviar. Cerramos todo.
				terminar_servidor_de_jugador(socket_fd, barco_actual, *tablero_jugador, soy_equipo_1, listo);
				pthread_exit(NULL); //como terminamos el servidor del jugador devolvemos NULL para que el thread termine
			}
			
			barco_actual.clear();
		}
		else if (comando == MSG_BOMBA) {

			Casillero ficha;
			if (parsear_bomba(mensaje, ficha) != 0) {
				// no es un mensaje BOMBA bien formado, hacer de cuenta que nunca llegó
				continue;
			}

			// ficha contiene la bomba a tirar
			// verificar si se está peleando y si es una posición válida del tablero
			if (peleando && ficha.fila <= alto - 1 && ficha.columna <= ancho - 1) {

				//Si había un BARCO, pongo una BOMBA
				char contenido; //leo para ver que hay en esa posicion
				if(soy_equipo_1){
		      mutex_casillero_equipo1.rlock();
		      contenido = (*tablero_rival)[ficha.fila][ficha.columna];
		      mutex_casillero_equipo1.runlock();
          if(contenido == BARCO){
            //es un barco => pongo una bomba
            mutex_casillero_equipo1.wlock();
			      (*tablero_rival)[ficha.fila][ficha.columna] = BOMBA;
			      mutex_casillero_equipo1.wunlock();
          }
				}else{
					mutex_casillero_equipo2.rlock();
		      contenido = (*tablero_rival)[ficha.fila][ficha.columna];
		      mutex_casillero_equipo2.runlock();
          if(contenido == BARCO){
            //es un barco => pongo una bomba
            mutex_casillero_equipo2.wlock();
			      (*tablero_rival)[ficha.fila][ficha.columna] = BOMBA;
			      mutex_casillero_equipo2.wunlock();
          }
				}
        if(contenido == BARCO){
					if (enviar_golpe(socket_fd) != 0) {
						// se produjo un error al enviar. Cerramos todo.
						terminar_servidor_de_jugador(socket_fd, barco_actual, *tablero_jugador, soy_equipo_1, listo);
						pthread_exit(NULL); //como terminamos el servidor del jugador devolvemos NULL para que el thread termine
					}

				}else if(contenido == BOMBA){
					// OK
					if (enviar_estaba_golpeado(socket_fd) != 0) {
						// se produjo un error al enviar. Cerramos todo.
						terminar_servidor_de_jugador(socket_fd, barco_actual, *tablero_jugador, soy_equipo_1, listo);
						pthread_exit(NULL); //como terminamos el servidor del jugador devolvemos NULL para que el thread termine
					}
				}else{
					// OK
					if (enviar_ok(socket_fd) != 0) {
						// se produjo un error al enviar. Cerramos todo.
						terminar_servidor_de_jugador(socket_fd, barco_actual, *tablero_jugador, soy_equipo_1, listo);
						pthread_exit(NULL); //como terminamos el servidor del jugador devolvemos NULL para que el thread termine
					}
				}
			}else {
				// ERROR
				if (enviar_error(socket_fd) != 0) {
					// se produjo un error al enviar. Cerramos todo.
					terminar_servidor_de_jugador(socket_fd, barco_actual, *tablero_jugador, soy_equipo_1, listo);
					pthread_exit(NULL); //como terminamos el servidor del jugador devolvemos NULL para que el thread termine
				}
			}

		}
		else if (comando == MSG_UPDATE) {
			if (enviar_tablero(socket_fd, soy_equipo_1) != 0) {
				// se produjo un error al enviar. Cerramos todo.
				terminar_servidor_de_jugador(socket_fd, barco_actual, *tablero_jugador, soy_equipo_1, listo);
				pthread_exit(NULL); //como terminamos el servidor del jugador devolvemos NULL para que el thread termine
			}
		}
		else if (comando == MSG_INVALID) {
			// no es un mensaje válido, hacer de cuenta que nunca llegó
			continue;
		}
		else {
			// se produjo un error al recibir. Cerramos todo.
			terminar_servidor_de_jugador(socket_fd, barco_actual, *tablero_jugador, soy_equipo_1, listo);
			pthread_exit(NULL); //como terminamos el servidor del jugador devolvemos NULL para que el thread termine
		}
	}
}


// mensajes recibidos por el server

int recibir_nombre_equipo(int socket_fd, char* nombre) {
	char buf[MENSAJE_MAXIMO+1];

	if (recibir(socket_fd, buf) != 0) {
		return -1;
	}

	int res = sscanf(buf, "EQUIPO %20s", nombre);

	if (res == EOF || res != 1) {
		cerr << "ERROR: no se pudo leer el nombre del equipo" << endl;
		return -1;
	}

	return 0;
}

// informa el tipo de comando recibido (o si es inválido)
// deja el mensaje en mensaje por si necesita seguir parseando
int recibir_comando(int socket_fd, char* mensaje) {
	if (recibir(socket_fd, mensaje) != 0) {
		return -1;
	}

	char comando[MENSAJE_MAXIMO];
	sscanf(mensaje, "%s", comando);

	if (strcmp(comando, "PARTE_BARCO") == 0) {
		// el mensaje es PARTE_BARCO
		return MSG_PARTE_BARCO;
	}
	else if (strcmp(comando, "BARCO_TERMINADO") == 0) {
		// el mensaje es BARCO_TERMINADO
		return MSG_BARCO_TERMINADO;
	}
	else if (strcmp(comando, "LISTO") == 0) {
		// el mensaje es LISTO
		return MSG_LISTO;
	}
	else if (strcmp(comando, "BOMBA") == 0) {
		// el mensaje es BOMBA
		return MSG_BOMBA;
	}
	else if (strcmp(comando, "UPDATE") == 0) {
		// el mensaje es UPDATE
		return MSG_UPDATE;
	}
	else {
		cerr << "ERROR: mensaje no válido" << endl;
		return MSG_INVALID;
	}
}

int parsear_bomba(char* mensaje, Casillero& ficha) {

	int cant = sscanf(mensaje, "BOMBA %d %d", &ficha.fila, &ficha.columna);
	ficha.contenido = BOMBA;

	if (cant == 2) {
		//El mensaje BARCO es válido
		return 0;
	}
	else {
		cerr << "ERROR: " << mensaje << " no está bien formado. Debe ser BOMBA <fila> <columna>" << endl;
		return -1;
	}
}

int parsear_barco(char* mensaje, Casillero& ficha) {

	int cant = sscanf(mensaje, "PARTE_BARCO %d %d", &ficha.fila, &ficha.columna);
	ficha.contenido = BARCO;

	if (cant == 2) {
		//El mensaje PARTE_BARCO es válido
		return 0;
	}
	else {
		cerr << "ERROR: " << mensaje << " no está bien formado. Debe ser PARTE_BARCO <fila> <columna>" << endl;
		return -1;
	}
}



// mensajes enviados por el server

int enviar_dimensiones(int socket_fd) {
	char buf[MENSAJE_MAXIMO+1];
	sprintf(buf, "TABLERO %d %d", ancho, alto);
	return enviar(socket_fd, buf);
}

int enviar_tablero(int socket_fd, bool equipo1) {
	char buf[MENSAJE_MAXIMO+1];
	int pos;
	vector<vector<char> > *tablero;

	//Si no estoy peleando, muestro los barcos de mi equipo
	if(!peleando){
		sprintf(buf, "BARCOS ");
    if(equipo1){
      tablero = &tablero_equipo1;
		}else{
      tablero = &tablero_equipo2;
    }
    pos = 7;
	}else{
	//Sino muestro los resultados de la batalla
		sprintf(buf, "BATALLA ");
		if(equipo1){
            tablero = &tablero_equipo2;
		}else{
            tablero = &tablero_equipo1;
        }
		pos = 8;
	}

	for (unsigned int fila = 0; fila < alto; ++fila) {
		for (unsigned int col = 0; col < ancho; ++col) {
			char contenido;
      //leo el contenido de cada casillero del tablero
			if(equipo1){
	      mutex_casillero_equipo1.rlock();
	      contenido = (*tablero)[fila][col];
	      mutex_casillero_equipo1.runlock();
			}else{
	      mutex_casillero_equipo2.rlock();
	      contenido = (*tablero)[fila][col];
	      mutex_casillero_equipo2.runlock();
	    }
			switch(contenido){
				case VACIO:
				  buf[pos] = '-';
				  break;
				case BARCO:
				  //si estoy peleando, oculto los barcos. Sino, los muestro
				  buf[pos] = peleando ? '-' : 'B';
				  break;
				case BOMBA:
					buf[pos] = '*';
					break;
				case 'b': //si es un barco no terminado lo oculto
					buf[pos] = '-';
			}

			pos++;
		}
	}
	buf[pos] = 0; //end of buffer
	cout << endl;

	return enviar(socket_fd, buf);
}

int enviar_ok(int socket_fd) {
	char buf[MENSAJE_MAXIMO+1];
	sprintf(buf, "OK");
	return enviar(socket_fd, buf);
}

int enviar_golpe(int socket_fd) {
	char buf[MENSAJE_MAXIMO+1];
	sprintf(buf, "GOLPE");
	return enviar(socket_fd, buf);
}

int enviar_estaba_golpeado(int socket_fd) {
	char buf[MENSAJE_MAXIMO+1];
	sprintf(buf, "ESTABA_GOLPEADO");
	return enviar(socket_fd, buf);
}

int enviar_error(int socket_fd) {
	char buf[MENSAJE_MAXIMO+1];
	sprintf(buf, "ERROR");
	return enviar(socket_fd, buf);
}


// otras funciones
void cerrar_servidor(int signal) {
	cout << "¡Adiós mundo cruel!" << endl;
	if (socket_servidor != -1)
		close(socket_servidor);
	exit(EXIT_SUCCESS);
}

void terminar_servidor_de_jugador(int socket_fd, list<Casillero>& barco_actual, vector<vector<char> >& tablero_cliente, bool equipo, bool listo) {

	cout << "Se interrumpió la comunicación con un cliente" << endl;
  pthread_mutex_lock(&mutex_clientes);
  if(!listo) cant_clientes--;
  pthread_mutex_unlock(&mutex_clientes);
	close(socket_fd);
	quitar_partes_barco(barco_actual, tablero_cliente, equipo);
}


void quitar_partes_barco(list<Casillero>& barco_actual, vector<vector<char> >& tablero_cliente, bool equipo) {
	for (list<Casillero>::const_iterator casillero = barco_actual.begin(); casillero != barco_actual.end(); casillero++) {
    //pasamos por cada lugar de barco actual poniendo VACIO en vez de b
		if(equipo){
			mutex_casillero_equipo1.wlock();
			tablero_cliente[casillero->fila][casillero->columna] = VACIO;
			mutex_casillero_equipo1.wunlock();
		}else{
			mutex_casillero_equipo2.wlock();
			tablero_cliente[casillero->fila][casillero->columna] = VACIO;
			mutex_casillero_equipo2.wunlock();
		}
	}
	barco_actual.clear();
}


bool es_ficha_valida(const Casillero& ficha, const list<Casillero>& barco_actual, const vector<vector<char> >& tablero, bool equipo) {

	// si está fuera del tablero, no es válida
	if (ficha.fila > alto - 1 || ficha.columna > ancho - 1) {
		return false;
	}

	// Leo el casillero, si está ocupado, tampoco es válida
	if(equipo){
		mutex_casillero_equipo1.rlock();
		if (tablero[ficha.fila][ficha.columna] != VACIO) {
			mutex_casillero_equipo1.runlock();
      return false;
		}
    mutex_casillero_equipo1.runlock();
		
	}else{
		mutex_casillero_equipo2.rlock();
		if (tablero[ficha.fila][ficha.columna] != VACIO) {
			mutex_casillero_equipo2.runlock();
      return false;
		}
    mutex_casillero_equipo2.runlock();
	}

	if (barco_actual.size() > 0) {
		// no es la primera parte del barco, ya hay fichas colocadas para este barco
		Casillero mas_distante = casillero_mas_distante_de(ficha, barco_actual);
		int distancia_vertical = ficha.fila - mas_distante.fila;
		int distancia_horizontal = ficha.columna - mas_distante.columna;

		if (distancia_vertical == 0) {
			// el barco es horizontal
			for (list<Casillero>::const_iterator casillero = barco_actual.begin(); casillero != barco_actual.end(); casillero++) {
				if (ficha.fila - casillero->fila != 0) {
					// no están alineadas horizontalmente
					return false;
				}
			}

			int paso = distancia_horizontal / abs(distancia_horizontal);
			for (unsigned int columna = mas_distante.columna; columna != ficha.columna; columna += paso) {
				// el casillero DEBE estar ocupado en el tablero
				if(equipo){
					mutex_casillero_equipo1.rlock();
					if (!(puso_barco_en(ficha.fila, columna, barco_actual)) && tablero[ficha.fila][columna] == VACIO) {
						mutex_casillero_equipo1.runlock();
            return false;
					}
					mutex_casillero_equipo1.runlock();
				}else{
					mutex_casillero_equipo2.rlock();
					if (!(puso_barco_en(ficha.fila, columna, barco_actual)) && tablero[ficha.fila][columna] == VACIO) {
						mutex_casillero_equipo2.runlock();
            return false;
					}
					mutex_casillero_equipo2.runlock();
				}
				
			}

		} else if (distancia_horizontal == 0) {
			// el barco es vertical
			for (list<Casillero>::const_iterator casillero = barco_actual.begin(); casillero != barco_actual.end(); casillero++) {
				if (ficha.columna - casillero->columna != 0) {
					// no están alineadas verticalmente
					return false;
				}
			}

			int paso = distancia_vertical / abs(distancia_vertical);
			for (unsigned int fila = mas_distante.fila; fila != ficha.fila; fila += paso) {
				// el casillero DEBE estar ocupado en el tablero
				if(equipo){
					mutex_casillero_equipo1.rlock();
					if (!(puso_barco_en(fila, ficha.columna, barco_actual)) && tablero[fila][ficha.columna] == VACIO) {
						mutex_casillero_equipo1.runlock();
            return false;
					}
					mutex_casillero_equipo1.runlock();
				}else{
					mutex_casillero_equipo2.rlock();
					if (!(puso_barco_en(fila, ficha.columna, barco_actual)) && tablero[fila][ficha.columna] == VACIO) {
						mutex_casillero_equipo2.runlock();
            return false;
					}
					mutex_casillero_equipo2.runlock();
				}
			}
		}
		else {
			// no están alineadas ni horizontal ni verticalmente
			return false;
		}
	}

	return true;
}


Casillero casillero_mas_distante_de(const Casillero& ficha, const list<Casillero>& barco_actual) {
	const Casillero* mas_distante;
	int max_distancia = -1;
	for (list<Casillero>::const_iterator casillero = barco_actual.begin(); casillero != barco_actual.end(); casillero++) {
		int distancia = max<unsigned int>(abs((int)(casillero->fila - ficha.fila)), abs((int)(casillero->columna - ficha.columna)));
		if (distancia > max_distancia) {
			max_distancia = distancia;
			mas_distante = &*casillero;
		}
	}

	return *mas_distante;
}


bool puso_barco_en(unsigned int fila, unsigned int columna, const list<Casillero>& barco_actual) {
	for (list<Casillero>::const_iterator casillero = barco_actual.begin(); casillero != barco_actual.end(); casillero++) {
		if (casillero->fila == fila && casillero->columna == columna)
			return true;
	}
	// si no encontró
	return false;
}

