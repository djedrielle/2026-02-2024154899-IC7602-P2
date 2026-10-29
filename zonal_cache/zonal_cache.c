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

typedef struct {
    char clave[256];
    long tamanno;
    time_t insercion;
    time_t ultimo_acceso;
    long cantidad_accesos;
    time_t expiracion;
} Meta;

int leer_meta(char *dir_punto_meta, Meta *salida) {
    FILE *f = fopen(dir_punto_meta, "r");
    if (f == NULL) {
        return -1;
    }

    // Leer los datos del archivo y construir un objeto Meta con estos datos
    char clave[256];
    long tamanno;
    long insercion;
    long ultimo_acceso;
    long cantidad_accesos;
    long expiracion;

    if (fscanf(f, "%*s %255s %*s %ld %*s %ld %*s %ld %*s %ld %*s %ld", clave, &tamanno, &insercion, &ultimo_acceso, &cantidad_accesos, &expiracion) != 6) {
        fclose(f);
        fprintf(stderr, "Formato invalido en el .meta %s\n", dir_punto_meta);
        return -1;
    }

    snprintf(salida->clave, sizeof(salida->clave), "%s", clave);
    salida->tamanno = tamanno;
    salida->insercion = insercion;
    salida->ultimo_acceso = ultimo_acceso;
    salida->cantidad_accesos = cantidad_accesos;
    salida->expiracion = expiracion;

    fclose(f);
    return 0;
}

char *leer_dat(char *dir_punto_dat, size_t *cantidad_leidos) {
    FILE *f = fopen(dir_punto_dat, "r");
    if (f == NULL) return NULL;
    // Obtener el tamanno del archivo
    struct stat st;
    if (stat(dir_punto_dat, &st)) {
        fclose(f);
        perror("Error intentando obtener el tamanno del archivo .dat");
        return NULL;
    }
    // Leer el archivo completo
    char *archivo = malloc(st.st_size);
    if (archivo == NULL) {
        fclose(f);
        return NULL;
    }
    *cantidad_leidos = fread(archivo, sizeof(char), st.st_size, f);
    fclose(f);
    return archivo;
}

int escribir_dat(char *dir_punto_dat, char *respuesta_origen, size_t *largo_salida) {
    FILE *f = fopen(dir_punto_dat, "wb");
    if (f == NULL) return -1;
    int escritos = fwrite(respuesta_origen, sizeof(char), *largo_salida, f);
    return escritos;
}

int actualizar_meta(char *dir_punto_meta) {
    Meta m;
    if (leer_meta(dir_punto_meta, &m) == -1) {
        return -1;
    }

    // Actualizar los datos
    m.ultimo_acceso = time(NULL);
    m.cantidad_accesos += 1;

    // Escribir los datos
    FILE *fw = fopen(dir_punto_meta, "w");
    if (fw == NULL) {
        return -1;
    }
    fprintf(fw, "clave_original: %s\ntamanno_bytes: %ld\nmomento_insercion: %ld\nultimo_acceso: %ld\ncantidad_accesos: %ld\nmomento_expiracion: %ld", 
        m.clave,
        m.tamanno,
        m.insercion,
        m.ultimo_acceso,
        m.cantidad_accesos,
        m.expiracion
    );
    fclose(fw);
    return 0;
}

int escribir_meta(char *dir_punto_meta, char *clave, long tam_bytes, long TTL) {
    char clave[256];
    long tamanno;
    long insercion;
    long ultimo_acceso;
    long cantidad_accesos;
    long expiracion;

    Meta m;
    snprintf(m.clave, sizeof(m.clave), "%s", clave);
    m.tamanno = tam_bytes;
    m.insercion = time(NULL);
    m.ultimo_acceso = time(NULL);
    m.cantidad_accesos = 1;
    m.expiracion = time(NULL) + TTL;

    FILE *fw = fopen(dir_punto_meta, "w");
    if (fw == NULL) {
        return -1;
    }
    fprintf(fw, "clave_original: %s\ntamanno_bytes: %ld\nmomento_insercion: %ld\nultimo_acceso: %ld\ncantidad_accesos: %ld\nmomento_expiracion: %ld", 
        m.clave,
        m.tamanno,
        m.insercion,
        m.ultimo_acceso,
        m.cantidad_accesos,
        m.expiracion
    );
    fclose(fw);
    return 0;
}

