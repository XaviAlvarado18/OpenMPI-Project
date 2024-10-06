#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <openssl/des.h>
#include <cuda_runtime.h>

#define BLOCK_SIZE 8  // DES usa bloques de 8 bytes
#define SEARCH_PHRASE "es una prueba de"

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

// Función para descifrar el texto usando DES en la CPU
void decrypt_cpu(long key, char *ciph, int len) {
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

// Función CUDA para probar una clave en la GPU
__global__ void tryKeyCUDA(long start_key, long range, char *ciph, int len, int *found_key, long *found_val) {
    long idx = blockIdx.x * blockDim.x + threadIdx.x;
    long key = start_key + idx;

    if (idx < range && !(*found_key)) {
        // Probar la clave actual
        // Solo realiza la búsqueda, el descifrado lo hará la CPU
        *found_val = key;
        *found_key = 1;  // Marcar que hemos encontrado una clave para probar en la CPU
    }
}

// Función para probar claves en la CPU
int tryKeyCPU(long key, char *ciph, int len) {
    char temp[len + 1];
    memcpy(temp, ciph, len);
    temp[len] = 0;

    decrypt_cpu(key, temp, len);
    return strstr(temp, SEARCH_PHRASE) != NULL;
}

int main(int argc, char *argv[]) {
    int rank, size;
    long upper = 18014398509481983L;  // (2^56) / 4, el espacio total de búsqueda
    long mylower, myupper;
    int found = 0;
    char *text = NULL;
    int text_len;
    long found_key = 0;

    // Inicializar MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Medir el tiempo de ejecución
    double start_time = MPI_Wtime();

    if (rank == 0) {
        readFile("ex1.txt", &text);
        text_len = strlen(text);

        // Aplicar padding
        text_len = pkcs5_padding(text, text_len);
    }

    // Enviar el tamaño del texto a todos los procesos
    MPI_Bcast(&text_len, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Distribuir el texto cifrado a todos los procesos
    if (rank != 0) {
        text = (char *)malloc(text_len + 1);
    }
    MPI_Bcast(text, text_len, MPI_CHAR, 0, MPI_COMM_WORLD);

    // Definir el rango de claves para cada proceso
    long range_per_process = upper / size;
    mylower = range_per_process * rank;
    myupper = range_per_process * (rank + 1) - 1;
    if (rank == size - 1) {
        myupper = upper;  // Asegurarse que el último proceso cubra todo el rango
    }

    // Copiar texto cifrado a memoria de la GPU
    int *d_found_key;
    long *d_found_val;
    cudaMalloc(&d_found_key, sizeof(int));
    cudaMalloc(&d_found_val, sizeof(long));
    cudaMemcpy(d_found_key, &found, sizeof(int), cudaMemcpyHostToDevice);

    // Ejecutar el kernel CUDA
    int threads_per_block = 256;
    long total_keys = myupper - mylower;
    int num_blocks = (total_keys + threads_per_block - 1) / threads_per_block;

    tryKeyCUDA<<<num_blocks, threads_per_block>>>(mylower, total_keys, text, text_len, d_found_key, d_found_val);

    // Copiar resultados desde la GPU
    cudaMemcpy(&found, d_found_key, sizeof(int), cudaMemcpyDeviceToHost);
    cudaMemcpy(&found_key, d_found_val, sizeof(long), cudaMemcpyDeviceToHost);

    // Si encontramos una clave en la GPU, probarla en la CPU
    if (found) {
        if (tryKeyCPU(found_key, text, text_len)) {
            printf("Proceso %d encontró la clave: %ld\n", rank, found_key);
            MPI_Abort(MPI_COMM_WORLD, 0);  // Finalizar todos los procesos cuando se encuentra la clave
        }
    }

    // Medir el tiempo de ejecución final
    double end_time = MPI_Wtime();

    if (rank == 0) {
        printf("Tiempo total de ejecución: %f segundos\n", end_time - start_time);
    }

    // Liberar memoria
    free(text);
    cudaFree(d_found_key);
    cudaFree(d_found_val);

    // Finalizar MPI
    MPI_Finalize();
    return 0;
}
