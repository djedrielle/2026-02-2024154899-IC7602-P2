/* BLOQUE 2 — Memoria dinamica: malloc, realloc, free
 *
 *   make 02-memoria && ./02-memoria
 *
 * AddressSanitizer esta activo: si deja memoria sin liberar, el programa
 * se lo reporta al final aunque todas las pruebas pasen. Un ejercicio
 * no esta terminado hasta que no haya fugas.
 *
 * Reglas del bloque:
 *   - Siempre revise si malloc/realloc devolvio NULL.
 *   - Quien pide la memoria decide quien la libera. Lea los comentarios.
 */
#include "pruebas.h"
#include <stdlib.h>
#include <string.h>

/* 1. Devuelve una copia en el heap de los n enteros de src.
 *    Si n == 0 devuelve NULL. El que llama debe liberar con free(). */
int *copiar_arreglo(const int *src, size_t n) {
    if (n == 0) return NULL;
    int *p = malloc(n * sizeof(*p));
    for (size_t i = 0; i < n; i++) {
        *(p + i) = *(src + i);
    }
    return p;
}

/* 2. Devuelve un bloque nuevo con los na enteros de a seguidos de los
 *    nb de b. Si na + nb == 0 devuelve NULL. El que llama libera. */
int *concatenar(const int *a, size_t na, const int *b, size_t nb) {
    if (na + nb == 0) return NULL;
    int *p = malloc((na + nb) * sizeof(*p));
    for (size_t i = 0; i < na; i++) {
        *(p + i) = *(a + i);
    }
    for (size_t i = 0; i < nb; i++) {
        *(p + na + i) = *(b + i);
    }
    return p;
}

/* 3. Devuelve una cadena nueva con s repetida 'veces' veces.
 *    repetir("ab", 3) -> "ababab"
 *    Si veces <= 0 devuelve una cadena vacia "" (igual hay que liberarla).
 *    El resultado debe terminar en '\0'. El que llama libera. */
char *repetir(const char *s, int veces) {
    char *vacia = malloc(1);
    if (vacia == NULL) return NULL;
    vacia[0] = '\0';
    if (veces <= 0) return vacia;
    int cant_chars = 0;
    while (*(s + cant_chars) != '\0') {
        cant_chars++;
    }
    if (cant_chars == 0) return vacia;
    char *p = malloc(cant_chars * veces + 1);
    int desplazamiento = 0;
    for (int i = 0; i < veces; i++) {
        for (int v = 0; v < cant_chars; v++) {
            *(p + v + desplazamiento) = *(s + v);
        }
        desplazamiento += cant_chars;
    }
    *(p + veces * cant_chars) = '\0'; 
    return p;
}

/* 4. Reserva una matriz de filas x columnas inicializada en ceros, como
 *    un arreglo de punteros a filas. Acceso: m[i][j].
 *    Si alguna reserva falla, libere lo ya reservado y devuelva NULL. */
int **crear_matriz(size_t filas, size_t cols) {
    int **puntero_principal = malloc(filas * sizeof(*puntero_principal));
    if (puntero_principal == NULL) return NULL;
    for (int i = 0; i < filas; i++) {
        int *p = calloc(cols, sizeof(*p));
        if (p == NULL) {
            for (size_t j = 0; j < i; j++) free(*(puntero_principal + j));
            free(puntero_principal);
            return NULL;
        }
        *(puntero_principal + i) = p;
    }
    return puntero_principal;
}

/* 5. Libera una matriz creada por crear_matriz. Debe tolerar m == NULL. */
void liberar_matriz(int **m, size_t filas) {
    if (m == NULL) return;
    for (int i = 0; i < filas; i++) {
        free(*(m + i));
    }
    free(m);
}

/* 6. Arreglo dinamico que crece solo.
 *    Agrega 'valor' al final. Si *n == *cap no cabe: duplique la capacidad
 *    con realloc (si *cap es 0, arranque en 4) y actualice *cap.
 *    Actualiza *arr y *n. Devuelve 1 si ok, 0 si realloc fallo.
 *    Si realloc falla, el arreglo original debe quedar intacto. */
