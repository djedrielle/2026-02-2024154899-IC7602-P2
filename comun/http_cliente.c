#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "http_cliente.h"

char *http_get(const char *host, const char *puerto, const char *ruta, size_t *largo_salida, int proxy, const char *encabezados_extra) {
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

    if (encabezados_extra == NULL) encabezados_extra = "";
    char peticion[512];
    int largo = snprintf(peticion, sizeof(peticion),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "%s"
        "\r\n",
        ruta, host, encabezados_extra);

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

    *largo_salida = total;
    if (proxy) return crudo; // Cuando se hace la consulta desde el proxy se retorna toda la respuesta, no solo el cuerpo

    char *cuerpo = strstr(crudo, "\r\n\r\n");
    if (!cuerpo) { free(crudo); return NULL; }
    cuerpo += 4;

    char *resultado_final = strdup(cuerpo);
    free(crudo);
    return resultado_final;
}