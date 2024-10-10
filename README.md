# OpenMPI-Project

## Project #02 - Parallel and distributed computing

- <strong>Universidad del Valle de Guatemala</strong>

- <strong>Computación Paralela y Distribuida</strong>


## Autores:

Kristopher Javier Alvarado López	 |  Carné 21188

David Jonathan Aragon Vasquez 		 |  Carné 21053

Renatto Esteban Guzman Sosa		   |  Carné 21646

## Correr los programas

### • bruteforce_modified.c

Compilar con:

```mpicc -o bruteforce_modified bruteforce_modified.c -lcrypto```
 
Ejecutar con:

```mpirun -np \<N procesos> ./bruteforce_modified```


### • bruteforce_m_sm.c

Compilar con:

```mpicc -o bruteforce_m_sm bruteforce_m_sm.c -lcrypto```
 
Ejecutar con:

```mpirun -np \<N procesos> ./bruteforce_m_sm```


### • bruteforce_balancer.c

Compilar con:

```mpicc -o bruteforce_balancer bruteforce_balancer.c -lcrypto```
 
Ejecutar con:

```mpirun -np \<N procesos> ./bruteforce_balancer \<texto> \<clave>```

### • bruteforce_hibrid.c

Compilar con:

```mpicc -o bruteforce_hibrid bruteforce_hibrid.c -lcrypto```
 
Ejecutar con:

```mpirun -np \<N procesos> ./bruteforce_hibrid \<texto> \<clave>```

### • bruteforce_mpi.c

Compilar con:

```mpicc -o bruteforce_mpi bruteforce_mpi.c -lcrypto```
 
Ejecutar con:

```mpirun -np \<N procesos> ./bruteforce_mpi \<texto> \<clave>```


### • des_bruteforce.c

Compilar con:

```gcc -o des_bruteforce des_bruteforce.c```
 
Ejecutar con:

```./des_bruteforce```
