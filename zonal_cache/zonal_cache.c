#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdint.h>
#include <time.h>

#include "../comun/http_cliente.h"
#include "../comun/variables_entorno.h"

char *leer_dat(char *dir_punto_dat, size_t *cantidad_leidos) {
    FILE *f = fopen(dir_punto_meta, "r");
    if (f == NULL) {
        fclose(f);
        return -1;
    }
    // Obtener el tamanno del archivo
    struct stat st;
    stat(dir_punto_dat, &st);
    // Leer el archivo completo
    static char archivo[st.st_size];
    cantidad_leidos = fread(archivo, sizeof(char), sizeof(archivo), f);
    return archivo;
}

int actualizar_meta(char *dir_punto_meta) {
    FILE *f = fopen(dir_punto_meta, "r");
    if (f == NULL) {
        fclose(f);
        return -1;
    }
    // Leer el archivo completo
    char archivo[1024];
    fread(archivo, sizeof(char), sizeof(archivo), f);
    // Leer la cantidad de accesos actual
    char *puntero_cantidad_accesos = strstr(archivo, "cantidad_accesos: ");
    puntero_cantidad_accesos += 18;
    char* endptr;
    long cantidad_accesos = strtol(puntero_cantidad_accesos, &endptr, 10);
    // Incrementar la cantidad de accesos y obtener el momento actual
    cantidad_accesos++;
    time_t momento_actual = time(NULL);
    // Actualizar los datos del archivo
    char *puntero_ultimo_acceso = strstr(archivo, "ultimo_acceso: ")
    puntero_ultimo_acceso += 15; // Aca hay que escribir el nuevo valor de ultimo_acceso
    char archivo_actualizado[1024];
    for (int i = 0; archivo[i] != '\0'; i++) {
        if (archivo + i == puntero_ultimo_acceso) {
            i += snprintf(archivo_actualizado + i, sizeof(archivo_actualizado), "%d", momento_actual);
        } else if (archivo + i == puntero_cantidad_accesos) {
            i += snprintf(archivo_actualizado + i, sizeof(archivo_actualizado), "%d", cantidad_accesos);
        }
        archivo_actualizado[i] = archivo[i];
    }
    // Ya tenemos los datos del archivo actualizado en el buffer archivo_actualizado
    fclose(f);
    // Escribir los datos
    FILE *fw = fopen(dir_punto_meta, "w");
    if (fw == NULL) {
        fclose(fw);
        return -1;
    }
    fwrite(archivo_actualizado, sizeof(char), strlen(archivo_actualizado), fw);
    fclose(fw);
    return 0;
}

int expirado(char *dir_punto_meta) {
    FILE *f = fopen(dir_punto_meta, "r");
    if (f == NULL) {
        return -1;
    }
    // Leer el momento de expiracion
    char archivo[1024];
    fread(archivo, sizeof(char), sizeof(archivo), f);
    char *puntero_momento_expiracion = strstr(archivo, "momento_expiracion: ");
    puntero_momento_expiracion += 20;
    char* endptr;
    time_t momento_expiracion = strtol(puntero_momento_expiracion, &endptr, 10);
    // Comparar con el momento actual
    time_t momento_actual = time(NULL);
    if (momento_actual < momento_expiracion) {
        fclose(f);
        return 0;
    } else {
        fclose(f);
        return 1;
    }
}

static uint64_t fnv1a(const char *s) {
    uint64_t h = 1469598103934665603ULL;
    while (*s) {
        h ^= (unsigned char)*s++;
        h *= 1099511628211ULL;
    }
    return h;
}

