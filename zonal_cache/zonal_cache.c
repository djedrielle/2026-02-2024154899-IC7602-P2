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

#include "../comun/http_cliente.h"
#include "../comun/variables_entorno.h"

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
} ConfigDominio;

typedef struct {
    char clave[256];
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

int buscar_config(char *host, ConfigDominio *configs, int cantidad_configs, ConfigDominio *config_destino) {
    for (int i = 0; i < cantidad_configs; i++) {
        if (!strcmp(host, configs[i].dominio)) {
            *config_destino = configs[i];
            return 0;
        }
    }
    return -1;
}

int inventario(EntradaCache **entradas_cache, size_t *tamanno_total, int *cantidad_entradas, char *dir_directorio) {
    DIR *dir = opendir(dir_directorio);
    if (dir == NULL) {
        perror("inventario");
        return -1;
    }
    struct dirent *dp;
    errno = 0;
    int largo;
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
            char hash[17];
            snprintf(hash, sizeof(hash), "%.*s", largo - 5, dp->d_name);
            snprintf(ptr_entradas_cache[i].hash, 17, "%s", hash);
            ptr_entradas_cache[i].meta = m;
            i++;
        }
    }
    closedir(dir);
    *entradas_cache = ptr_entradas_cache;
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

char *procesar_entrada(char* buffer_entrada, ConfigDominio *configs, int cantidad_configs, size_t *largo_salida) {
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
    printf("Ruta: %s\n", ruta);

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
    printf("Host: %s\n", host);

    // Consultar la configuracion del dominio
    ConfigDominio config;
    if (buscar_config(host, configs, cantidad_configs, &config) == -1) {
        fprintf(stderr, "No se encontró una configuración para ese host.\n");
        free(metodo);
        free(ruta);
        free(host);
        return NULL;
    }

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
        if (stat(dir_directorio, &st)) {
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
        snprintf(buffer_from_int_to_char, sizeof(buffer_from_int_to_char), "%d", config.origen_puerto);
        char *respuesta_origen = http_get(config.origen_host, buffer_from_int_to_char, ruta, largo_salida, 1, NULL);
        free(metodo);
        free(ruta);
        free(host);
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
        EntradaCache *entradas_cache;
        int cantidad_entradas = 0;
        if (inventario(&entradas_cache, &tamanno_total, &cantidad_entradas, dir_directorio) == -1) {
            return NULL;
        }
        while (tamanno_total + *largo_salida > config.tamanno_maximo) {
            int indice_victima = elegir_victima(entradas_cache, config.algoritmo, cantidad_entradas);
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

        if ((escribir_meta(dir_punto_meta, clave, tam_bytes, config.ttl)) == -1) {
            printf("Error al intentar escribir el .meta");
            free(respuesta_origen);
            return NULL;
        }

        printf("Salida Apache Server: %.*s", (int)*largo_salida, respuesta_origen);

        return respuesta_origen;
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

int obtener_algoritmo(char *algoritmo_texto) {
    static const char *NOMBRES_ALG[] = { "LRU", "LFU", "FIFO", "MRU", "RANDOM" };
    for (size_t i = 0; i < sizeof(NOMBRES_ALG)/sizeof(NOMBRES_ALG[0]); i++) {
        if (!strcmp(NOMBRES_ALG[i], algoritmo_texto)) {
            return i;
        }
    }
    return -1;
}

int cargar_configs(ConfigDominio *configs, int *cantidad_configs, int env_cant_max_dominios) {
    // Leer variables de entorno
    const char *env_extra_headers = env_texto("EXTRA_HEADERS", "x-config-key: ABCDEFG\r\n");
    const char *env_node_api_host = env_texto("NODE_API_HOST", "127.0.0.1");
    int env_node_api_port = env_entero("NODE_API_PORT", 3000);
    size_t largo_salida = 0;

    char buffer_from_int_to_char[20];
    snprintf(buffer_from_int_to_char, sizeof(buffer_from_int_to_char), "%d", env_node_api_port);
    char *respuesta_api = http_get(env_node_api_host, buffer_from_int_to_char, "/api/config", &largo_salida, 0, env_extra_headers);
    if (respuesta_api == NULL) {
        fprintf(stderr, "config: no se pudo contactar el API en %s:%d (revise NODE_API_HOST y NODE_API_PORT, y que el API este arriba)\n",
            env_node_api_host, env_node_api_port);
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
            fprintf(stderr, "config: el dominio '%s' declara el algoritmo '%s', que no es ninguno de LRU, LFU, FIFO, MRU, RANDOM\n",
                dominio, algoritmo_texto);
            free(respuesta_api);
            return -1;
        }
        enum AlgReemplazo algoritmo = int_algoritmo;
        int origen_puerto = extraer_int(inicio_config, "\"origen_puerto\":");
        long tamanno_maximo = extraer_int(inicio_config, "\"tamanno_maximo\":");
        long ttl = extraer_int(inicio_config, "\"ttl\":");
        int requiere_auth = extraer_booleano(inicio_config, "\"requiere_auth\":");
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
        configs[i].origen_puerto = origen_puerto;
        configs[i].algoritmo = algoritmo;
        configs[i].tamanno_maximo = tamanno_maximo;
        configs[i].ttl = ttl;
        configs[i].requiere_auth = requiere_auth;

        *fin_config = '}';
        inicio_config = fin_config + 2;
    }
    free(respuesta_api);
    return 0;
}


int main(void) {
    srand(time(NULL));
    // Leer variables de entorno
    int env_max_tam_entrada = env_entero("MAX_TAM_ENTRADA", 1024);
    int env_puerto_zonal_cache = env_entero("ZONAL_PUERTO_IN", 9777);
    int env_cant_max_dominios = env_entero("CANT_MAX_DOMINIOS", 16);
    

    int cantidad_configs;
    ConfigDominio configs[env_cant_max_dominios];
    if (cargar_configs(configs, &cantidad_configs, env_cant_max_dominios) == -1) {
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

        // Ensamblar la peticion: TCP es un flujo, read() puede entregarla en varios pedazos.
        // Se acumula hasta encontrar el "\r\n\r\n" que cierra los encabezados.
        size_t total = 0;
        char *fin_encabezados = NULL;
        while (total < sizeof(entrada) - 1) {
            ssize_t bytes_leidos = read(client_socket, entrada + total, sizeof(entrada) - 1 - total);
            if (bytes_leidos < 0) {
                perror("read");
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
            close(client_socket);
            continue;
        }

        printf("Entrada: %s", entrada);
        size_t largo_salida = 0;
        char *respuesta = procesar_entrada(entrada, configs, cantidad_configs, &largo_salida);
        if (respuesta != NULL) {
            send(client_socket, respuesta, largo_salida, MSG_NOSIGNAL);
            free(respuesta);
        }

        // Una peticion por conexion: se responde y se cierra
        close(client_socket);
    }

    close(server_socket);

    return 0;
}