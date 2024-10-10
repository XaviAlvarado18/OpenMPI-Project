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
// mpicc -o bruteforce_m_sm bruteforce_m_sm.c -lcrypto
// 
// Ejecutar con:
// mpirun -np <N procesos> ./bruteforce_m_sm


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <unistd.h>
#include <openssl/des.h>

// Función para descifrar el texto usando DES
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

// Función para cifrar el texto usando DES
void encrypt(long key, char *plain, char *ciph, int len) {
    DES_cblock des_key;
    DES_key_schedule schedule;
    
    for (int i = 0; i < 8; i++) {
        des_key[i] = (key >> (56 - 8 * i)) & 0xFF;
    }
    
    DES_set_odd_parity(&des_key);
    DES_set_key_checked(&des_key, &schedule);
    
    for (int i = 0; i < len; i += 8) {
        DES_ecb_encrypt((DES_cblock *)(plain + i), (DES_cblock *)(ciph + i), &schedule, DES_ENCRYPT);
    }
}

// Función para probar una clave en un rango determinado

char search[] = " the ";
int tryKey(long key, char *ciph, int len) {
    char temp[len+1];
    memcpy(temp, ciph, len);
    temp[len] = 0;
    decrypt(key, temp, len);
    return strstr((char *)temp, search) != NULL;
}

int main(int argc, char *argv[]) {
    int N, id;
    long upper = 10000;
    long mylower, myupper;
    MPI_Status st;
    MPI_Request req;
    MPI_Comm comm = MPI_COMM_WORLD;

    // Proabr mensaje y llave
    char plaintext[] = "Hello the World!"; 
    long test_key = 2222; // Simple test key
    int ciphlen = ((strlen(plaintext) + 7) / 8) * 8;
    char ciphertext[ciphlen];

    MPI_Init(NULL, NULL);
    MPI_Comm_size(comm, &N);
    MPI_Comm_rank(comm, &id);

    if (id == 0) {
        printf("Original message: %s\n", plaintext);
        printf("Test key: %ld\n", test_key);
        encrypt(test_key, plaintext, ciphertext, ciphlen);
        printf("Encrypted message: ");
        for (int i = 0; i < ciphlen; i++) {
            printf("%02x", (unsigned char)ciphertext[i]);
        }
        printf("\n");
    }

    MPI_Bcast(ciphertext, ciphlen, MPI_CHAR, 0, comm);

    long range_per_node = upper / N;
    mylower = range_per_node * id;
    myupper = range_per_node * (id+1) - 1;
    if (id == N-1) {
        myupper = upper;
    }

    printf("Proceso %d de %d buscando rango [%ld, %ld]\n", id, N, mylower, myupper);
    fflush(stdout);

    long found = 0;

    MPI_Irecv(&found, 1, MPI_LONG, MPI_ANY_SOURCE, MPI_ANY_TAG, comm, &req);

    for (long i = mylower; i <= myupper && (found == 0); ++i) {
        if (tryKey(i, ciphertext, ciphlen)) {
            found = i;
            for (int node = 0; node < N; node++) {
                MPI_Send(&found, 1, MPI_LONG, node, 0, MPI_COMM_WORLD);
            }
            break;
        }
    }

    if (id == 0) {
        MPI_Wait(&req, &st);
        decrypt(found, ciphertext, ciphlen);
        ciphertext[ciphlen] = '\0';
        printf("Key found: %ld\n", found);
        printf("Decrypted message: %s\n", ciphertext);
    }

    // Finalizar MPI
    MPI_Finalize();
    return 0;
}