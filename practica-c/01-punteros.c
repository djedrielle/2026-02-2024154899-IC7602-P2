/* BLOQUE 1 — Punteros y aritmetica de punteros
 *
 * Implemente las 6 funciones. No modifique main() ni las pruebas.
 *   make 01-punteros && ./01-punteros
 *
 * Regla del bloque: prohibido usar indices [] en las funciones 4, 5 y 6.
 * Solo aritmetica de punteros (*(p + i), p++, etc).
 */
#include "pruebas.h"
#include <stddef.h>

/* 1. Intercambia los valores apuntados por a y b. */
void intercambiar(int *a, int *b) {
    int c = *b;
    *b = *a;
    *a = c;
}

/* 2. Devuelve cociente y resto de a/b a traves de los punteros de salida.
 *    Si b == 0, no escribe nada y devuelve 0. Si todo bien, devuelve 1.
 *    Cualquiera de los dos punteros de salida puede venir en NULL:
 *    en ese caso simplemente no se escribe ese resultado. */
int dividir(int a, int b, int *cociente, int *resto) {
    if (b == 0) return 0;
    if (cociente != NULL) *cociente = a / b;
    if (resto    != NULL) *resto    = a % b;
    return 1;
}

/* 3. Devuelve un PUNTERO al elemento mayor del arreglo.
 *    Si n == 0 devuelve NULL. Si hay empate, devuelve el primero. */
int *maximo(int *arr, size_t n) {
    int *max = arr;
    for (size_t i = 1; i < n; i++) {
        if (arr[i] > *max) max = arr + i;
    }
    return max;
}

/* 4. Suma delta a cada elemento del arreglo. (sin [] ) */
void incrementar_todos(int *arr, size_t n, int delta) {
    for (int *p = arr; p < arr + n; p++) {
        *p += delta;
    }
}

/* 5. Cuenta cuantos elementos son estrictamente mayores que umbral. (sin [] ) */
size_t contar_mayores(const int *arr, size_t n, int umbral) {
    size_t contador = 0;
    for (const int *p = arr; p < arr + n; p++) {
        if (*p > umbral) {
            contador += 1;
        }
    }
    return contador;
}

/* 6. Invierte el arreglo en el lugar, usando dos punteros que se acercan
 *    desde los extremos. (sin [] ) */
void invertir(int *arr, size_t n) {
    int *izq = arr;
    int *der = arr + n - 1;     // cuidado si n == 0
    int a;
    int b;
    while (izq < der) {
        a = *izq;
        b = *der;
        *izq = b;
        *der = a;
        izq++;
        der--;
    }
}

/* ------------------------- pruebas ------------------------- */

static int igual_arr(const int *a, const int *b, size_t n) {
    for (size_t i = 0; i < n; i++) if (a[i] != b[i]) return 0;
    return 1;
}

int main(void) {
    bloque("1. intercambiar");
    {
        int x = 3, y = 8;
        intercambiar(&x, &y);
        check("intercambia 3 y 8", x == 8 && y == 3);
        intercambiar(&x, &x);
        check("intercambiar consigo mismo no rompe", x == 8);
    }

    bloque("2. dividir");
    {
        int c = -1, r = -1;
        check("17/5 devuelve 1", dividir(17, 5, &c, &r) == 1);
        check("17/5 -> cociente 3", c == 3);
        check("17/5 -> resto 2", r == 2);

        c = r = -1;
        check("division entre 0 devuelve 0", dividir(9, 0, &c, &r) == 0);
        check("division entre 0 no toca las salidas", c == -1 && r == -1);

        c = -1;
        check("resto NULL no rompe", dividir(20, 6, &c, NULL) == 1 && c == 3);
        r = -1;
        check("cociente NULL no rompe", dividir(20, 6, NULL, &r) == 1 && r == 2);
    }

    bloque("3. maximo");
    {
        int a[] = {4, 19, 7, 19, 2};
        int *m = maximo(a, 5);
        check("encuentra el 19", m != NULL && *m == 19);
        check("devuelve el PRIMER 19 (indice 1)", m == a + 1);
        check("arreglo vacio -> NULL", maximo(a, 0) == NULL);

        int b[] = {-9, -3, -40};
        int *mb = maximo(b, 3);
        check("funciona con negativos", mb != NULL && *mb == -3);
    }

    bloque("4. incrementar_todos");
    {
        int a[] = {1, 2, 3};
        int esperado[] = {11, 12, 13};
        incrementar_todos(a, 3, 10);
        check("suma 10 a cada uno", igual_arr(a, esperado, 3));

        int vacio[1] = {77};
        incrementar_todos(vacio, 0, 5);
        check("n = 0 no escribe nada", vacio[0] == 77);
    }

    bloque("5. contar_mayores");
    {
        int a[] = {1, 50, 3, 50, 9};
        check("mayores que 10 son 2", contar_mayores(a, 5, 10) == 2);
        check("mayores que 50 son 0 (estricto)", contar_mayores(a, 5, 50) == 0);
        check("mayores que 0 son 5", contar_mayores(a, 5, 0) == 5);
        check("n = 0 da 0", contar_mayores(a, 0, 0) == 0);
    }

    bloque("6. invertir");
    {
        int a[] = {1, 2, 3, 4};
        int ea[] = {4, 3, 2, 1};
        invertir(a, 4);
        check("largo par", igual_arr(a, ea, 4));

        int b[] = {1, 2, 3, 4, 5};
        int eb[] = {5, 4, 3, 2, 1};
        invertir(b, 5);
        check("largo impar (el del medio se queda)", igual_arr(b, eb, 5));

        int c[] = {9};
        invertir(c, 1);
        check("un solo elemento", c[0] == 9);
    }

    return resumen();
}