char *procesar_entrada(char* buffer_entrada, int env_puerto_server_origen, size_t *largo_salida) {
    // Extraer el metodo
    // Se coloca en el primer ' ' para obtener el metodo y luego obtener el parametro de la peticion
    char *puntero_fin_buscado = strchr(buffer_entrada, ' ');
    size_t cant_caracteres = puntero_fin_buscado - buffer_entrada;
    
    char *metodo = malloc(cant_caracteres + 1); // + 1 para agregar el '\0'
    memcpy(metodo, buffer_entrada, cant_caracteres);
    metodo[cant_caracteres] = '\0';
    printf("Metodo: %s\n", metodo);

    // Extraer la ruta
    char *puntero_fin_buscado_anterior = puntero_fin_buscado;
    puntero_fin_buscado_anterior++; // Apuntar al '/'
    puntero_fin_buscado = strchr(puntero_fin_buscado_anterior, ' ');
    cant_caracteres = puntero_fin_buscado - puntero_fin_buscado_anterior;
    
    char *ruta = malloc(cant_caracteres + 1);
    memcpy(ruta, puntero_fin_buscado_anterior, cant_caracteres);
    ruta[cant_caracteres] = '\0';
    printf("Ruta: %s\n", ruta);

    // Extraer el host
    char *puntero_inicio_host = strstr(buffer_entrada, "Host: ");
    puntero_inicio_host += 6; // Saltar "Host: "
    puntero_fin_buscado = strchr(puntero_inicio_host, '\r');
    cant_caracteres = puntero_fin_buscado - puntero_inicio_host;
    char *host = malloc(cant_caracteres + 1);
    int i = 0;
    while (i <= cant_caracteres && puntero_inicio_host[i] != ':') {
        host[i] = puntero_inicio_host[i];
        i++;
    }
    host[i] = '\0';
    printf("Host: %s\n", host);

    // Hashear host + ruta
    char clave[strlen(host) + strlen(ruta) + 1];
    snprintf(clave, sizeof(clave), "%s%s", host, ruta);

    char hash_clave[16];
    snprintf(hash_clave, sizeof(hash_clave), "%016lx", fnv1a(clave));

    // Verificar si se tiene informacion en cache del host correspondiente
    struct stat st = {0};
    
    char dir_directorio[6 + cant_caracteres]; // "cache/" -> 6 caracteres
    snprintf(dir_directorio, sizeof(dir_directorio), "%s%s", "cache/", host);

    if (stat(dir_directorio, &st)) {
        // Formar la direccion de los .meta y .dat
        char dir_punto_meta[sizeof(dir_directorio) + 1 + sizeof(hash_clave) + 5]; // +1 por el '/' entre el directorio y el hash. +5 por el ".meta"
        char dir_punto_dat[sizeof(dir_directorio) + 1 + sizeof(hash_clave) + 4]; // +4 por el ".dat"
        snprintf(dir_punto_meta + sizeof(dir_punto_meta) - 4, sizeof(dir_punto_meta), "%s", ".meta");
        snprintf(dir_punto_dat + sizeof(dir_punto_dat) - 3, sizeof(dir_punto_dat), "%s", ".dat");
        
        // Verificar si la carpeta del dominio ya esta creada
        if (!stat(dir_punto_meta, &st)) {
            // Verificar si ya expiro
            int esta_expirado = expirado(dir_punto_meta);
            if (esta_expirado == 0) {
                // Actualizar metadatos
                if (actualizar_meta(dir_punto_meta) == -1) {
                    perror("Error al intentar actualizar los valores del .meta");
                    return NULL;
                }
                // Leer .dat
                char *respuesta = leer_dat(dir_punto_dat, largo_salida);
                // Liberar memoria y retornar la respuesta al usuario
                free(metodo);
                free(ruta);
                free(host);
                return respuesta_origen;
            } else if (esta_expirado == -1) {
                perror("Error al intentar leer el .meta");
                return NULL;
            } // Si esta_expirado == 1, entonces ya expiro
        } else {
            perror("Error al intentar ubicar el .meta");
            return NULL;
        }
    } else {
        // Si no esta creada se crea, mas abajo se trae la informacion del dominio
        if (mkdir(ruta, 0777) != 0) {
            perror("Error al crear el directorio");
            return NULL;
        }
    }
    
    // Consultar informacion al servidor origen
    char buffer_from_int_to_char[20];
    snprintf(buffer_from_int_to_char, sizeof(buffer_from_int_to_char), "%d", env_puerto_server_origen);
    char *respuesta_origen = http_get(host, buffer_from_int_to_char, ruta, largo_salida, 1);
    if (respuesta_origen == NULL) {
        printf("No se puede recuperar información del origen.");
        free(metodo);
        free(ruta);
        free(host);
        return NULL;
    }

    // Guardar .dat y .meta

    printf("Salida Apache Server: %.*s", (int)*largo_salida, respuesta_origen);
    
    free(metodo);
    free(ruta);
    free(host);

    return respuesta_origen;
}


int main(void) {
    // Leer variables de entorno
    int env_max_tam_entrada = env_entero("MAX_TAM_ENTRADA", 1024);
    int env_puerto_zonal_cache = env_entero("ZONAL_PUERTO_IN", 9777);
    int env_puerto_server_origen = env_entero("ORIGEN_PUERTO", 80);


    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server_address, client_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(env_puerto_zonal_cache);

    // Permite reusar el puerto aunque haya quedado en TIME_WAIT de una corrida anterior
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_socket);
        return 1;
    }

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        fprintf(stderr, "bind al puerto %d: %s\n", env_puerto_zonal_cache, strerror(errno));
        close(server_socket);
        return 1;
    }

    if (listen(server_socket, 10) < 0) {
        perror("listen");
        close(server_socket);
        return 1;
    }
    printf("Zonal Cache escuchando en el puerto %d\n", env_puerto_zonal_cache);

    ssize_t bytes_leidos;
    while (true) {
        printf("\nEscuchando conexiones...\n");
        socklen_t client_size = sizeof(client_address);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_size);
        printf("Cliente conectado, IP: %s, puerto: %i\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

        char entrada[env_max_tam_entrada];
        while ((bytes_leidos = read(client_socket, entrada, sizeof(entrada))) > 0) {
            printf("Entrada: %s", entrada);
            size_t largo_salida = 0;
            char *respuesta = procesar_entrada(entrada, env_puerto_server_origen, &largo_salida);
            if (respuesta == NULL) continue;
            send(client_socket, respuesta, largo_salida, MSG_NOSIGNAL);
            free(respuesta);
        }

        close(client_socket);
    }

    close(server_socket);

    return 0;
}