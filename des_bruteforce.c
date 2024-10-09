#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/provider.h>  // Para manejar proveedores
#include <time.h>

// Tamaño del bloque para DES
#define TEXT_SIZE 8

// Número de bytes variables en la clave
#define VAR_KEY_BYTES 2

// Función para medir el tiempo
double get_time_in_seconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

// Función para generar llaves con una parte fija y una parte variable
void generate_key(unsigned char* key, unsigned long long counter, int var_key_bytes) {
    // Parte fija de la clave (primeros 8 - var_key_bytes)
    // Puedes cambiar estos valores según prefieras
    for(int i = 0; i < 8 - var_key_bytes; i++) {
        key[i] = 0x01;  // Valor fijo, por ejemplo, 0x01
    }

    // Parte variable de la clave (últimos var_key_bytes)
    for(int i = 0; i < var_key_bytes; i++) {
        key[8 - var_key_bytes + i] = (counter >> (8 * i)) & 0xFF;
    }
}

// Función para cifrar con DES en modo ECB usando EVP
int des_encrypt(const unsigned char* plaintext, unsigned char* ciphertext, const unsigned char* key) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        fprintf(stderr, "Error creando el contexto de cifrado.\n");
        ERR_print_errors_fp(stderr);
        return 0;
    }

    if (1 != EVP_EncryptInit_ex(ctx, EVP_des_ecb(), NULL, key, NULL)) {
        fprintf(stderr, "Error inicializando el cifrado.\n");
        ERR_print_errors_fp(stderr);
        EVP_CIPHER_CTX_free(ctx);
        return 0;
    }

    // Desactivar el padding
    if (1 != EVP_CIPHER_CTX_set_padding(ctx, 0)) {
        fprintf(stderr, "Error al desactivar el padding.\n");
        ERR_print_errors_fp(stderr);
        EVP_CIPHER_CTX_free(ctx);
        return 0;
    }

    int len;
    int ciphertext_len = 0;

    if (1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, TEXT_SIZE)) {
        fprintf(stderr, "Error durante EVP_EncryptUpdate.\n");
        ERR_print_errors_fp(stderr);
        EVP_CIPHER_CTX_free(ctx);
        return 0;
    }
    ciphertext_len += len;

    if (1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len)) {
        fprintf(stderr, "Error durante EVP_EncryptFinal_ex.\n");
        ERR_print_errors_fp(stderr);
        EVP_CIPHER_CTX_free(ctx);
        return 0;
    }
    ciphertext_len += len;

    EVP_CIPHER_CTX_free(ctx);
    return ciphertext_len;
}

// Función para descifrar con DES en modo ECB usando EVP
int des_decrypt(const unsigned char* ciphertext, unsigned char* decrypted, const unsigned char* key) {
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        fprintf(stderr, "Error creando el contexto de descifrado.\n");
        ERR_print_errors_fp(stderr);
        return 0;
    }

    if (1 != EVP_DecryptInit_ex(ctx, EVP_des_ecb(), NULL, key, NULL)) {
        fprintf(stderr, "Error inicializando el descifrado.\n");
        ERR_print_errors_fp(stderr);
        EVP_CIPHER_CTX_free(ctx);
        return 0;
    }

    // Desactivar el padding
    if (1 != EVP_CIPHER_CTX_set_padding(ctx, 0)) {
        fprintf(stderr, "Error al desactivar el padding.\n");
        ERR_print_errors_fp(stderr);
        EVP_CIPHER_CTX_free(ctx);
        return 0;
    }

    int len;
    int decrypted_len = 0;

    if (1 != EVP_DecryptUpdate(ctx, decrypted, &len, ciphertext, TEXT_SIZE)) {
        // No imprimir error aquí para evitar confusión si la clave es incorrecta
        EVP_CIPHER_CTX_free(ctx);
        return 0;
    }
    decrypted_len += len;

    if (1 != EVP_DecryptFinal_ex(ctx, decrypted + len, &len)) {
        // No imprimir error aquí para evitar confusión si la clave es incorrecta
        EVP_CIPHER_CTX_free(ctx);
        return 0;
    }
    decrypted_len += len;

    EVP_CIPHER_CTX_free(ctx);
    return decrypted_len;
}

