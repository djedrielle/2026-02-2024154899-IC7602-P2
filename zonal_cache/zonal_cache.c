#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

int PUERTO_IN = 9777;
int TAM_ENTRADA = 1024;

void procesar_entrada(char* buffer_entrada) {
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

    // Construir la respuesta

    // Enviar la respuesta al usuario

    free(metodo);
    free(ruta);
    free(host);
}


int main(void) {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_address, client_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(PUERTO_IN);

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address));
    listen(server_socket, 1);

    ssize_t bytes_leidos;
    while (true) {
        printf("\nEscuchando conexiones...\n");
        socklen_t client_size = sizeof(client_address);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_size);
        printf("Cliente conectado, IP: %s, puerto: %i\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

        char entrada[TAM_ENTRADA];
        while ((bytes_leidos = read(client_socket, entrada, sizeof(entrada))) > 0) {
            printf("Entrada: %s", entrada);
            procesar_entrada(entrada);
        }

        close(client_socket);
    }

    close(server_socket);

    return 0;
}