int expirado(char *dir_punto_meta) {
    Meta m;
    if (leer_meta(dir_punto_meta, &m) == -1) {
        return -1;
    }

    // Comparar con el momento actual
    time_t momento_actual = time(NULL);
    if (momento_actual < m.expiracion) {
        return 0;
    } else {
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
    size_t i = 0;
    while (i < cant_caracteres && puntero_inicio_host[i] != ':') {
        host[i] = puntero_inicio_host[i];
        i++;
    }
    host[i] = '\0';
    printf("Host: %s\n", host);

    // Hashear host + ruta
    char clave[strlen(host) + strlen(ruta) + 1];
    snprintf(clave, sizeof(clave), "%s%s", host, ruta);

    char hash_clave[16 + 1];
    snprintf(hash_clave, sizeof(hash_clave), "%016lx", fnv1a(clave));

    // Verificar si se tiene informacion en cache del host correspondiente
    struct stat st = {0};
    
    char dir_directorio[6 + strlen(host) + 1]; // "cache/" -> 6 caracteres
    snprintf(dir_directorio, sizeof(dir_directorio), "%s%s", "cache/", host);

    // Formar la direccion de los .meta y .dat
    char dir_punto_meta[sizeof(dir_directorio) + 1 + sizeof(hash_clave) + 5]; // +1 por el '/' entre el directorio y el hash. +5 por el ".meta"
    char dir_punto_dat[sizeof(dir_directorio) + 1 + sizeof(hash_clave) + 4]; // +4 por el ".dat"
    snprintf(dir_punto_meta, sizeof(dir_punto_meta), "%s/%s.meta", dir_directorio, hash_clave);
    snprintf(dir_punto_dat, sizeof(dir_punto_dat), "%s/%s.dat", dir_directorio, hash_clave);

    // Verificar si el directorio del dominio, si la carpeta .meta existe y si la informacion no ha expirado
    int esta_expirado;
    if (!stat(dir_directorio, &st) && !stat(dir_punto_meta, &st) && (esta_expirado = expirado(dir_punto_meta)) == 0) {
        // Si es un hit no se necesitara mas estos datos
        free(metodo);
        free(ruta);
        free(host);

        if (esta_expirado == -1) {
            fprintf(stderr, "Error al intentar leer el .meta %s\n", dir_punto_meta);
            return NULL;
        }

        // Actualizar metadatos
        if (actualizar_meta(dir_punto_meta) == -1) {
            fprintf(stderr, "Error al intentar actualizar los valores del .meta %s\n", dir_punto_meta);
            return NULL;
        }

        // Leer .dat
        char *respuesta = leer_dat(dir_punto_dat, largo_salida);

        if (respuesta == NULL) {
            fprintf(stderr, "Error al intentar recuperar la respuesta del cache %s\n", dir_punto_dat);
            return NULL;
        }
        // Retornar la respuesta al usuario
        return respuesta;
    
    } else {
        // Si no esta creada se crea, mas abajo se trae la informacion del dominio
        if (!stat(dir_directorio, &st)) {
            if (mkdir(dir_directorio, 0777) != 0) {
                perror("Error al crear el directorio");
                free(metodo);
                free(ruta);
                free(host);
                return NULL;
            }
        }
        
        // Consultar informacion al servidor origen
        char buffer_from_int_to_char[20];
        snprintf(buffer_from_int_to_char, sizeof(buffer_from_int_to_char), "%d", env_puerto_server_origen);
        char *respuesta_origen = http_get(host, buffer_from_int_to_char, ruta, largo_salida, 1); 
        free(metodo);
        free(ruta);
        free(host);
        if (respuesta_origen == NULL) {
            printf("No se puede recuperar información del origen");
            return NULL;
        }
        // Guardar .dat y .meta
        long tam_bytes;
        if (tam_bytes = escribir_dat(dir_punto_dat, respuesta_origen, largo_salida)) == -1 {
            printf("Error al intentar escribir el .dat");
            return NULL;
        }

        if (escribir_meta(dir_punto_meta, clave, tam_bytes)) == -1 {
            printf("Error al intentar escribir el .meta");
            return NULL;
        }

        printf("Salida Apache Server: %.*s", (int)*largo_salida, respuesta_origen);
        
        return respuesta_origen;
    }
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
            if (respuesta == NULL) break;
            send(client_socket, respuesta, largo_salida, MSG_NOSIGNAL);
            free(respuesta);
        }

        close(client_socket);
    }

    close(server_socket);

    return 0;
}