#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <openssl/des.h>

#define BLOCK_SIZE 8  // DES usa bloques de 8 bytes
#define SEARCH_PHRASE "es una prueba de"
#define BLOCK_SIZE_SEARCH 1000000  // Tamaño del bloque para asignar dinámicamente

// Funciones de cifrado/descifrado y utilidades (como pkcs5_padding, encrypt, decrypt, tryKey, readFile) se mantienen igual.
// Función para aplicar PKCS5 Padding
int pkcs5_padding(char *text, int len) {
    int pad_len = BLOCK_SIZE - (len % BLOCK_SIZE);
    for (int i = len; i < len + pad_len; i++) {
        text[i] = pad_len;
    }
    return len + pad_len;  // Nuevo tamaño con padding
}

// Función para remover PKCS5 Padding
int pkcs5_unpadding(char *text, int len) {
    int pad_len = text[len - 1];
    return len - pad_len;  // Nuevo tamaño sin padding
}

// Función para leer el contenido de un archivo .txt
void readFile(const char *filename, char **buffer) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error al abrir el archivo");
        exit(EXIT_FAILURE);
    }
    
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    *buffer = (char *)malloc(file_size + BLOCK_SIZE + 1);  // +BLOCK_SIZE para el padding
    fread(*buffer, 1, file_size, file);
    (*buffer)[file_size] = '\0';
    
    fclose(file);
}

// Función para cifrar el texto usando DES
void encrypt(long key, char *ciph, int len) {
    DES_key_schedule ks;
    DES_cblock des_key;
    for (int i = 0; i < 8; ++i) {
        des_key[i] = (key >> (i * 8)) & 0xFF;
    }
    DES_set_odd_parity(&des_key);
    DES_set_key_checked(&des_key, &ks);

    for (int i = 0; i < len; i += BLOCK_SIZE) {
        DES_ecb_encrypt((const_DES_cblock *)(ciph + i), (DES_cblock *)(ciph + i), &ks, DES_ENCRYPT);
    }
}

// Función para descifrar el texto usando DES
void decrypt(long key, char *ciph, int len) {
    DES_key_schedule ks;
    DES_cblock des_key;
    for (int i = 0; i < 8; ++i) {
        des_key[i] = (key >> (i * 8)) & 0xFF;
    }
    DES_set_odd_parity(&des_key);
    DES_set_key_checked(&des_key, &ks);

    for (int i = 0; i < len; i += BLOCK_SIZE) {
        DES_ecb_encrypt((const_DES_cblock *)(ciph + i), (DES_cblock *)(ciph + i), &ks, DES_DECRYPT);
    }
}

// Función para probar una clave en un rango determinado
int tryKey(long key, char *ciph, int len) {
    char temp[len + 1];
    memcpy(temp, ciph, len);
    temp[len] = 0;
    decrypt(key, temp, len);
    return strstr(temp, SEARCH_PHRASE) != NULL;
}

// Función para el proceso maestro
void master_process(long upper, int size) {
    long current_key = 0;
    int active_workers = size - 1;
    MPI_Status status;

    while (active_workers > 0) {
        MPI_Recv(NULL, 0, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        int worker_rank = status.MPI_SOURCE;

        if (current_key > upper) {
            // Enviar señal de terminación al trabajador
            MPI_Send(NULL, 0, MPI_CHAR, worker_rank, 1, MPI_COMM_WORLD);
            active_workers--;
        } else {
            // Asignar un nuevo bloque al trabajador
            long mylower = current_key;
            long myupper = mylower + BLOCK_SIZE_SEARCH - 1;
            if (myupper > upper) {
                myupper = upper;
            }
            current_key = myupper + 1;

            long range[2] = {mylower, myupper};
            MPI_Send(range, 2, MPI_LONG, worker_rank, 0, MPI_COMM_WORLD);
        }
    }
}

// Función para el proceso trabajador
void worker_process(int rank, char *text, int text_len, double start_time) {
    long range[2];
    MPI_Status status;

    while (1) {
        // Solicitar trabajo al maestro
        MPI_Send(NULL, 0, MPI_CHAR, 0, 0, MPI_COMM_WORLD);

        // Recibir rango de claves para probar
        MPI_Recv(range, 2, MPI_LONG, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if (status.MPI_TAG == 1) {
            // Señal de terminación
            break;
        }

        long mylower = range[0];
        long myupper = range[1];
        int found = 0;

        // Probar claves en el rango asignado sin OpenMP, en un solo hilo
        for (long key = mylower; key <= myupper; key++) {
            if (found) {
                continue;
            }

            if (tryKey(key, text, text_len)) {
                found = 1;
                double end_time = MPI_Wtime();
                printf("Proceso %d encontró la clave: %ld\n", rank, key);
                printf("Clave encontrada: %ld en %f segundos\n", key, end_time - start_time);
                MPI_Abort(MPI_COMM_WORLD, 0);  // Finalizar todos los procesos cuando se encuentra la clave
            }
        }
    }
}

int main(int argc, char *argv[]) {
    int rank, size;
    long upper = (1L << 56);  // Definimos un rango grande para las llaves (56 bits en este caso)
    char *text = NULL;
    int text_len;

    // Comprobar si se pasaron los argumentos necesarios
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <archivo> <clave_correcta>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Obtener la clave correcta desde los argumentos
    long key_to_find = atol(argv[2]);

    // Inicializar MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Medir el tiempo de ejecución
    double start_time = MPI_Wtime();

    if (rank == 0) {
        // Proceso maestro lee el archivo y lo distribuye
        readFile(argv[1], &text);  // Leer el archivo de texto desde los argumentos
        text_len = strlen(text);

        // Aplicar padding
        text_len = pkcs5_padding(text, text_len);
        // Cifrar el texto usando la clave correcta
        encrypt(key_to_find, text, text_len);

        // Difundir el tamaño del texto y el texto cifrado a todos los procesos
        MPI_Bcast(&text_len, 1, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(text, text_len, MPI_CHAR, 0, MPI_COMM_WORLD);

        // Ejecutar la función del proceso maestro
        master_process(upper, size);
    } else {
        // Recibir el tamaño del texto y el texto cifrado
        MPI_Bcast(&text_len, 1, MPI_INT, 0, MPI_COMM_WORLD);
        text = (char *)malloc(text_len + 1);
        MPI_Bcast(text, text_len, MPI_CHAR, 0, MPI_COMM_WORLD);

        // Ejecutar la función del proceso trabajador
        worker_process(rank, text, text_len, start_time);
    }

    // Medir el tiempo de ejecución final
    double end_time = MPI_Wtime();
    if (rank == 0) {
        printf("Tiempo total de ejecución: %f segundos\n", end_time - start_time);
    }

    // Liberar memoria
    free(text);

    // Finalizar MPI
    MPI_Finalize();
    return 0;
}
