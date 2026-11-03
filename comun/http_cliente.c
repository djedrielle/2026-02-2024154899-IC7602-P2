#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <signal.h>

#include "http_cliente.h"

/* Libera todo lo que se haya llegado a reservar. Evita repetir tres lineas de
 * limpieza en cada camino de error, que es como se filtran recursos. */
static void cerrar_conexion(SSL *ssl, SSL_CTX *ctx, int sockfd) {
    if (ssl != NULL) SSL_free(ssl);
    if (ctx != NULL) SSL_CTX_free(ctx);
    if (sockfd >= 0) close(sockfd);
}

char *http_get(const char *host, const char *puerto, const char *ruta, size_t *largo_salida, int proxy, const char *encabezados_extra, int TLS) {
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

    //Si es una conexion https entonces hay que crear el contexto
    SSL_CTX *ctx = NULL;
    SSL *ssl = NULL;
    if (TLS) {
        ctx = SSL_CTX_new(TLS_client_method());
        if (ctx == NULL) {
            fprintf(stderr, "Error al crear el contexto cliente SSL.\n");
            ERR_print_errors_fp(stderr);
            cerrar_conexion(ssl, ctx, sockfd);
            return NULL;
        }

        /* Carga el almacen de certificados raiz del sistema operativo.
         * En una imagen de Docker minima ese almacen NO existe: hay que instalar
         * el paquete ca-certificates o toda verificacion falla con
         * "unable to get local issuer certificate". */
        if (!SSL_CTX_set_default_verify_paths(ctx)) {
            fprintf(stderr, "Error al cargar el almacen de CA del sistema.\n");
            ERR_print_errors_fp(stderr);
            cerrar_conexion(ssl, ctx, sockfd);
            return NULL;
        }

        /* Exige que el certificado del servidor este firmado por una CA conocida.
         * Sin esto, SSL_connect aceptaria cualquier certificado, incluido uno
         * fabricado por quien intercepte la conexion. */
        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

        /* Muchos servidores cierran el TCP sin mandar el aviso close_notify de
         * TLS. OpenSSL 3.x reporta eso como error ("unexpected eof while
         * reading") y descartaria una respuesta que llego completa.
         *
         * Como pedimos "Connection: close", el fin de la conexion ES el fin de
         * la respuesta, asi que se le indica que trate ese EOF como normal.
         * El precio: un atacante podria cortar la conexion antes de tiempo y la
         * respuesta llegaria truncada sin aviso. */
        SSL_CTX_set_options(ctx, SSL_OP_IGNORE_UNEXPECTED_EOF);

        ssl = SSL_new(ctx);
        if (ssl == NULL) {
            fprintf(stderr, "Error al crear la estructura SSL.\n");
            ERR_print_errors_fp(stderr);
            cerrar_conexion(ssl, ctx, sockfd);
            return NULL;
        }

        if (!SSL_set_fd(ssl, sockfd)) {
            fprintf(stderr, "Error al asociar el socket a la estructura SSL.\n");
            ERR_print_errors_fp(stderr);
            cerrar_conexion(ssl, ctx, sockfd);
            return NULL;
        }

        /* SNI: anuncia en claro, dentro del Client Hello, a que dominio le
         * hablamos. Un servidor que sirve miles de sitios desde la misma IP
         * (Vercel, por ejemplo) lo necesita para elegir cual certificado enviar. */
        if (!SSL_set_tlsext_host_name(ssl, host)) {
            fprintf(stderr, "Error al establecer el SNI para %s.\n", host);
            ERR_print_errors_fp(stderr);
            cerrar_conexion(ssl, ctx, sockfd);
            return NULL;
        }

        /* Verifica que el certificado pertenezca a ESTE host.
         * SSL_VERIFY_PEER solo comprueba que la firma sea de una CA valida; sin
         * esta linea, un certificado legitimo de cualquier otro dominio pasaria
         * la prueba y la autenticacion del servidor seria inutil. */
        if (!SSL_set1_host(ssl, host)) {
            fprintf(stderr, "Error al fijar el nombre a verificar (%s).\n", host);
            ERR_print_errors_fp(stderr);
            cerrar_conexion(ssl, ctx, sockfd);
            return NULL;
        }

        /* Aqui ocurre el handshake completo. Si el certificado esta vencido, lo
         * firmo una CA desconocida o el nombre no coincide, falla aca y el
         * detalle sale por ERR_print_errors_fp. */
        if (SSL_connect(ssl) <= 0) {
            fprintf(stderr, "Fallo el handshake TLS con %s:%s\n", host, puerto);
            ERR_print_errors_fp(stderr);
            cerrar_conexion(ssl, ctx, sockfd);
            return NULL;
        }
    }

    if (encabezados_extra == NULL) encabezados_extra = "";
    char peticion[512];
    int largo = snprintf(peticion, sizeof(peticion),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "%s"
        "\r\n",
        ruta, host, encabezados_extra);

    if (ssl != NULL) {
        // SSL_write devuelve <= 0 en error, no solo negativos
        if (SSL_write(ssl, peticion, largo) <= 0) {
            fprintf(stderr, "Error al enviar la peticion sobre TLS.\n");
            ERR_print_errors_fp(stderr);
            cerrar_conexion(ssl, ctx, sockfd);
            return NULL;
        }
    } else {
        if (send(sockfd, peticion, (size_t)largo, 0) < 0) {
            perror("send");
            cerrar_conexion(ssl, ctx, sockfd);
            return NULL;
        }
    }

    size_t cap = 4096, total = 0;
    char  *crudo = malloc(cap);
    if (!crudo) { cerrar_conexion(ssl, ctx, sockfd); return NULL; }

    ssize_t n;
    if (ssl != NULL) {
        while ((n = SSL_read(ssl, crudo + total, cap - total)) > 0) {
            total += (size_t)n;
            if (total == cap) {
                cap *= 2;
                char *tmp = realloc(crudo, cap);
                if (!tmp) { free(crudo); cerrar_conexion(ssl, ctx, sockfd); return NULL; }
                crudo = tmp;
            }
        }
        /* SSL_read devuelve 0 tanto en un cierre limpio como en un error. Se
         * consulta el motivo: ZERO_RETURN es el close_notify del servidor, que
         * es el final normal de la respuesta. */
        if (n < 0 || (n == 0 && SSL_get_error(ssl, (int)n) != SSL_ERROR_ZERO_RETURN)) {
            fprintf(stderr, "Error al leer la respuesta sobre TLS tras %zu bytes.\n", total);
            ERR_print_errors_fp(stderr);
            free(crudo);
            cerrar_conexion(ssl, ctx, sockfd);
            return NULL;
        }
        SSL_shutdown(ssl);
    } else {
        while ((n = recv(sockfd, crudo + total, cap - total, 0)) > 0) {
            total += (size_t)n;
            if (total == cap) {
                cap *= 2;
                char *tmp = realloc(crudo, cap);
                if (!tmp) { free(crudo); cerrar_conexion(ssl, ctx, sockfd); return NULL; }
                crudo = tmp;
            }
        }
        if (n < 0) { free(crudo); perror("recv"); cerrar_conexion(ssl, ctx, sockfd); return NULL; }
    }

    cerrar_conexion(ssl, ctx, sockfd);
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