int agregar(int **arr, size_t *n, size_t *cap, int valor) {
    /* TODO */
    return 0;
}

/* ------------------------- pruebas ------------------------- */

static int igual_arr(const int *a, const int *b, size_t n) {
    for (size_t i = 0; i < n; i++) if (a[i] != b[i]) return 0;
    return 1;
}

int main(void) {
    bloque("1. copiar_arreglo");
    {
        int orig[] = {5, 6, 7};
        int *c = copiar_arreglo(orig, 3);
        check("devuelve memoria", c != NULL);
        if (c) {
            check("copia el contenido", igual_arr(c, orig, 3));
            check("es un bloque distinto", c != orig);
            c[0] = 99;
            check("modificar la copia no toca el original", orig[0] == 5);
            free(c);
        }
        check("n = 0 devuelve NULL", copiar_arreglo(orig, 0) == NULL);
    }

    bloque("2. concatenar");
    {
        int a[] = {1, 2}, b[] = {3, 4, 5};
        int esperado[] = {1, 2, 3, 4, 5};
        int *r = concatenar(a, 2, b, 3);
        check("une los dos arreglos", r != NULL && igual_arr(r, esperado, 5));
        free(r);

        int *r2 = concatenar(a, 2, NULL, 0);
        check("segundo vacio", r2 != NULL && igual_arr(r2, a, 2));
        free(r2);

        check("ambos vacios -> NULL", concatenar(NULL, 0, NULL, 0) == NULL);
    }

    bloque("3. repetir");
    {
        char *r = repetir("ab", 3);
        check("repetir(\"ab\",3) == \"ababab\"", r != NULL && strcmp(r, "ababab") == 0);
        free(r);

        char *v = repetir("xyz", 0);
        check("veces 0 -> cadena vacia", v != NULL && strcmp(v, "") == 0);
        free(v);

        char *n = repetir("", 5);
        check("cadena vacia repetida -> vacia", n != NULL && strcmp(n, "") == 0);
        free(n);

        char *u = repetir("hola", 1);
        check("una sola vez", u != NULL && strcmp(u, "hola") == 0);
        free(u);
    }

    bloque("4-5. matriz");
    {
        int **m = crear_matriz(3, 4);
        check("devuelve memoria", m != NULL);
        if (m) {
            int ceros = 1;
            for (size_t i = 0; i < 3; i++)
                for (size_t j = 0; j < 4; j++)
                    if (m[i][j] != 0) ceros = 0;
            check("arranca en ceros", ceros);

            m[2][3] = 42;
            m[0][0] = 7;
            check("m[2][3] guarda 42", m[2][3] == 42);
            check("las filas son independientes", m[0][3] == 0 && m[0][0] == 7);
            liberar_matriz(m, 3);
        }
        liberar_matriz(NULL, 0);
        check("liberar_matriz(NULL) no revienta", 1);
    }

    bloque("6. agregar (arreglo dinamico)");
    {
        int   *arr = NULL;
        size_t n = 0, cap = 0;

        check("primer agregar funciona", agregar(&arr, &n, &cap, 10) == 1);
        check("n pasa a 1", n == 1);
        check("cap arranca en 4", cap == 4);
        check("guarda el valor", arr != NULL && arr[0] == 10);

        for (int i = 1; i < 4; i++) agregar(&arr, &n, &cap, 10 + i);
        check("4 elementos sin crecer", n == 4 && cap == 4);

        agregar(&arr, &n, &cap, 14);
        check("el quinto duplica la capacidad", cap == 8);
        check("n llega a 5", n == 5);

        int esperado[] = {10, 11, 12, 13, 14};
        check("conserva todo en orden", igual_arr(arr, esperado, 5));

        for (int i = 0; i < 100; i++) agregar(&arr, &n, &cap, i);
        check("aguanta 105 elementos", n == 105 && cap >= 105);
        check("el elemento 104 es correcto", arr[104] == 99);

        free(arr);
    }

    return resumen();
}
