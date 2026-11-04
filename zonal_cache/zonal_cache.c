#define _GNU_SOURCE
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
#include <dirent.h>
#include <limits.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <signal.h>
#include <sys/wait.h>
#include <fnmatch.h>
#include <ctype.h>

#include "../comun/http_cliente.h"
#include "../comun/variables_entorno.h"

#define MAX_API_KEYS 8

/* Segundos que se espera a que un cliente mande su peticion antes de cerrarle.
 *
 * Sin esto, una conexion que se abre y no habla deja al proceso que la atiende
 * bloqueado en read() para siempre. Los navegadores abren conexiones
 * especulativas (preconnect) que hacen exactamente eso. */
#define SEGUNDOS_ESPERA_CLIENTE 10

enum AlgReemplazo {
    LRU,
    LFU,
    FIFO,
    MRU,
    RANDOM 
};

typedef struct {
    char dominio[256];
    char origen_host[256];
    int origen_puerto;
    long tamanno_maximo;
    long ttl;
    enum AlgReemplazo algoritmo;
    int requiere_auth;
    int tls;
    char api_keys[MAX_API_KEYS][128];
    int cantidad_api_keys;
} ConfigDominio;

typedef struct {
    char id[24];
    char dominio[256];
    char patron[256];
    char tipo[12];
    long ttl;
    enum AlgReemplazo algoritmo;
    long tamanno_cache;
    int requiere_auth;
} ReglaURL;

typedef struct {
    char clave[256];
    char id_regla[24];
    long tamanno;
    time_t insercion;
    time_t ultimo_acceso;
    long cantidad_accesos;
    time_t expiracion;
} Meta;

typedef struct {
    char hash[17];     // el nombre sin el ".meta"
    Meta meta;
} EntradaCache;

typedef struct {
    int fd;
    SSL *ssl;
} Conexion;


int leer_meta(char *dir_punto_meta, Meta *salida) {
    FILE *f = fopen(dir_punto_meta, "r");
    if (f == NULL) {
        return -1;
    }

    // Leer los datos del archivo y construir un objeto Meta con estos datos
    char clave[256];
    char id_regla[24];
    long tamanno;
    long insercion;
    long ultimo_acceso;
    long cantidad_accesos;
    long expiracion;

    if (fscanf(f, "%*s %255s %*s %23s %*s %ld %*s %ld %*s %ld %*s %ld %*s %ld", clave, id_regla, &tamanno, &insercion, &ultimo_acceso, &cantidad_accesos, &expiracion) != 7) {
        fclose(f);
        fprintf(stderr, "Formato invalido en el .meta %s\n", dir_punto_meta);
        return -1;
    }

    snprintf(salida->clave, sizeof(salida->clave), "%s", clave);
    snprintf(salida->id_regla, sizeof(salida->id_regla), "%s", id_regla);
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
    size_t escritos = fwrite(respuesta_origen, sizeof(char), *largo_salida, f);
    fclose(f);
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
    fprintf(fw, "clave_original: %s\nid_regla: %s\ntamanno_bytes: %ld\nmomento_insercion: %ld\nultimo_acceso: %ld\ncantidad_accesos: %ld\nmomento_expiracion: %ld", 
        m.clave,
        m.id_regla,
        m.tamanno,
        m.insercion,
        m.ultimo_acceso,
        m.cantidad_accesos,
        m.expiracion
    );
    fclose(fw);
    return 0;
}

