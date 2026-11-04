// Codigo recuperado de: https://www.geeksforgeeks.org/computer-networks/udp-client-server-using-connect-c-implementation/
// y modificado a necesidad de la implementacion del proyecto 2

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <strings.h>

#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include<netinet/in.h>
#include <sys/time.h>
#include <unistd.h>

#include "../comun/http_cliente.h"
#include "../comun/variables_entorno.h"

size_t construir_respuesta(char *buffer_solicitud, char *ip, char *buffer_respuesta, size_t max) {
    size_t longitud_label;
    size_t i = 12;

    while (buffer_solicitud[i] != '\0') {
        longitud_label = buffer_solicitud[i];
        i += longitud_label + 1;
    }

    i += 5; // Saltarse QTYPE y QCLASS de la Seccion Question
    // Aca i ya esta al inicio de RR Answer
    if (i + 16 > max) return 0;

    // ############ Formar la respuesta
    memcpy(buffer_respuesta, buffer_solicitud, i);

    buffer_respuesta[2] = buffer_respuesta[2] | 0x80; // Cambiar QR a 1 para indicar que es una respuesta
    buffer_respuesta[3] |= 0x80;      // RA = 1
    // Poner ANCOUNT en 1
    buffer_respuesta[6] = 0x00;
    buffer_respuesta[7] = 0x01;
    // NSCOUNT en 0
    buffer_respuesta[8] = 0x00;
    buffer_respuesta[9] = 0x00;
    // ARCOUNT en 0
    buffer_respuesta[10] = 0x00;
    buffer_respuesta[11] = 0x00;

    // La seccion Question queda igual

    // Pasamos a la seccion RR del Answer
    // Puntero a la ubicacion del nombre del dominio al que pertenece el RR
    buffer_respuesta[i] = 0xC0; // Indica que es un puntero
    i++;
    buffer_respuesta[i] = 0x0C; // Indica que se encuentra en el offset 12
    i++;

    // TYPE -> A = 1
    buffer_respuesta[i] = 0x00;
    i++;
    buffer_respuesta[i] = 0x01;
    i++;

    // CLASS -> IN = 1
    buffer_respuesta[i] = 0x00;
    i++;
    buffer_respuesta[i] = 0x01;
    i++;

    // TTL -> 300 seg
    buffer_respuesta[i] = 0x00;
    i++;
    buffer_respuesta[i] = 0x00;
    i++;
    buffer_respuesta[i] = 0x01;
    i++;
    buffer_respuesta[i] = 0x2c;
    i++;

    // RDLENGHT -> es IPv4 por lo tanto es 4
    buffer_respuesta[i] = 0x00;
    i++;
    buffer_respuesta[i] = 0x04;
    i++;

    struct in_addr parse_ip;
    if (inet_pton(AF_INET, ip, &parse_ip) != 1) {
        return 0;
    }
    memcpy(buffer_respuesta + i, &parse_ip.s_addr, 4);
    i += 4;

    return i;
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

char *consultar_api(char *ruta, struct sockaddr_in cliaddr, const char *env_node_api_host, int env_node_api_port) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &cliaddr.sin_addr, ip, sizeof(ip));
    char buffer_ruta[128];
    snprintf(buffer_ruta, sizeof(buffer_ruta), ruta, ip);
    char buffer_from_int_to_char[20];
    snprintf(buffer_from_int_to_char, sizeof(buffer_from_int_to_char), "%d", env_node_api_port);
    size_t largo_salida;
    int env_node_api_tls = env_entero("NODE_API_TLS", 0);
    char *json = http_get(env_node_api_host, buffer_from_int_to_char, buffer_ruta, &largo_salida, 0, NULL, env_node_api_tls);
    // Usar largo_salida en caso de ser necesario. De momento no se utiliza.
    return json;
}


