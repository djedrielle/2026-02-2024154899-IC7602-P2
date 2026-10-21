// Codigo recuperado de: https://www.geeksforgeeks.org/computer-networks/udp-client-server-using-connect-c-implementation/
// y modificado a necesidad de la implementacion del proyecto 2

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <strings.h>

#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include<netinet/in.h>
#define PORT 5000
#define MAXLINE 1000

void imprimir_hex(const unsigned char *buf, int n) {
    for (int i = 0; i < n; i++) {
        printf("%02x ", buf[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");
}

char *extraer_dominio(char *buffer) {
    static char dominio[256];
    int indice_buffer = 12;
    int indice_dominio = 0;
    int longitud_label = buffer[indice_buffer];
    int fin;
    while (buffer[indice_buffer] != 0) {
        indice_buffer = indice_buffer + 1;
        fin = indice_buffer + longitud_label;
        while (indice_buffer < fin) {
            dominio[indice_dominio] = buffer[indice_buffer];
            indice_dominio += 1;
            indice_buffer += 1;
        }
        if (buffer[indice_buffer] != 0) {
            dominio[indice_dominio] = '.';
            indice_dominio += 1;
        } else {
            dominio[indice_dominio] = '\0';
        }
        longitud_label = buffer[indice_buffer];
    }
    return dominio;
}

/* Hace un GET a http://host:puerto{ruta} y devuelve SOLO el cuerpo
 * de la respuesta, reservado con malloc (el que llama libera con free()).
 * Devuelve NULL si algo falla. */
char *http_get(const char *host, const char *puerto, const char *ruta) {
    struct addrinfo hints = {0}, *resultado;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, puerto, &hints, &resultado) != 0) {
        fprintf(stderr, "no se pudo resolver %s\n", host);
        return NULL;
    }

    int sockfd = socket(resultado->ai_family, resultado->ai_socktype, 0);
    if (sockfd < 0 || connect(sockfd, resultado->ai_addr, resultado->ai_addrlen) < 0) {
        perror("connect");
        freeaddrinfo(resultado);
        return NULL;
    }
    freeaddrinfo(resultado);

    char peticion[512];
    int largo = snprintf(peticion, sizeof(peticion),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "\r\n",
        ruta, host);

    if (send(sockfd, peticion, (size_t)largo, 0) < 0) {
        perror("send");
        close(sockfd);
        return NULL;
    }

    size_t cap = 4096, total = 0;
    char  *crudo = malloc(cap);
    if (!crudo) { close(sockfd); return NULL; }

    ssize_t n;
    while ((n = recv(sockfd, crudo + total, cap - total, 0)) > 0) {
        total += (size_t)n;
        if (total == cap) {
            cap *= 2;
            char *tmp = realloc(crudo, cap);
            if (!tmp) { free(crudo); close(sockfd); return NULL; }
            crudo = tmp;
        }
    }
    close(sockfd);

    if (n < 0) { free(crudo); perror("recv"); return NULL; }
    crudo[total] = '\0';

    char *cuerpo = strstr(crudo, "\r\n\r\n");
    if (!cuerpo) { free(crudo); return NULL; }
    cuerpo += 4;

    char *resultado_final = strdup(cuerpo);
    free(crudo);
    return resultado_final;
}

char *get(char *ruta, struct sockaddr_in cliaddr) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &cliaddr.sin_addr, ip, sizeof(ip));
    char buffer_ruta[128];
    snprintf(buffer_ruta, sizeof(buffer_ruta), ruta, ip);
    // En futuros avances: colocar el dominio al API REST y el puerto como variable de entorno
    char *json = http_get("api-node.tu-dominio.com", "80", buffer_ruta);
    return json;
}

int main() {   
    char buffer[100];
    char *message = "Hello Client";
    int listenfd, len;
    struct sockaddr_in servaddr, cliaddr;
    bzero(&servaddr, sizeof(servaddr));

    // Create a UDP Socket
    listenfd = socket(AF_INET, SOCK_DGRAM, 0);        
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);
    servaddr.sin_family = AF_INET; 
 
    // bind server address to socket descriptor
    bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
     
    //receive the datagram
    len = sizeof(cliaddr);
    int n = recvfrom(listenfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&cliaddr,&len); //receive message from server
    // Imprimir la informacion recibida
    printf("recibidos %d bytes:\n", n);
    imprimir_hex((unsigned char *)buffer, n);
    // Obtener el valor de qr y opcode
    bool qr = ((buffer[2] >> 7) & 1) == 1;
    uint8_t op_code = (buffer[2] >> 3) & 15;
    if (!qr && op_code == 0) {
        char *dominio = extraer_dominio(buffer);
        printf("%s\n", dominio);
        // Consultar ip to country
        char *respuesta = get("/api/ip_to_country/%s", cliaddr);
        if (respuesta) {
            printf("respuesta: %s\n", respuesta);
            free(respuesta);
        }
        printf("Es estandar query.\n");
    } else {
        printf("No estandar query.\n");
    }
         
    // send the response
    //sendto(listenfd, message, strlen(message), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
}