#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <openssl/des.h>

#define BLOCK_SIZE 8  // DES usa bloques de 8 bytes

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

int main(int argc, char *argv[]) {
    int rank, size;
    long key;
    char *text = NULL;
    int text_len;

    // Iniciar MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc != 3) {
        if (rank == 0) {
            printf("Uso: %s <archivo.txt> <clave>\n", argv[0]);
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    key = atol(argv[2]);

    // El proceso maestro (rank 0) lee el archivo y aplica padding
    if (rank == 0) {
        readFile(argv[1], &text);
        text_len = strlen(text);

        // Aplicar padding antes de cifrar
        text_len = pkcs5_padding(text, text_len);
    }

    // Enviar el tamaño del texto a todos los procesos
    MPI_Bcast(&text_len, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Calcular la cantidad de texto por proceso
    int chunk_size = text_len / size;

    // Crear un buffer local para cada proceso
    char *local_text = (char *)malloc(chunk_size + 1);

    // Dividir el texto entre los procesos
    MPI_Scatter(text, chunk_size, MPI_CHAR, local_text, chunk_size, MPI_CHAR, 0, MPI_COMM_WORLD);

    // Cifrar la parte local del texto
    encrypt(key, local_text, chunk_size);

    // Reunir los textos cifrados en el proceso maestro
    MPI_Gather(local_text, chunk_size, MPI_CHAR, text, chunk_size, MPI_CHAR, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("Texto cifrado en hex: ");
        for (int i = 0; i < text_len; i++) {
            printf("%02x", (unsigned char)text[i]);  // Mostrar en formato hexadecimal
        }
        printf("\n");
    }

    // Descifrar la parte local del texto
    decrypt(key, local_text, chunk_size);

    // Reunir los textos descifrados en el proceso maestro
    MPI_Gather(local_text, chunk_size, MPI_CHAR, text, chunk_size, MPI_CHAR, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        // Remover padding después de descifrar
        text_len = pkcs5_unpadding(text, text_len);
        text[text_len] = '\0';

        printf("Texto descifrado: %s\n", text);
    }

    // Liberar memoria
    free(local_text);
    if (rank == 0) {
        free(text);
    }

    // Finalizar MPI
    MPI_Finalize();
    return EXIT_SUCCESS;
}
