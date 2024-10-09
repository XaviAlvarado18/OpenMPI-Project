// Universidad del Valle de Guatemala
// Computación paralela y distribuida
// 
// Autores:
// Kristopher Javier Alvarado López	 |  Carné 21188
// David Jonathan Aragon Vasquez 		 |  Carné 21053
// Renatto Esteban Guzman Sosa		   |  Carné 21646

// Instalar librerias de desarrollo de openssl y openmpi
// sudo apt-get install libssl-dev libopenmpi-dev
// 
// Compilar con:
// mpicc -o bruteforce_modified bruteforce_modified.c -lcrypto
// 
// Ejecutar con:
// mpirun -np <N procesos> ./bruteforce_modified


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <unistd.h>
#include <openssl/des.h>

void decrypt(long key, char *ciph, int len) {
    DES_cblock des_key;
    DES_key_schedule schedule;
    
    for (int i = 0; i < 8; i++) {
        des_key[i] = (key >> (56 - 8 * i)) & 0xFF;
    }
    
    DES_set_odd_parity(&des_key);
    DES_set_key_checked(&des_key, &schedule);
    
    for (int i = 0; i < len; i += 8) {
        DES_ecb_encrypt((DES_cblock *)(ciph + i), (DES_cblock *)(ciph + i), &schedule, DES_DECRYPT);
    }
}

void encrypt(long key, char *ciph, int len) {
    DES_cblock des_key;
    DES_key_schedule schedule;
    
    for (int i = 0; i < 8; i++) {
        des_key[i] = (key >> (56 - 8 * i)) & 0xFF;
    }
    
    DES_set_odd_parity(&des_key);
    DES_set_key_checked(&des_key, &schedule);
    
    for (int i = 0; i < len; i += 8) {
        DES_ecb_encrypt((DES_cblock *)(ciph + i), (DES_cblock *)(ciph + i), &schedule, DES_ENCRYPT);
    }
}

char search[] = " the ";
int tryKey(long key, char *ciph, int len) {
    char temp[len+1];
    memcpy(temp, ciph, len);
    temp[len] = 0;
    decrypt(key, temp, len);
    return strstr((char *)temp, search) != NULL;
}

unsigned char cipher[] = {108, 245, 65, 63, 125, 200, 150, 66, 17, 170, 207, 170, 34, 31, 70, 215, 0};

int main(int argc, char *argv[]) {
    int N, id;
    long upper = (1L << 56); // upper bound DES keys 2^56
    long mylower, myupper;
    MPI_Status st;
    MPI_Request req;
    int ciphlen = strlen((char*)cipher);
    MPI_Comm comm = MPI_COMM_WORLD;

    MPI_Init(NULL, NULL);
    MPI_Comm_size(comm, &N);
    MPI_Comm_rank(comm, &id);

    printf("Proceso %d de %d iniciado\n", id, N);
    fflush(stdout);

    long range_per_node = upper / N;
    mylower = range_per_node * id;
    myupper = range_per_node * (id+1) - 1;
    if (id == N-1) {
        // compensar residuo
        myupper = upper;
    }

    printf("Proceso %d buscando en el rango [%ld, %ld]\n", id, mylower, myupper);
    fflush(stdout);


    long found = 0;

    MPI_Irecv(&found, 1, MPI_LONG, MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &req);

    for (long i = mylower; i < myupper && (found == 0); ++i) {
        if (tryKey(i, (char *)cipher, ciphlen)) {
            found = i;
            for (int node = 0; node < N; node++) {
                MPI_Send(&found, 1, MPI_LONG, node, 0, MPI_COMM_WORLD);
            }
            break;
        }
    }

    if (id == 0) {
        MPI_Wait(&req, &st);
        decrypt(found, (char *)cipher, ciphlen);
        printf("Clave encontrada: %ld\n", found);
        printf("Mensaje descifrado: %s\n", cipher);
        fflush(stdout);
    }

    MPI_Finalize();
    return 0;
}