#ifndef HTTP_H
#define HTTP_H

#include <stddef.h>

char *http_get(const char *host, const char *puerto, const char *ruta, size_t *largo_salida, int proxy);

#endif