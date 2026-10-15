/* Mini framework de pruebas. No hay que modificar este archivo. */
#ifndef PRUEBAS_H
#define PRUEBAS_H

#include <stdio.h>
#include <string.h>

static int pruebas_ok = 0;
static int pruebas_mal = 0;

static void bloque(const char *titulo) {
    printf("\n\033[1m%s\033[0m\n", titulo);
}

static void check(const char *desc, int cond) {
    if (cond) {
        pruebas_ok++;
        printf("  \033[32m OK \033[0m %s\n", desc);
    } else {
        pruebas_mal++;
        printf("  \033[31mFALLA\033[0m %s\n", desc);
    }
}

static int resumen(void) {
    printf("\n\033[1m%d correctas, %d fallidas\033[0m\n", pruebas_ok, pruebas_mal);
    if (pruebas_mal == 0) printf("\033[32mTodo pasa.\033[0m\n");
    return pruebas_mal ? 1 : 0;
}

#endif