/* Decide si el dominio consultado es de los nuestros.
 *
 * "propios" es una lista separada por comas, p.ej.
 * "crhoy.com,api.demo.cr,sitio.demo.cr". Se compara sin distinguir mayusculas
 * porque el DNS no las distingue.
 *
 * Tambien acepta los subdominios: una entrada "demo.cr" cubre "api.demo.cr".
 * Se exige que el caracter anterior sea un punto, para que "nodemo.cr" no
 * coincida con "demo.cr". */
bool dominio_es_propio(const char *dominio, const char *propios) {
    /* Lista vacia: se intercepta TODO, que es como funcionaba antes de existir
     * esta variable. Devolver false aqui haria que un despliegue que no la
     * define dejara de interceptar nada, en silencio. */
    if (propios == NULL || *propios == '\0') return true;

    size_t largo_dominio = strlen(dominio);
    const char *inicio = propios;

    while (*inicio != '\0') {
        const char *coma = strchr(inicio, ',');
        size_t largo = (coma != NULL) ? (size_t)(coma - inicio) : strlen(inicio);

        if (largo > 0 && largo <= largo_dominio) {
            const char *cola = dominio + largo_dominio - largo;
            if (strncasecmp(cola, inicio, largo) == 0 &&
                (cola == dominio || *(cola - 1) == '.')) {
                return true;
            }
        }

        if (coma == NULL) break;
        inicio = coma + 1;
    }
    return false;
}

/* Reenvia la consulta tal cual a un servidor DNS de verdad y devuelve su
 * respuesta tambien tal cual.
 *
 * No hace falta interpretar ni construir nada: el identificador de
 * transaccion, las banderas y la seccion de pregunta ya viajan en esos bytes,
 * asi que al cliente le llega algo perfectamente coherente.
 *
 * Devuelve cuantos bytes trajo la respuesta, o 0 si no se pudo. */
ssize_t reenviar_consulta(const char *servidor, const char *consulta, size_t largo_consulta,
                          char *respuesta, size_t max_respuesta) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket (reenvio)");
        return 0;
    }

    /* Sin este tope, un servidor que no conteste cuelga al interceptor entero:
     * atiende de a una consulta por vez. */
    struct timeval espera = { .tv_sec = 2, .tv_usec = 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &espera, sizeof(espera));

    struct sockaddr_in dns = {0};
    dns.sin_family = AF_INET;
    dns.sin_port = htons(53);
    if (inet_pton(AF_INET, servidor, &dns.sin_addr) != 1) {
        fprintf(stderr, "DNS_EXTERNO=\"%s\" no es una IPv4 valida\n", servidor);
        close(fd);
        return 0;
    }

    if (sendto(fd, consulta, largo_consulta, 0, (struct sockaddr*)&dns, sizeof(dns)) < 0) {
        perror("sendto (reenvio)");
        close(fd);
        return 0;
    }

    ssize_t n = recvfrom(fd, respuesta, max_respuesta, 0, NULL, NULL);
    if (n < 0) fprintf(stderr, "el DNS externo %s no respondio a tiempo\n", servidor);
    close(fd);
    return (n > 0) ? n : 0;
}

