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

// Leer las variables de entorno
static const char *env_texto(const char *nombre, const char *respaldo) {
    const char *v = getenv(nombre);
    return (v && *v) ? v : respaldo;
}

static int env_entero(const char *nombre, int respaldo) {
    const char *v = getenv(nombre);
    if (!v || !*v) return respaldo;

    int errno = 0;
    char *sobrante;
    long n = strtol(v, &sobrante, 10);

    if (errno != 0 || *sobrante != '\0' || n < 0 || n > 65535) {
        fprintf(stderr, "Aviso: %s=\"%s\" no es valido, se usa %d\n", nombre, v, respaldo);
        return respaldo;
    }
    return (int)n;
}

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

char *get_ip_to_country(char *ruta, struct sockaddr_in cliaddr, const char *node_api_host, int node_api_port) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &cliaddr.sin_addr, ip, sizeof(ip));
    char buffer_ruta[128];
    snprintf(buffer_ruta, sizeof(buffer_ruta), ruta, ip);
    char buffer_to_integer[20];
    snprintf(buffer_to_integer, sizeof(buffer_to_integer), "%d", node_api_port);
    char *json = http_get(node_api_host, buffer_to_integer, buffer_ruta);
    return json;
}

char *get_zonal_ip(char *ruta, char *codigo_pais, const char *node_api_host, int node_api_port) {
    char buffer_ruta[128];
    snprintf(buffer_ruta, sizeof(buffer_ruta), ruta, codigo_pais);
    char buffer_to_integer[20];
    snprintf(buffer_to_integer, sizeof(buffer_to_integer), "%d", node_api_port);
    char *json = http_get(node_api_host, buffer_to_integer, buffer_ruta);
    return json;
}

int main() {   
    char buffer[100];
    // Leer variables de entorno
    int puerto_dns_interceptor = env_entero("DNS_INTERCEPTOR_PORT", 5000);
    const char *node_api_host = env_texto("NODE_API_HOST", "127.0.0.1");
    int node_api_port = env_entero("NODE_API_PORT", 3000);

    int listenfd, len;
    struct sockaddr_in servaddr, cliaddr;
    bzero(&servaddr, sizeof(servaddr));

    // Create a UDP Socket
    listenfd = socket(AF_INET, SOCK_DGRAM, 0);        
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(puerto_dns_interceptor);
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
        
        // %%%%%%%%%%%%%%%%%%%% Consultar ip to country
        char *respuesta_ip_to_country = get_ip_to_country("/api/ip_to_country/%s", cliaddr, node_api_host, node_api_port);
        // Extraer el codigo pais
        char codigo_pais[3];
        if (respuesta_ip_to_country) {
            printf("respuesta codigo pais: %s\n", respuesta_ip_to_country);
            char *p = strstr(respuesta_ip_to_country, "\"pais\":\"");
            if (p) {
                p += 8;
                codigo_pais[0] = p[0];
                codigo_pais[1] = p[1];
                codigo_pais[2] = '\0';
            }
            free(respuesta_ip_to_country);
        }
        
        // %%%%%%%%%%%%%%%%%%%% Consultar la ip del Cache Zonal para un codigo pais
        printf("Codigo pais: %s\n", codigo_pais);
        char *respuesta_zonal_ip = get_zonal_ip("/api/zonal_ip/%s", codigo_pais, node_api_host, node_api_port);
        if (respuesta_zonal_ip) {
            printf("respuesta zonal ip: %s\n", respuesta_zonal_ip);
            free(respuesta_zonal_ip);
        }
        printf("Es estandar query.\n");
    } else {
        printf("No estandar query.\n");
    }
         
    // send the response
    //sendto(listenfd, message, strlen(message), 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
}