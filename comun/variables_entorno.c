#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "variables_entorno.h"

const char *env_texto(const char *nombre, const char *respaldo) {
    const char *v = getenv(nombre);
    return (v && *v) ? v : respaldo;
}

int env_entero(const char *nombre, int respaldo) {
    const char *v = getenv(nombre);
    if (!v || !*v) return respaldo;

    errno = 0;
    char *sobrante;
    long n = strtol(v, &sobrante, 10);

    if (errno != 0 || *sobrante != '\0' || n < 0 || n > 65535) {
        fprintf(stderr, "Aviso: %s=\"%s\" no es valido, se usa %d\n", nombre, v, respaldo);
        return respaldo;
    }
    return (int)n;
}