int main() {
    // Leer variables de entorno
    int env_puerto_dns_interceptor = env_entero("DNS_INTERCEPTOR_PORT", 5000);
    const char *env_node_api_host = env_texto("NODE_API_HOST", "127.0.0.1");
    int env_node_api_port = env_entero("NODE_API_PORT", 3000);

    /* Dominios que administra esta plataforma, separados por comas. Cualquier
     * otro se reenvia a DNS_EXTERNO, de modo que el interceptor se puede poner
     * como resolvedor del sistema sin dejarlo sin internet. Vacio = se
     * intercepta todo, que es como venia funcionando. */
    const char *env_dominios_propios = env_texto("DOMINIOS_PROPIOS", "");
    const char *env_dns_externo = env_texto("DNS_EXTERNO", "8.8.8.8");

    int listenfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listenfd < 0) { perror("socket"); return 1; }

    struct sockaddr_in servaddr = {0};
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(env_puerto_dns_interceptor);

    if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof servaddr) < 0) {
        perror("bind");
        return 1;
    }
    printf("Interceptor escuchando en el puerto %d\n", env_puerto_dns_interceptor);

    /* --- ciclo de atención --- */
    /* 4096 y no 512: las respuestas de dominios reales, con varios registros
     * o con EDNS, superan los 512 bytes y llegarian truncadas. */
    char buffer[4096];
    struct sockaddr_in cliaddr;

    while (1) {
        //receive the datagram
        socklen_t len = sizeof(cliaddr);          /* se reinicia cada vuelta */

        ssize_t n = recvfrom(listenfd, buffer, sizeof buffer, 0, (struct sockaddr*)&cliaddr, &len);
        if (n < 0) { perror("recvfrom"); continue; }
        
        // Imprimir la informacion recibida
        printf("recibidos %ld bytes:\n", n);
        imprimir_hex((unsigned char *)buffer, n);
        
        // Obtener el valor de qr y opcode
        bool qr = ((buffer[2] >> 7) & 1) == 1;
        uint8_t op_code = (buffer[2] >> 3) & 15;
        
        if (!qr && op_code == 0) {
            char *dominio = extraer_dominio(buffer);
            printf("%s\n", dominio);

            /* Si el dominio no es nuestro, el interceptor no tiene nada que
             * decir: se lo pasa a un DNS de verdad y devuelve su respuesta sin
             * tocarla. Esto es lo que permite ponerlo como resolvedor del
             * sistema sin romper el resto de internet. */
            if (!dominio_es_propio(dominio, env_dominios_propios)) {
                char reenvio[4096];
                ssize_t largo_reenvio = reenviar_consulta(env_dns_externo, buffer, n,
                                                          reenvio, sizeof(reenvio));
                if (largo_reenvio > 0) {
                    sendto(listenfd, reenvio, largo_reenvio, 0,
                           (struct sockaddr*)&cliaddr, sizeof(cliaddr));
                    printf("Reenviado a %s (%zd bytes)\n", env_dns_externo, largo_reenvio);
                }
                continue;
            }

            
            // %%%%%%%%%%%%%%%%%%%% Consultar pais + ip del Cache Zonal (una sola llamada)
            char codigo_pais[3] = {0};
            char ip[16] = {0};

            char *respuesta_api = consultar_api("/api/resolver/%s", cliaddr, env_node_api_host, env_node_api_port);
            if (respuesta_api == NULL) {
                fprintf(stderr, "no se pudo consultar el API\n");
                continue;
            }
            printf("respuesta API: %s\n", respuesta_api);

            // Extraer el codigo pais
            char *p = strstr(respuesta_api, "\"pais\":\"");
            if (p) {
                p += 8;
                codigo_pais[0] = p[0];
                codigo_pais[1] = p[1];
                codigo_pais[2] = '\0';
            }

            // Extraer la ip del Zonal Cache
            p = strstr(respuesta_api, "\"ip_cache\":\"");
            if (p) {
                p += 12;
                size_t k = 0;
                while (p[k] != '"' && p[k] != '\0' && k < sizeof(ip) - 1) {
                    ip[k] = p[k];
                    k++;
                }
                ip[k] = '\0';
            }
            free(respuesta_api);

            if (codigo_pais[0] == '\0' || ip[0] == '\0') {
                fprintf(stderr, "respuesta del API incompleta\n");
                continue;
            }
            printf("Codigo pais: %s\n", codigo_pais);
            printf("IP Zonal Cache: %s\n", ip);
            // Contruir la respuesta para el DNS Client
            char respuesta[512];
            size_t largo_respuesta = construir_respuesta(buffer, ip, respuesta, sizeof(respuesta));
            // Enviar la respuesta al DNS Client
            if (largo_respuesta > 0) {
                sendto(listenfd, respuesta, largo_respuesta, 0, (struct sockaddr*)&cliaddr, sizeof(cliaddr));
            }
            printf("Es estandar query.\n");
        } else {
            printf("No estandar query.\n");
        }
    }
}