/* Diagnostico: prediga la salida ANTES de compilar.
 * Compilar y correr:  make diag
 */
#include <stdio.h>
#include <string.h>

static void bloque_1(void) {
    int  a = 5;
    int *p = &a;
    *p += 2;
    int b = a++;
    printf("B1: a=%d b=%d *p=%d\n", a, b, *p);
}

static void bloque_2(void) {
    int arr[5] = {10, 20, 30, 40, 50};
    int *p = arr + 1;
    printf("B2: %d %d %d %ld\n", *p, *(p + 2), p[-1], p - arr);
}

static void bloque_3(void) {
    char s[10] = "hola";
    printf("B3: strlen=%zu sizeof=%zu s[4]=%d\n", strlen(s), sizeof(s), s[4]);
}

static void suma_mal(int x)  { x += 100; }
static void suma_bien(int *x) { *x += 100; }

static void bloque_4(void) {
    int n = 1;
    suma_mal(n);
    suma_bien(&n);
    printf("B4: n=%d\n", n);
}

static void bloque_5(void) {
    char  texto[] = "abc";
    char *lit     = "abc";
    printf("B5: sizeof(texto)=%zu sizeof(lit)=%zu\n", sizeof(texto), sizeof(lit));
}

static void bloque_6(void) {
    unsigned char c = 0x0F;
    printf("B6: %d %d %d %d\n", c << 4, c >> 2, c & 0x09, c ^ 0xFF);
}

int main(void) {
    bloque_1(); bloque_2(); bloque_3();
    bloque_4(); bloque_5(); bloque_6();
    return 0;
}
