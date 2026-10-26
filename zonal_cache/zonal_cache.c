#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

#include "../comun/http_cliente.h"
#include "../comun/variables_entorno.h"

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
    memcpy(host, puntero_inicio_host, cant_caracteres);
    host[cant_caracteres] = '\0';
    printf("Host: %s\n", host);

    // Verificar si se tiene informacion en cache del host correspondiente
    //      De momento esto lo dejamos pendiente, en futuras iteraciones agregamos el cache.
    //      Por ahora solo hacemos el puente hacia el servidor Apache
    
    // Si no, entonces consultar la informacion al servidor origen
    //      Consultar http://127.0.0.1:8081/
    char buffer_from_int_to_char[20];
    snprintf(buffer_from_int_to_char, sizeof(buffer_from_int_to_char), "%d", env_puerto_server_origen);
    char *respuesta_origen = http_get("127.0.0.1", buffer_from_int_to_char, ruta, largo_salida, 1);
    if (respuesta_origen == NULL) {
        printf("No se puede recuperar información del origen.");
        free(metodo);
        free(ruta);
        free(host);
        return NULL;
    }

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