int escribir_meta(char *dir_punto_meta, char *clave, char *id_regla, long tam_bytes, long TTL) {
    Meta m;
    snprintf(m.clave, sizeof(m.clave), "%s", clave);
    snprintf(m.id_regla, sizeof(m.id_regla), "%s", id_regla);
    m.tamanno = tam_bytes;
    m.insercion = time(NULL);
    m.ultimo_acceso = time(NULL);
    m.cantidad_accesos = 1;
    m.expiracion = time(NULL) + TTL;

    FILE *fw = fopen(dir_punto_meta, "w");
    if (fw == NULL) {
        return -1;
    }
    fprintf(fw, "clave_original: %s\nid_regla: %s\ntamanno_bytes: %ld\nmomento_insercion: %ld\nultimo_acceso: %ld\ncantidad_accesos: %ld\nmomento_expiracion: %ld", 
        m.clave,
        m.id_regla,
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

int buscar_config(char *host, ConfigDominio *configs, int cantidad_configs, ConfigDominio *config_destino) {
    for (int i = 0; i < cantidad_configs; i++) {
        if (!strcmp(host, configs[i].dominio)) {
            *config_destino = configs[i];
            return 0;
        }
    }
    return -1;
}

int inventario(EntradaCache **entradas_cache, size_t *tamanno_total, size_t *tamanno_total_regla, int *cantidad_entradas, char *dir_directorio, char *id_regla) {
    DIR *dir = opendir(dir_directorio);
    if (dir == NULL) {
        perror("inventario");
        return -1;
    }
    struct dirent *dp;
    errno = 0;
    int largo;
    // Contar la cantidad de .meta
    while ((dp = readdir(dir)) != NULL) {
        largo = strlen(dp->d_name);
        if ((largo > 5) && (!strcmp(dp->d_name + largo - 5, ".meta"))) {
            (*cantidad_entradas)++;
        }
    }
    if (errno != 0) {
        closedir(dir);
        return -1;
    }
    
    EntradaCache *ptr_entradas_cache = malloc(sizeof(EntradaCache) * *cantidad_entradas);
    if (ptr_entradas_cache == NULL) {
        closedir(dir);
        return -1;
    }
    int i = 0;
    rewinddir(dir);
    while ((dp = readdir(dir)) != NULL) {
        largo = strlen(dp->d_name);
        if ((largo > 5) && (!strcmp(dp->d_name + largo - 5, ".meta"))) {
            char ruta[strlen(dir_directorio) + 1 + largo + 5];
            snprintf(ruta, sizeof(ruta), "%s/%s", dir_directorio, dp->d_name);
            Meta m;
            if (leer_meta(ruta, &m) == -1) {
                // Si el ".meta" esta corrupto entonces se eliminan los dos archivos para que el cache se autorepare
                char ruta_meta[strlen(dir_directorio) + 1 + 1 + 16 + 5];
                char ruta_dat[strlen(dir_directorio) + 1 + 1 + 16 + 4];
                snprintf(ruta_meta, sizeof(ruta_meta), "%s/%s", dir_directorio, dp->d_name);
                snprintf(ruta_dat, sizeof(ruta_dat), "%s/%.*s.dat", dir_directorio, largo - 5, dp->d_name);
                if (remove(ruta_meta) || remove(ruta_dat)) {
                    fprintf(stderr, "Error al intentar eliminar los archivos %s y %s\n", ruta_meta, ruta_dat);
                }
                free(ptr_entradas_cache);
                closedir(dir);
                return -1;
            }
            *tamanno_total += m.tamanno;
            if (!strcmp(m.id_regla, id_regla)) {
                *tamanno_total_regla += m.tamanno;
                char hash[17];
                snprintf(hash, sizeof(hash), "%.*s", largo - 5, dp->d_name);
                snprintf(ptr_entradas_cache[i].hash, 17, "%s", hash);
                ptr_entradas_cache[i].meta = m;
                i++;
            }
            
        }
    }
    closedir(dir);
    *entradas_cache = ptr_entradas_cache;
    *cantidad_entradas = i;
    return 0;
}

int elegir_victima(EntradaCache *entradas_cache, enum AlgReemplazo algoritmo, int cantidad_entradas) {
    if (algoritmo == RANDOM) {
        int min = 0;
        int max = cantidad_entradas - 1;
        int indice_random = min + rand() % (max - min + 1);
        return indice_random;
    }
    
    int indice_mejor = 0;
    for (int i = 0; i < cantidad_entradas; i++) {
        switch (algoritmo) {
            case LRU:
                if (entradas_cache[i].meta.ultimo_acceso < entradas_cache[indice_mejor].meta.ultimo_acceso) indice_mejor = i;
            break;

            case MRU:
                if (entradas_cache[i].meta.ultimo_acceso > entradas_cache[indice_mejor].meta.ultimo_acceso) indice_mejor = i;
            break;

            case LFU:
                if (entradas_cache[i].meta.cantidad_accesos < entradas_cache[indice_mejor].meta.cantidad_accesos) indice_mejor = i;
            break;

            case FIFO:
                if (entradas_cache[i].meta.insercion < entradas_cache[indice_mejor].meta.insercion) indice_mejor = i;
            break;

            case RANDOM:
            break;
        }
    }
    return indice_mejor;
}

/* Arma una respuesta HTTP completa en memoria del monton.
 *
 * El resultado se devuelve tal cual desde procesar_entrada(), y main() lo libera
 * con free() despues del send(). Por eso NO puede ser un literal de cadena ni un
 * arreglo estatico: free() sobre memoria que no vino de malloc termina el proceso.
 *
 * encabezados_extra puede ser NULL; si no lo es, debe venir ya terminado en "\r\n".
 * cuerpo puede ser NULL, y en ese caso la respuesta va sin cuerpo (Content-Length: 0).
 *
 * Devuelve NULL si no hay memoria. Escribe el largo total en *largo_salida. */
char *construir_respuesta_http(int codigo, const char *razon, const char *encabezados_extra, const char *cuerpo, size_t *largo_salida) {
    if (encabezados_extra == NULL) encabezados_extra = "";
    if (cuerpo == NULL) cuerpo = "";

    const char *plantilla =
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "%s"
        "\r\n"
        "%s";

    size_t largo_cuerpo = strlen(cuerpo);

    /* Primera pasada con destino NULL y tamanno 0: snprintf no escribe nada y
     * devuelve cuantos caracteres habria necesitado. Asi se pide exactamente
     * la memoria que hace falta, sin adivinar un tamanno. */
    int necesarios = snprintf(NULL, 0, plantilla, codigo, razon, largo_cuerpo, encabezados_extra, cuerpo);
    if (necesarios < 0) return NULL;

    char *respuesta = malloc((size_t)necesarios + 1); // +1 por el '\0'
    if (respuesta == NULL) return NULL;

    int escritos = snprintf(respuesta, (size_t)necesarios + 1, plantilla, codigo, razon, largo_cuerpo, encabezados_extra, cuerpo);
    if (escritos != necesarios) {
        free(respuesta);
        return NULL;
    }

    /* El largo es el de la respuesta sin el '\0': ese terminador existe para poder
     * tratarla como cadena, pero no se transmite. */
    *largo_salida = (size_t)escritos;
    return respuesta;
}

/* 401 para una credencial ausente o invalida en un dominio que exige autenticacion.
 * El encabezado WWW-Authenticate es lo que indica al cliente como autenticarse;
 * un 401 sin el esta incompleto segun el RFC 7235. */
char *respuesta_401(size_t *largo_salida) {
    return construir_respuesta_http(401, "Unauthorized",
        "WWW-Authenticate: ApiKey realm=\"zonal-cache\"\r\n",
        "401 Unauthorized: la credencial es invalida o no fue enviada.\n",
        largo_salida);
}

/* 302 hacia el formulario del UI, para una peticion sin credenciales a un dominio
 * protegido. El navegador sigue el Location y el usuario se autentica alla. */
char *respuesta_302(const char *url_login, size_t *largo_salida) {
    char encabezado[1024];
    snprintf(encabezado, sizeof(encabezado), "Location: %s\r\n", url_login);
    return construir_respuesta_http(302, "Found", encabezado,
        "302 Found: se requiere autenticacion, siga el encabezado Location.\n",
        largo_salida);
}

/* 404 para un Host que no tiene configuracion en Firestore. */
char *respuesta_404(size_t *largo_salida) {
    return construir_respuesta_http(404, "Not Found", NULL,
        "404 Not Found: el dominio solicitado no esta configurado en esta cache.\n",
        largo_salida);
}

/* 502 cuando el servidor origen no responde o la cache no pudo servir el recurso. */
char *respuesta_502(size_t *largo_salida) {
    return construir_respuesta_http(502, "Bad Gateway", NULL,
        "502 Bad Gateway: no se pudo obtener el recurso del servidor origen.\n",
        largo_salida);
}

char *consultar_node_api(const char *ruta) {
    // Leer variables de entorno
    const char *env_extra_headers = env_texto("EXTRA_HEADERS", "x-config-key: ABCDEFG\r\n");
    const char *env_node_api_host = env_texto("NODE_API_HOST", "zonal-cache-rest-api-nodejs.vercel.app");
    int env_node_api_port = env_entero("NODE_API_PORT", 443);
    int env_node_api_tls = env_entero("NODE_API_TLS", 1);
    size_t largo_salida = 0;

    char buffer_from_int_to_char[20];
    snprintf(buffer_from_int_to_char, sizeof(buffer_from_int_to_char), "%d", env_node_api_port);
    /* NODE_API_TLS va por variable de entorno y no por la configuracion del
     * dominio, porque la cache necesita hablarle al API antes de tener ninguna
     * configuracion cargada. Ponerlo en 1 es lo que permite apuntar al deploy
     * de Vercel, que solo sirve HTTPS.*/
    
    return http_get(env_node_api_host, buffer_from_int_to_char, ruta, &largo_salida, 0, env_extra_headers, env_node_api_tls);
}

int extraer_booleano(char *inicio_config, char *str_buscar) {
    char *etiqueta = strstr(inicio_config, str_buscar);
    if (etiqueta == NULL) return -1;
    etiqueta += strlen(str_buscar);
    if (strncmp(etiqueta, "true", 4) == 0) {
        return 1;
    } else {
        return 0;
    }
}

int extraer_tipo(char *ruta, char *tipo_ruta) {
    char *ultimo_slash = strrchr(ruta, '/');
    if (ultimo_slash == NULL) {
        return -1;
    }
    char *punto = strrchr(ultimo_slash, '.');
    if (punto == NULL) {
        snprintf(tipo_ruta, 12, "%s", "otros");
        return 0;
    }
    char tipo[12];
    snprintf(tipo, sizeof(tipo), "%s", punto + 1);
    // Pasar todo a minuscula
    for (int i = 0; tipo[i]; i++) {
        tipo[i] = tolower((unsigned char)tipo[i]);
    }
    if (strcmp(tipo, "html") && strcmp(tipo, "css") && strcmp(tipo, "js") && strcmp(tipo, "png") && strcmp(tipo, "jpg") && strcmp(tipo, "gif") && strcmp(tipo, "svg") && strcmp(tipo, "json")) {
        snprintf(tipo_ruta, 12, "%s", "otros");
        return 0;
    }
    snprintf(tipo_ruta, 12, "%s", tipo);
    return 0;
}

/* Decide si la llave que llego en x-api-key es una de las del dominio.
 *
 * "recibida" apunta al valor dentro del buffer de la peticion, que sigue con el
 * fin de linea y los encabezados que vengan despues, asi que primero hay que
 * medir donde termina: strcspn lo corta en el primer '\r' o '\n'.
 *
 * La comparacion exige que los largos sean iguales antes de mirar los bytes, y
 * eso cierra dos agujeros que tenia la version anterior:
 *
 *   - Una llave almacenada vacia daba strlen 0, y strncmp(lo_que_sea, "", 0)
 *     devuelve 0, es decir "iguales". Cualquier valor autenticaba. Un dominio
 *     creado desde el UI nace exactamente asi.
 *   - Se comparaba solo hasta el largo de la almacenada, de modo que un valor
 *     que apenas empezara igual que la llave buena tambien pasaba.
 *
 * Devuelve 0 cuando el dominio no tiene llaves, que es lo correcto: una regla
 * que exige api_key sobre un dominio sin llaves no debe dejar pasar a nadie. */
int api_key_valida(const char *recibida, const ConfigDominio *config) {
    size_t largo_recibido = strcspn(recibida, "\r\n");

    for (int i = 0; i < config->cantidad_api_keys; i++) {
        const char *almacenada = config->api_keys[i];
        if (strlen(almacenada) == largo_recibido && !strncmp(recibida, almacenada, largo_recibido)) {
            return 1;
        }
    }
    return 0;
}

char *procesar_entrada(char* buffer_entrada, ConfigDominio *configs, int cantidad_configs, ReglaURL *reglas,  int cantidad_reglas, size_t *largo_salida, const char *env_dir_cache) {
    // Extraer el metodo
    // Se coloca en el primer ' ' para obtener el metodo y luego obtener el parametro de la peticion
    char *puntero_fin_buscado = strchr(buffer_entrada, ' ');
    if (puntero_fin_buscado == NULL) {
        fprintf(stderr, "Peticion mal formada: no se encontro el metodo\n");
        return NULL;
    }
    size_t cant_caracteres = puntero_fin_buscado - buffer_entrada;

    char *metodo = malloc(cant_caracteres + 1); // + 1 para agregar el '\0'
    memcpy(metodo, buffer_entrada, cant_caracteres);
    metodo[cant_caracteres] = '\0';
    printf("Metodo: %s\n", metodo);

    // Extraer la ruta
    char *puntero_fin_buscado_anterior = puntero_fin_buscado;
    puntero_fin_buscado_anterior++; // Apuntar al '/'
    puntero_fin_buscado = strchr(puntero_fin_buscado_anterior, ' ');
    if (puntero_fin_buscado == NULL) {
        fprintf(stderr, "Peticion mal formada: no se encontro la ruta\n");
        free(metodo);
        return NULL;
    }
    cant_caracteres = puntero_fin_buscado - puntero_fin_buscado_anterior;

    char *ruta = malloc(cant_caracteres + 1);
    memcpy(ruta, puntero_fin_buscado_anterior, cant_caracteres);
    ruta[cant_caracteres] = '\0';
    char *session = strstr(ruta, "?session=");
    if (session != NULL) {
        *session = '\0';
        session += 9;
    }
    printf("Ruta: %s\n", ruta);
    // Extraer el tipo de la ruta (.jpg, .png, ...etc)
    char tipo_ruta[12];
    if (extraer_tipo(ruta, tipo_ruta) == -1) {
        fprintf(stderr, "No se encontró '/' en la ruta a la hora de identificar el tipo de recurso\n");
        free(metodo);
        free(ruta);
        return NULL;
    }

    // Extraer el host
    char *puntero_inicio_host = strstr(buffer_entrada, "Host: ");
    if (puntero_inicio_host == NULL) {
        fprintf(stderr, "Peticion mal formada: falta el encabezado Host\n");
        free(metodo);
        free(ruta);
        return NULL;
    }
    puntero_inicio_host += 6; // Saltar "Host: "
    puntero_fin_buscado = strchr(puntero_inicio_host, '\r');
    if (puntero_fin_buscado == NULL) {
        fprintf(stderr, "Peticion mal formada: el encabezado Host no termina en CRLF\n");
        free(metodo);
        free(ruta);
        return NULL;
    }
    cant_caracteres = puntero_fin_buscado - puntero_inicio_host;
    char *host = malloc(cant_caracteres + 1);
    size_t i = 0;
    while (i < cant_caracteres && puntero_inicio_host[i] != ':') {
        host[i] = puntero_inicio_host[i];
        i++;
    }
    host[i] = '\0';
    
    char *host_completo = malloc(cant_caracteres + 1);
    i = 0;
    while (i < cant_caracteres) {
        host_completo[i] = puntero_inicio_host[i];
        i++;
    }
    printf("Host: %s, completo: %s\n", host, host_completo);

    // Buscar la configuracion de la dominio
    // Esto nos sirve porque en caso de que no exista configuracion especifica
    // para la URL solicitada, entonces se aplica la configuracion del dominio
    ConfigDominio config;
    if (buscar_config(host, configs, cantidad_configs, &config) == -1) {
        fprintf(stderr, "No se encontró una configuración para ese host.\n");
        free(metodo);
        free(ruta);
        free(host);
        free(host_completo);
        return NULL;
    }

    // Se crea una regla default. Si se encuentra una regla que aplique para el
    // recurso solicitado entonces se modifica, de lo contrario se trabaja con la
    // regla default, esta es la especificada por el dominio
    ReglaURL regla = {0};
    snprintf(regla.id, sizeof(regla.id), "%s", "__dominio__");
    regla.ttl = config.ttl;
    regla.algoritmo = config.algoritmo;
    regla.tamanno_cache = config.tamanno_maximo;
    regla.requiere_auth = config.requiere_auth;    
    if (reglas != NULL) {
        char id_candidato[24] = {0};
        for (int i = 0; i < cantidad_reglas; i++) {
            if ((!strcmp(reglas[i].dominio, host)) && (fnmatch(reglas[i].patron, ruta, 0) == 0)) {
                snprintf(id_candidato, sizeof(id_candidato), "%s", reglas[i].id);
                break;
            }
        }
        if (*id_candidato != 0) {
            for (int i = 0; i < cantidad_reglas; i++) {
                if ((!strcmp(reglas[i].id, id_candidato)) && (!strcmp(reglas[i].tipo, tipo_ruta))) {
                    regla = reglas[i];
                    break;
                }
            }
        }
    }

    // Verificar autenticacion
    char *api_key = strcasestr(buffer_entrada, "x-api-key: ");
    if (regla.requiere_auth) {
        if (api_key != NULL) {
            api_key += 11;
            if (!api_key_valida(api_key, &config)) {
                free(metodo);
                free(ruta);
                free(host);
                free(host_completo);
                return respuesta_401(largo_salida);
            }
        } else if (session != NULL) {
            char buffer_ruta[12 + strlen(session) + strlen(host) + 1];
            snprintf(buffer_ruta, sizeof(buffer_ruta), "/api/sesion/%s?dominio=%s", session, host);
            char *respuesta_api = consultar_node_api(buffer_ruta);
            if (respuesta_api == NULL) {
                fprintf(stderr, "config: no se pudo contactar el API (revise NODE_API_HOST y NODE_API_PORT, y que el API este arriba)\n");
                free(metodo);
                free(ruta);
                free(host);
                free(host_completo);
                return NULL;
            }
            if (extraer_booleano(respuesta_api, "\"valida\":") != 1) {
                free(respuesta_api);
                free(metodo);
                free(ruta);
                free(host);
                free(host_completo);
                return respuesta_401(largo_salida);
            }
            free(respuesta_api);
        } else {
            // Leer variables de entorno
            const char *env_node_api_host = env_texto("NODE_API_HOST", "zonal-cache-rest-api-nodejs.vercel.app");
            int env_ui_port = env_entero("UI_PORT", 443);
            char ruta_location[256];
            snprintf(ruta_location, sizeof(ruta_location), "https://%s:%d/login?redirect=%s%s", env_node_api_host, env_ui_port, host_completo, ruta);
            free(metodo);
            free(ruta);
            free(host);
            free(host_completo);
            return respuesta_302(ruta_location, largo_salida);
        }
    }

    // Hashear host + ruta
    char clave[strlen(host) + strlen(ruta) + 1];
    snprintf(clave, sizeof(clave), "%s%s", host, ruta);

    char hash_clave[16 + 1];
    snprintf(hash_clave, sizeof(hash_clave), "%016lx", fnv1a(clave));

    // Verificar si se tiene informacion en cache del host correspondiente
    struct stat st = {0};
    
    char dir_directorio[strlen(env_dir_cache) + strlen(host) + 2]; // +2 por el '/' y el '\0'
    snprintf(dir_directorio, sizeof(dir_directorio), "%s/%s", env_dir_cache, host);

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
        free(host_completo);

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
        fprintf(stderr, "Recurso retornado desde cache.\n");
        return respuesta;
    
    } else {
        // Si no esta creada se crea, mas abajo se trae la informacion del dominio
        if (stat(dir_directorio, &st)) {
            if (mkdir(dir_directorio, 0777) != 0) {
                perror("Error al crear el directorio");
                free(metodo);
                free(ruta);
                free(host);
                free(host_completo);
                return NULL;
            }
        }
        
        // Consultar informacion al servidor origen
        char buffer_from_int_to_char[20];
        snprintf(buffer_from_int_to_char, sizeof(buffer_from_int_to_char), "%d", config.origen_puerto);
        char *respuesta_origen = http_get(config.origen_host, buffer_from_int_to_char, ruta, largo_salida, 1, NULL, config.tls);
        free(metodo);
        free(ruta);
        free(host);
        free(host_completo);
        if (respuesta_origen == NULL) {
            fprintf(stderr, "No se puede recuperar información del origen");
            return NULL;
        }

        // Si el largo de la salida es mayor al tamanno maximo del dominio, entonces servimos la respuesta sin cachear
        if (*largo_salida > config.tamanno_maximo) {
            fprintf(stderr, "Esta salida se sirve sin cachear. El tamanno del recurso es mayor al tamanno del dominio. tam_recurso = %d, tam_max_dominio = %ld", (int)*largo_salida, config.tamanno_maximo);
            return respuesta_origen;
        }

        // Obtener el tamanno total del directorio cache del dominio y una lista de las entradas presentes en cache
        size_t tamanno_total = 0;
        size_t tamanno_total_regla = 0;
        EntradaCache *entradas_cache;
        int cantidad_entradas = 0;
        if (inventario(&entradas_cache, &tamanno_total, &tamanno_total_regla, &cantidad_entradas, dir_directorio, regla.id) == -1) {
            return NULL;
        }
        while (tamanno_total_regla + *largo_salida > regla.tamanno_cache) {
            int indice_victima = elegir_victima(entradas_cache, regla.algoritmo, cantidad_entradas);
            // Eliminar recurso
            char ruta_meta[sizeof(dir_directorio) + 1 + 16 + 5];
            char ruta_dat[sizeof(dir_directorio) + 1 + 16 + 4];
            snprintf(ruta_meta, sizeof(ruta_meta), "%s/%s.meta", dir_directorio, entradas_cache[indice_victima].hash);
            snprintf(ruta_dat, sizeof(ruta_dat), "%s/%s.dat", dir_directorio, entradas_cache[indice_victima].hash);
            if (remove(ruta_meta) || remove(ruta_dat)) {
                fprintf(stderr, "Error al intentar eliminar los archivos %s y %s\n", ruta_meta, ruta_dat);
                free(entradas_cache);
                return NULL;
            }
            tamanno_total -= entradas_cache[indice_victima].meta.tamanno;
            entradas_cache[indice_victima] = entradas_cache[cantidad_entradas - 1];
            cantidad_entradas--;
        }

        free(entradas_cache);

        // Guardar .dat y .meta
        size_t tam_bytes;
        if ((tam_bytes = escribir_dat(dir_punto_dat, respuesta_origen, largo_salida)) != *largo_salida) {
            printf("Error al intentar escribir el .dat");
            free(respuesta_origen);
            return NULL;
        }

        if ((escribir_meta(dir_punto_meta, clave, regla.id, tam_bytes, regla.ttl)) == -1) {
            printf("Error al intentar escribir el .meta");
            free(respuesta_origen);
            return NULL;
        }

        printf("Salida Apache Server: %.*s", (int)*largo_salida, respuesta_origen);

        return respuesta_origen;
    }
}

int extraer_array_texto(char *inicio_config, char *str_buscar, char destino[][128], int max_elementos, int *cantidad) {
    *cantidad = 0;

    char *etiqueta = strstr(inicio_config, str_buscar);
    if (etiqueta == NULL) return 0;
    etiqueta += strlen(str_buscar);

    while (1) {
        char *fin_texto = strchr(etiqueta, '"');
        if (fin_texto == NULL) return 0;

        if (*cantidad == max_elementos) return -1;

        int largo = fin_texto - etiqueta;
        snprintf(destino[*cantidad], sizeof(destino[0]), "%.*s", largo, etiqueta);
        (*cantidad)++;

        if (fin_texto[1] != ',') return 0;
        etiqueta = fin_texto + 3;   // salta la coma y la comilla que abre el siguiente
    }
}

int extraer_texto(char *inicio_config, char *str_buscar, char *destino, size_t tam_destino) {
    char *etiqueta = strstr(inicio_config, str_buscar);
    if (etiqueta == NULL) return -1;
    etiqueta += strlen(str_buscar);
    char *fin_texto = strchr(etiqueta, '"');
    if (fin_texto == NULL) return -1;
    int largo = fin_texto - etiqueta;
    snprintf(destino, tam_destino, "%.*s", largo, etiqueta);
    return 0;
}

long extraer_int(char *inicio_config, char *str_buscar) {
    char *etiqueta = strstr(inicio_config, str_buscar);
    if (etiqueta == NULL) return -1;
    etiqueta += strlen(str_buscar);
    char *endptr;
    long numero = strtol(etiqueta, &endptr, 10);
    return numero;
}

int obtener_algoritmo(char *algoritmo_texto) {
    static const char *NOMBRES_ALG[] = { "LRU", "LFU", "FIFO", "MRU", "RANDOM" };
    for (size_t i = 0; i < sizeof(NOMBRES_ALG)/sizeof(NOMBRES_ALG[0]); i++) {
        if (!strcmp(NOMBRES_ALG[i], algoritmo_texto)) {
            return i;
        }
    }
    return -1;
}

int cargar_configs(ConfigDominio *configs, int *cantidad_configs, int env_cant_max_dominios, ReglaURL *reglas, int *cantidad_reglas, int env_cant_max_reglas) {
    char *respuesta_api = consultar_node_api("/api/config");
    if (respuesta_api == NULL) {
        fprintf(stderr, "config: no se pudo contactar el API (revise NODE_API_HOST y NODE_API_PORT, y que el API este arriba)\n");
        return -1;
    }

    *cantidad_configs = extraer_int(respuesta_api, "\"cantidad\":"); // Parsear la cantidad de hosts
    if (*cantidad_configs == -1) {
        fprintf(stderr, "config: la respuesta del API no trae el campo \"cantidad\". Lo recibido fue: %.200s\n", respuesta_api);
        fprintf(stderr, "config: si dice \"No autorizado\", revise que EXTRA_HEADERS lleve el mismo secreto que CONFIG_API_KEY en el API\n");
        free(respuesta_api);
        return -1;
    }
    if (*cantidad_configs == 0) {
        fprintf(stderr, "config: el API no devolvio ningun dominio usable. O la coleccion 'dominios' de Firestore esta vacia,\n");
        fprintf(stderr, "config: o todos los documentos fueron descartados por mal formados. Consulte /api/config con curl\n");
        fprintf(stderr, "config: y revise el campo \"descartados\", que trae el motivo de cada uno\n");
        free(respuesta_api);
        return -1;
    }
    if (*cantidad_configs > env_cant_max_dominios) {
        fprintf(stderr, "config: el API devolvio %d dominios y el maximo compilado es %d (suba CANT_MAX_DOMINIOS)\n",
            *cantidad_configs, env_cant_max_dominios);
        free(respuesta_api);
        return -1;
    }
    // Parsear el resto de la informacion
    char *inicio_config = strstr(respuesta_api, "\"dominios\":[");
    if (inicio_config == NULL) {
        fprintf(stderr, "config: la respuesta trae \"cantidad\" pero no el arreglo \"dominios\". Lo recibido fue: %.200s\n", respuesta_api);
        free(respuesta_api);
        return -1;
    }
    
    // Ciclo para parsear los dominios
    inicio_config += 12;
    char *fin_config;
    for (int i = 0; i < *cantidad_configs; i++) {
        fin_config = strchr(inicio_config + 1, '}');
        if (fin_config == NULL) {
            fprintf(stderr, "config: el JSON se corta en el dominio %d de %d; no se encontro el cierre del objeto\n",
                i + 1, *cantidad_configs);
            free(respuesta_api);
            return -1;
        }
        *fin_config = '\0';

        char dominio[256];
        if (extraer_texto(inicio_config, "\"dominio\":\"", dominio, sizeof(dominio)) == -1) {
            fprintf(stderr, "config: al dominio %d de %d le falta el campo \"dominio\": %.200s\n",
                i + 1, *cantidad_configs, inicio_config);
            free(respuesta_api);
            return -1;
        }
        // De aqui en adelante ya se conoce el nombre del dominio, asi que los
        // mensajes lo incluyen para poder ubicar el documento en Firestore.
        char origen_host[256];
        if (extraer_texto(inicio_config, "\"origen_host\":\"", origen_host, sizeof(origen_host)) == -1) {
            fprintf(stderr, "config: el dominio '%s' no trae \"origen_host\" como texto (revise que el documento en Firestore lo tenga y no este vacio)\n", dominio);
            free(respuesta_api);
            return -1;
        }
        
        char algoritmo_texto[16];
        if (extraer_texto(inicio_config, "\"algoritmo\":\"", algoritmo_texto, sizeof(algoritmo_texto)) == -1) {
            fprintf(stderr, "config: el dominio '%s' no trae \"algoritmo\" como texto\n", dominio);
            free(respuesta_api);
            return -1;
        }
        
        int int_algoritmo = obtener_algoritmo(algoritmo_texto);
        if (int_algoritmo == -1) {
            fprintf(stderr, "config: el dominio '%s' declara el algoritmo '%s', que no es ninguno de LRU, LFU, FIFO, MRU, RANDOM\n", dominio, algoritmo_texto);
            free(respuesta_api);
            return -1;
        }
        enum AlgReemplazo algoritmo = int_algoritmo;
        int origen_puerto = extraer_int(inicio_config, "\"origen_puerto\":");
        long tamanno_maximo = extraer_int(inicio_config, "\"tamanno_maximo\":");
        long ttl = extraer_int(inicio_config, "\"ttl\":");
        int requiere_auth = extraer_booleano(inicio_config, "\"requiere_auth\":");
        int tls = extraer_booleano(inicio_config, "\"tls\":");

        char api_keys[MAX_API_KEYS][128] = {0};
        int cantidad_api_keys = 0;
        if (extraer_array_texto(inicio_config, "\"api_keys\":[\"", api_keys, MAX_API_KEYS, &cantidad_api_keys) == -1) {
            fprintf(stderr, "config: el dominio '%s' declara mas de %d llaves; las sobrantes se ignoran y sus clientes recibiran 401\n",
                dominio, MAX_API_KEYS);
        }

        // Validar que la informacion se recupere bien
        if (origen_puerto == -1 || tamanno_maximo == -1 || ttl == -1 || requiere_auth == -1) {
            fprintf(stderr, "config: al dominio '%s' le falta un campo numerico o booleano ->%s%s%s%s\n",
                dominio,
                origen_puerto   == -1 ? " origen_puerto"   : "",
                tamanno_maximo  == -1 ? " tamanno_maximo"  : "",
                ttl             == -1 ? " ttl"             : "",
                requiere_auth   == -1 ? " requiere_auth"   : "");
            free(respuesta_api);
            return -1;
        }

        snprintf(configs[i].dominio, 256, "%s", dominio);
        snprintf(configs[i].origen_host, 256, "%s", origen_host);
        for (int n = 0; n < cantidad_api_keys; n++) {
            snprintf(configs[i].api_keys[n], 128, "%s", api_keys[n]);
        }
        configs[i].cantidad_api_keys = cantidad_api_keys;
        configs[i].origen_puerto = origen_puerto;
        configs[i].algoritmo = algoritmo;
        configs[i].tamanno_maximo = tamanno_maximo;
        configs[i].ttl = ttl;
        configs[i].requiere_auth = requiere_auth;
        configs[i].tls = tls;

        *fin_config = '}';
        inicio_config = fin_config + 2;
    }

    // Ciclo para extraer las reglas por URL
    *cantidad_reglas = extraer_int(respuesta_api, "\"cantidad_reglas\":");
    if (*cantidad_reglas == 0) {
        reglas = NULL;
        fprintf(stderr, "No existen reglas especificas para ninguna URL.\n");
        free(respuesta_api);
        return 0;
    }
    if (*cantidad_reglas == -1) {
        reglas = NULL;
        fprintf(stderr, "Surgio un error al intentar obtener la cantidad de reglas.\n");
        free(respuesta_api);
        return -1;
    }

    if (*cantidad_reglas > env_cant_max_reglas) {
        fprintf(stderr, "regla: el API devolvio %d reglas y el maximo compilado es %d (suba CANT_MAX_REGLAS)\n",
            *cantidad_reglas, env_cant_max_reglas);
        free(respuesta_api);
        return -1;
    }

    char *inicio_regla = strstr(respuesta_api, "\"reglas\":[");
    if (inicio_regla == NULL) {
        fprintf(stderr, "regla: la respuesta trae \"cantidad_reglas\" pero no el arreglo \"reglas\". Lo recibido fue: %.200s\n", respuesta_api);
        free(respuesta_api);
        return -1;
    }

    inicio_regla += 10;
    char *fin_regla;
    for (int i = 0; i < *cantidad_reglas; i++) {
        fin_regla = strchr(inicio_regla + 1, '}');
        if (fin_regla == NULL) {
            fprintf(stderr, "regla: el JSON se corta en la regla %d de %d; no se encontro el cierre del objeto\n",
                i + 1, *cantidad_reglas);
            free(respuesta_api);
            return -1;
        }
        *fin_regla = '\0';

        char id[24];
        if (extraer_texto(inicio_regla, "\"id\":\"", id, sizeof(id)) == -1) {
            fprintf(stderr, "regla: a la regla %d de %d le falta el campo \"id\": %.200s\n",
                i + 1, *cantidad_reglas, inicio_regla);
            free(respuesta_api);
            return -1;
        }
        char dominio[256];
        if (extraer_texto(inicio_regla, "\"dominio\":\"", dominio, sizeof(dominio)) == -1) {
            fprintf(stderr, "regla: a la regla %d de %d le falta el campo \"dominio\": %.200s\n",
                i + 1, *cantidad_reglas, inicio_regla);
            free(respuesta_api);
            return -1;
        }
        char patron[256];
        if (extraer_texto(inicio_regla, "\"patron\":\"", patron, sizeof(patron)) == -1) {
            fprintf(stderr, "regla: a la regla %d de %d le falta el campo \"patron\": %.200s\n",
                i + 1, *cantidad_reglas, inicio_regla);
            free(respuesta_api);
            return -1;
        }
        char tipo[12];
        if (extraer_texto(inicio_regla, "\"tipo\":\"", tipo, sizeof(tipo)) == -1) {
            fprintf(stderr, "regla: a la regla %d de %d le falta el campo \"tipo\": %.200s\n",
                i + 1, *cantidad_reglas, inicio_regla);
            free(respuesta_api);
            return -1;
        }
        
        char algoritmo_texto[16];
        if (extraer_texto(inicio_regla, "\"algoritmo\":\"", algoritmo_texto, sizeof(algoritmo_texto)) == -1) {
            fprintf(stderr, "regla: la regla '%s' no trae \"algoritmo\" como texto\n", id);
            free(respuesta_api);
            return -1;
        }
        
        int int_algoritmo = obtener_algoritmo(algoritmo_texto);
        if (int_algoritmo == -1) {
            fprintf(stderr, "regla: la regla '%s' declara el algoritmo '%s', que no es ninguno de LRU, LFU, FIFO, MRU, RANDOM\n", id, algoritmo_texto);
            free(respuesta_api);
            return -1;
        }

        long ttl = extraer_int(inicio_regla, "\"ttl\":");
        enum AlgReemplazo algoritmo = int_algoritmo;
        long tamanno_cache = extraer_int(inicio_regla, "\"tamanno_cache\":");

        char autenticacion[24];
        if (extraer_texto(inicio_regla, "\"autenticacion\":\"", autenticacion, sizeof(autenticacion)) == -1) {
            fprintf(stderr, "regla: la regla '%s' no trae metodo de \"autenticacion\" valido\n", id);
            free(respuesta_api);
            return -1;
        }
        
        int requiere_auth = 0;
        if (strcmp(autenticacion, "ninguna")) {
            requiere_auth = 1;
        }
        
        // Validar que la informacion se recupere bien
        if (tamanno_cache == -1 || ttl == -1 || requiere_auth == -1) {
            fprintf(stderr, "regla: la regla '%s' le falta un campo numerico o booleano ->%s%s%s\n",
                id,
                tamanno_cache  == -1 ? " tamanno_cache"  : "",
                ttl             == -1 ? " ttl"             : "",
                requiere_auth   == -1 ? " requiere_auth"   : "");
            free(respuesta_api);
            return -1;
        }

        snprintf(reglas[i].id, 24, "%s", id);
        snprintf(reglas[i].dominio, 256, "%s", dominio);
        snprintf(reglas[i].patron, 256, "%s", patron);
        snprintf(reglas[i].tipo, 12, "%s", tipo);
        reglas[i].ttl = ttl;
        reglas[i].algoritmo = algoritmo;
        reglas[i].tamanno_cache = tamanno_cache;
        reglas[i].requiere_auth = requiere_auth;

        *fin_regla = '}';
        inicio_regla = fin_regla + 2;
    }
    free(respuesta_api);
    return 0;
}

ssize_t conexion_leer(Conexion *c, char *ptr_inicio, size_t tam_escribir) {
    if (c->ssl != NULL) {
        return SSL_read(c->ssl, ptr_inicio, tam_escribir);
    } else {
        return read(c->fd, ptr_inicio, tam_escribir);
    }
}

/* Escribe los 'largo' bytes completos en la conexion.
 *
 * Tanto send() como SSL_write() pueden transmitir MENOS de lo pedido: cuando el
 * buffer de salida del socket se llena, devuelven lo que alcanzaron a copiar.
 * Sin este ciclo una respuesta grande llega truncada, y como el cliente leyo un
 * Content-Length mayor, se queda esperando bytes que nunca van a salir.
 *
 * Devuelve los bytes escritos, o -1 si la conexion fallo. */
ssize_t conexion_escribir(Conexion *c, const char *datos, size_t largo) {
    size_t enviados = 0;

    while (enviados < largo) {
        size_t resto = largo - enviados;
        ssize_t n;

        if (c->ssl != NULL) {
            /* SSL_write recibe la longitud como int, asi que se parte el envio
             * si alguna vez superara ese limite. */
            int pedir = resto > INT_MAX ? INT_MAX : (int)resto;
            n = SSL_write(c->ssl, datos + enviados, pedir);

            if (n <= 0) {
                int motivo = SSL_get_error(c->ssl, (int)n);
                /* Sobre un socket bloqueante estos dos solo aparecen durante una
                 * renegociacion: la operacion no fallo, hay que repetirla. */
                if (motivo == SSL_ERROR_WANT_WRITE || motivo == SSL_ERROR_WANT_READ) continue;
                fprintf(stderr, "Error al escribir sobre TLS tras %zu de %zu bytes\n", enviados, largo);
                ERR_print_errors_fp(stderr);
                return -1;
            }
        } else {
            n = send(c->fd, datos + enviados, resto, 0);

            if (n < 0) {
                if (errno == EINTR) continue; // una senal interrumpio el envio
                fprintf(stderr, "Error al escribir tras %zu de %zu bytes: %s\n",
                    enviados, largo, strerror(errno));
                return -1;
            }
            if (n == 0) break; // el otro extremo ya no recibe
        }

        enviados += (size_t)n;
    }

    return (ssize_t)enviados;
}

void conexion_cerrar(Conexion *c) {
    if (c->ssl != NULL) {
        SSL_shutdown(c->ssl);
        SSL_free(c->ssl);
    }
    close(c->fd);
}

int main(void) {
    signal(SIGPIPE, SIG_IGN);

    /* Cada conexion se atiende en un proceso hijo. Ignorar SIGCHLD hace que el
     * nucleo los recoja solo: no hacen falta los codigos de salida y asi no se
     * acumulan zombis. De paso, al estar ignorada la senal no interrumpe el
     * accept() del padre, asi que no hay que manejar EINTR ahi. */
    signal(SIGCHLD, SIG_IGN);
    srand(time(NULL));
    // Leer variables de entorno
    int env_max_tam_entrada = env_entero("MAX_TAM_ENTRADA", 32768);
    int env_puerto_zonal_cache = env_entero("ZONAL_PUERTO_IN", 9777);
    int env_cant_max_dominios = env_entero("CANT_MAX_DOMINIOS", 16);
    int env_cant_max_reglas = env_entero("CANT_MAX_REGLAS", 32);
    const char *env_dir_cache = env_texto("CACHE_DIR", "cache");
    int env_TLS_habilitado = env_entero("TLS_HABILITADO", 0);
    const char *env_dir_certificado = env_texto("CERTIFICADO_DIR", "./cert.pem");
    const char *env_dir_private_key = env_texto("PRIVATE_KEY_DIR", "./clave.pem");

    int cantidad_configs;
    ConfigDominio configs[env_cant_max_dominios];
    int cantidad_reglas;
    ReglaURL reglas[env_cant_max_reglas];
    if (cargar_configs(configs, &cantidad_configs, env_cant_max_dominios, reglas, &cantidad_reglas, env_cant_max_reglas) == -1) {
        fprintf(stderr, "No se pudo cargar la configuracion desde el API\n");
        return 1;
    }

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

    // Crear el directorio cache
    if (mkdir(env_dir_cache, 0777) != 0 && errno != EEXIST) {
        perror("mkdir");
        close(server_socket);
        return 1;
    }

    // Crear el contexto SSL (en caso de que sea necesario)
    SSL_CTX *ctx = NULL;
    if (env_TLS_habilitado) {
        ctx = SSL_CTX_new(TLS_server_method());
        if (ctx == NULL) {
            fprintf(stderr, "Error al crear el contexto SSL.\n");
            ERR_print_errors_fp(stderr);
            close(server_socket);
            return 1;
        }
        if (!SSL_CTX_use_certificate_file(ctx, env_dir_certificado, SSL_FILETYPE_PEM)) {
            fprintf(stderr, "Error al cargar el certificado.\n");
            ERR_print_errors_fp(stderr);
            SSL_CTX_free(ctx);
            close(server_socket);
            return 1;
        }
        if (!SSL_CTX_use_PrivateKey_file(ctx, env_dir_private_key, SSL_FILETYPE_PEM)) {
            fprintf(stderr, "Error al cargar la llave privada.\n");
            ERR_print_errors_fp(stderr);
            SSL_CTX_free(ctx);
            close(server_socket);
            return 1;
        }
        if (!SSL_CTX_check_private_key(ctx)) {
            fprintf(stderr, "La llave privada no corresponde al certificado.\n");
            ERR_print_errors_fp(stderr);
            SSL_CTX_free(ctx);
            close(server_socket);
            return 1;
        }
        if (SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION) != 1) {
            fprintf(stderr, "Error al establecer la versión mínima de TLS.\n");
            ERR_print_errors_fp(stderr);
            SSL_CTX_free(ctx);
            close(server_socket);
            return 1;
        }
    }

    if (listen(server_socket, 10) < 0) {
        perror("listen");
        close(server_socket);
        return 1;
    }
    printf("Zonal Cache escuchando en el puerto %d\n", env_puerto_zonal_cache);

    char entrada[env_max_tam_entrada];
    while (true) {
        printf("\nEscuchando conexiones...\n");
        socklen_t client_size = sizeof(client_address);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_size);
        if (client_socket < 0) {
            perror("accept");
            continue;
        }
        printf("Cliente conectado, IP: %s, puerto: %i\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

        /* Una conexion, un proceso.
         *
         * Antes la cache atendia de a una: mientras buscaba en el origen (dos
         * segundos para una pagina real) las demas conexiones se quedaban en la
         * cola del nucleo. Un navegador abre hasta seis en paralelo y pide
         * decenas de recursos, asi que la pagina tardaba minutos y terminaba en
         * ERR_TIMED_OUT, aunque un curl suelto funcionara.
         *
         * El fflush va antes del fork a proposito: el hijo hereda el buffer de
         * stdout, y si queda algo sin volcar, cada hijo lo repetiria al salir. */
        fflush(stdout);
        pid_t hijo = fork();

        if (hijo < 0) {
            perror("fork");
            close(client_socket);
            continue;
        }

        if (hijo > 0) {
            /* Padre: cierra SU copia del descriptor y vuelve a aceptar.
             *
             * Ese close no es opcional. Padre e hijo comparten el descriptor, y
             * si el padre se queda con el suyo abierto la conexion no se cierra
             * del todo cuando el hijo termina: se van acumulando descriptores
             * hasta que accept() falla con EMFILE y la cache deja de aceptar. */
            close(client_socket);
            continue;
        }

        /* ---------- de aqui en adelante, solo el hijo ---------- */

        // No lo usa, y mantenerlo abierto estorbaria al reiniciar el servicio.
        close(server_socket);

        /* Tiempo de espera de lectura.
         *
         * El fork resuelve la concurrencia, pero una conexion que no manda nada
         * dejaria a SU hijo bloqueado en read() para siempre, y esos procesos se
         * irian acumulando. Al vencer, read()/SSL_read() devuelven -1 con errno
         * en EAGAIN, que mas abajo se trata como "este cliente no hablo". */

        // Este paso es mega importante
        struct timeval espera = { .tv_sec = SEGUNDOS_ESPERA_CLIENTE, .tv_usec = 0 };
        if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &espera, sizeof(espera)) < 0) {
            perror("setsockopt SO_RCVTIMEO");
        }

        Conexion conexion = {client_socket, NULL};
        // Handshake
        if (ctx != NULL) {
            SSL *ssl = SSL_new(ctx);
            if (ssl == NULL) {
                fprintf(stderr, "Error al crear el puntero SSL.\n");
                ERR_print_errors_fp(stderr);
                close(client_socket);
                _exit(1);
            }
            if (!SSL_set_fd(ssl, conexion.fd)) {
                fprintf(stderr, "Error al conectar el puntero al descriptor de archivo.\n");
                ERR_print_errors_fp(stderr);
                SSL_free(ssl);
                close(client_socket);
                _exit(1);
            }
            if (SSL_accept(ssl) <= 0) {
                fprintf(stderr, "Error al ejecutar el handshake.\n");
                ERR_print_errors_fp(stderr);
                SSL_free(ssl);
                close(client_socket);
                _exit(1);
            }
            conexion.ssl = ssl;
        }
        
        // Ensamblar la peticion: TCP es un flujo, read() puede entregarla en varios pedazos.
        // Se acumula hasta encontrar el "\r\n\r\n" que cierra los encabezados.
        size_t total = 0;
        char *fin_encabezados = NULL;
        while (total < sizeof(entrada) - 1) {
            errno = 0;
            ssize_t bytes_leidos = conexion_leer(&conexion, entrada + total, sizeof(entrada) - 1 - total);

            /* EAGAIN/EWOULDBLOCK aqui no es un fallo: es el SO_RCVTIMEO
             * venciendo porque el cliente no mando nada. Tipico del preconnect
             * de los navegadores. Se cierra en silencio y el hijo termina; si
             * se reportara como error llenaria la bitacora sin motivo. */
            if (bytes_leidos < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                conexion_cerrar(&conexion);
                _exit(0);
            }

            if (bytes_leidos < 0 && conexion.ssl == NULL) {
                perror("read");
                break;
            } else if (bytes_leidos < 0 && conexion.ssl != NULL) {
                fprintf(stderr, "Error al leer la entrada del cliente sobre TLS.\n");
                ERR_print_errors_fp(stderr);
                break;
            }
            if (bytes_leidos == 0) break; // El cliente cerro la conexion
            total += bytes_leidos;
            entrada[total] = '\0';
            fin_encabezados = strstr(entrada, "\r\n\r\n");
            if (fin_encabezados != NULL) break; // Ya estan los encabezados completos
        }

        if (fin_encabezados == NULL) {
            fprintf(stderr, "Peticion incompleta o mas grande que el buffer (%zu bytes), se descarta\n", total);
            conexion_cerrar(&conexion);
            _exit(0);
        }

        printf("Entrada: %s", entrada);
        size_t largo_salida = 0;
        char *respuesta = procesar_entrada(entrada, configs, cantidad_configs, reglas, cantidad_reglas, &largo_salida, env_dir_cache);
        if (respuesta != NULL) {
            ssize_t escritos = conexion_escribir(&conexion, respuesta, largo_salida);
            if (escritos < 0 || (size_t)escritos != largo_salida) {
                fprintf(stderr, "La respuesta se entrego incompleta: %zd de %zu bytes\n",
                    escritos, largo_salida);
            }
            free(respuesta);
        }
        // Una peticion por conexion: se responde y se cierra
        conexion_cerrar(&conexion);

        /* El hijo termina aqui. Nunca vuelve al accept: eso es del padre.
         * fflush porque _exit no vacia el buffer de stdout. */
        fflush(stdout);
        _exit(0);
    }
    SSL_CTX_free(ctx);
    close(server_socket);
    return 0;
}