// Fuerza bruta para encontrar la llave
int brute_force_des(const unsigned char* ciphertext, const unsigned char* original_text, int var_key_bytes) {
    unsigned char decrypted[TEXT_SIZE];
    unsigned char key[8];

    unsigned long long total_keys = 1;
    for(int i = 0; i < var_key_bytes; i++) {
        total_keys *= 256;
    }

    for (unsigned long long i = 0; i < total_keys; ++i) {
        generate_key(key, i, var_key_bytes);

        if (des_decrypt(ciphertext, decrypted, key)) {
            if (memcmp(decrypted, original_text, TEXT_SIZE) == 0) {
                printf("Llave encontrada: ");
                for (int j = 0; j < 8; ++j) {
                    printf("%02x", key[j]);
                }
                printf("\n");
                return 1;  // Llave encontrada
            }
        }

        // Opcional: Mostrar progreso cada cierto número de intentos
        /*
        if (i % 10000 == 0) {
            printf("Progreso: %llu/%llu\n", i, total_keys);
        }
        */
    }
    return 0;  // Llave no encontrada
}

int main() {
    // Cargar los proveedores de OpenSSL
    OSSL_PROVIDER *default_prov = OSSL_PROVIDER_load(NULL, "default");
    if (default_prov == NULL) {
        fprintf(stderr, "Error cargando el proveedor default.\n");
        ERR_print_errors_fp(stderr);
        return 1;
    }

    OSSL_PROVIDER *legacy_prov = OSSL_PROVIDER_load(NULL, "legacy");
    if (legacy_prov == NULL) {
        fprintf(stderr, "Error cargando el proveedor legacy.\n");
        ERR_print_errors_fp(stderr);
        // Liberar el proveedor default antes de salir
        OSSL_PROVIDER_unload(default_prov);
        return 1;
    }

    // Inicializar OpenSSL
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();

    // Texto original y cifrado
    const unsigned char original_text[TEXT_SIZE] = "Texto123";  // Debe ser de 8 bytes para DES
    unsigned char ciphertext[TEXT_SIZE];  // Sin padding, 8 bytes son suficientes

    // Definir una llave secreta conocida con una parte fija y una parte variable
    // Para este ejemplo, los primeros 6 bytes son fijos y los últimos 2 bytes variarán
    unsigned char secret_key[8] = {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x03};  // Clave conocida

    // Cifrar el texto original
    if (des_encrypt(original_text, ciphertext, secret_key) == 0) {
        fprintf(stderr, "Error cifrando el texto.\n");
        // Liberar los proveedores antes de salir
        OSSL_PROVIDER_unload(legacy_prov);
        OSSL_PROVIDER_unload(default_prov);
        return 1;
    }

    // Mostrar el texto cifrado
    printf("Texto cifrado: ");
    for (int i = 0; i < TEXT_SIZE; ++i) {
        printf("%02x", ciphertext[i]);
    }
    printf("\n");

    // Verificación adicional
    unsigned char decrypted_verify[TEXT_SIZE];
    if (des_decrypt(ciphertext, decrypted_verify, secret_key) == 0) {
        fprintf(stderr, "Error descifrando para verificación.\n");
        // Liberar los proveedores antes de salir
        OSSL_PROVIDER_unload(legacy_prov);
        OSSL_PROVIDER_unload(default_prov);
        return 1;
    }

    if (memcmp(decrypted_verify, original_text, TEXT_SIZE) == 0) {
        printf("Verificación exitosa: El texto descifrado coincide con el original.\n");
    } else {
        printf("Verificación fallida: El texto descifrado NO coincide con el original.\n");
    }

    // Longitudes de llaves a probar (Número de bytes variables)
    int var_key_bytes_list[] = {1, 2};  // Prueba con 1 y 2 bytes variables
    int num_lengths = sizeof(var_key_bytes_list) / sizeof(var_key_bytes_list[0]);

    // Medir el tiempo para cada número de bytes variables en la llave
    for (int i = 0; i < num_lengths; ++i) {
        int var_key_bytes = var_key_bytes_list[i];
        printf("\nProbando con %d bytes variables en la llave\n", var_key_bytes);

        double start_time = get_time_in_seconds();
        int found = brute_force_des(ciphertext, original_text, var_key_bytes);
        double end_time = get_time_in_seconds();

        if (found) {
            printf("Tiempo de búsqueda con %d bytes variables: %.2f segundos\n", var_key_bytes, end_time - start_time);
        } else {
            printf("Llave no encontrada con %d bytes variables\n", var_key_bytes);
        }
    }

    // Limpiar OpenSSL y liberar los proveedores
    EVP_cleanup();
    ERR_free_strings();
    OSSL_PROVIDER_unload(legacy_prov);
    OSSL_PROVIDER_unload(default_prov);

    return 0;
}
