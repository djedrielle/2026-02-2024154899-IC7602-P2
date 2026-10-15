/* BLOQUE 3 — Cadenas: el terminador '\0' y todo lo que se rompe por el
 *
 *   make 03-strings && ./03-strings
 *
 * Reglas del bloque:
 *   - Prohibido usar strlen, strcpy, strcmp, strdup, strstr de la libreria.
 *     Las esta reimplementando. Si puede usar malloc/free y memcpy.
 *   - Toda cadena que devuelva en el heap debe terminar en '\0'.
 */
#include "pruebas.h"
#include <stdlib.h>
#include <string.h>   /* solo para las pruebas, no para sus funciones */

/* 1. Largo de la cadena, sin contar el '\0'. */
size_t mi_strlen(const char *s) {
    /* TODO */
    return 0;
}

/* 2. Copia src (incluyendo el '\0') dentro de dst. Devuelve dst.
 *    Asuma que dst tiene espacio suficiente. */
char *mi_strcpy(char *dst, const char *src) {
    /* TODO */
    return dst;
}

/* 3. Compara a y b.
 *    Devuelve < 0 si a va antes, 0 si son iguales, > 0 si a va despues.
 *    El valor exacto no importa, solo el signo. */
int mi_strcmp(const char *a, const char *b) {
    /* TODO */
    return 0;
}

/* 4. Copia de s en el heap. Devuelve NULL si s es NULL o si falla malloc.
 *    El que llama libera con free(). */
char *mi_strdup(const char *s) {
    /* TODO */
    return NULL;
}

/* 5. Devuelve 1 si s empieza con prefijo, 0 si no.
 *    Toda cadena empieza con "". */
int empieza_con(const char *s, const char *prefijo) {
    /* TODO */
    return 0;
}

/* 6. Quita espacios, tabs y saltos de linea del inicio y del final,
 *    MODIFICANDO s en el lugar. Devuelve un puntero al primer caracter
 *    util (que puede no ser s).
 *    "  hola  " -> devuelve puntero a 'h', y esa cadena vale "hola"
 *    "     "    -> devuelve una cadena vacia */
char *recortar(char *s) {
    /* TODO */
    return s;
}

/* 7. Parte s en pedazos separados por 'sep', MODIFICANDO s: reemplaza cada
 *    separador por '\0' y guarda en partes[] punteros al inicio de cada
 *    pedazo. Llena a lo sumo 'max' punteros.
 *    Devuelve cuantos pedazos guardo.
 *
 *    "a,b,c"  sep=','  -> 3 pedazos: "a" "b" "c"
 *    "a,,b"   sep=','  -> 3 pedazos: "a" ""  "b"
 *    ""       sep=','  -> 1 pedazo:  ""
 *    Si hay mas pedazos que max, se queda en max. */
int partir(char *s, char sep, char *partes[], int max) {
    /* TODO */
    return 0;
}

/* ------------------------- pruebas ------------------------- */

int main(void) {
    bloque("1. mi_strlen");
    {
        check("\"hola\" mide 4", mi_strlen("hola") == 4);
        check("\"\" mide 0", mi_strlen("") == 0);
        char con_nulo[8] = {'a', 'b', '\0', 'c', 'd', '\0'};
        check("se detiene en el primer '\\0'", mi_strlen(con_nulo) == 2);
    }

    bloque("2. mi_strcpy");
    {
        char dst[16];
        memset(dst, 'X', sizeof dst);
        char *r = mi_strcpy(dst, "perro");
        check("copia el contenido", strcmp(dst, "perro") == 0);
        check("devuelve dst", r == dst);
        check("pone el '\\0'", dst[5] == '\0');
        check("no toca lo que sigue", dst[6] == 'X');

        char vacio[4];
        memset(vacio, 'Y', sizeof vacio);
        mi_strcpy(vacio, "");
        check("copiar cadena vacia", vacio[0] == '\0' && vacio[1] == 'Y');
    }

    bloque("3. mi_strcmp");
    {
        check("iguales -> 0", mi_strcmp("casa", "casa") == 0);
        check("\"\" vs \"\" -> 0", mi_strcmp("", "") == 0);
        check("\"abc\" < \"abd\"", mi_strcmp("abc", "abd") < 0);
        check("\"abd\" > \"abc\"", mi_strcmp("abd", "abc") > 0);
        check("prefijo es menor", mi_strcmp("ab", "abc") < 0);
        check("mas largo es mayor", mi_strcmp("abc", "ab") > 0);
        check("\"\" < \"a\"", mi_strcmp("", "a") < 0);
        /* ojo: los chars con bit alto deben compararse como unsigned */
        check("maneja bytes > 127", mi_strcmp("\xFF", "a") > 0);
    }

    bloque("4. mi_strdup");
    {
        const char *orig = "cadena";
        char *c = mi_strdup(orig);
        check("devuelve memoria", c != NULL);
        if (c) {
            check("mismo contenido", strcmp(c, orig) == 0);
            check("bloque distinto", (const char *)c != orig);
            free(c);
        }
        char *v = mi_strdup("");
        check("duplica la vacia", v != NULL && v[0] == '\0');
        free(v);
        check("NULL -> NULL", mi_strdup(NULL) == NULL);
    }

    bloque("5. empieza_con");
    {
        check("\"hola mundo\" empieza con \"hola\"", empieza_con("hola mundo", "hola") == 1);
        check("no empieza con \"mundo\"", empieza_con("hola mundo", "mundo") == 0);
        check("todo empieza con \"\"", empieza_con("hola", "") == 1);
        check("prefijo mas largo que s", empieza_con("ho", "hola") == 0);
        check("iguales", empieza_con("hola", "hola") == 1);
        check("\"\" no empieza con \"a\"", empieza_con("", "a") == 0);
    }

    bloque("6. recortar");
    {
        char a[] = "   hola   ";
        char *ra = recortar(a);
        check("quita antes y despues", strcmp(ra, "hola") == 0);

        char b[] = "sin nada";
        check("sin espacios queda igual", strcmp(recortar(b), "sin nada") == 0);

        char c[] = "      ";
        check("solo espacios -> vacia", strcmp(recortar(c), "") == 0);

        char d[] = "\t\n  dato  \n\t";
        check("tabs y saltos de linea", strcmp(recortar(d), "dato") == 0);

        char e[] = "a b";
        check("no toca los espacios de adentro", strcmp(recortar(e), "a b") == 0);

        char f[] = "";
        check("cadena vacia", strcmp(recortar(f), "") == 0);
    }

    bloque("7. partir");
    {
        char s1[] = "a,b,c";
        char *p1[8];
        int n1 = partir(s1, ',', p1, 8);
        check("\"a,b,c\" da 3", n1 == 3);
        check("pedazos correctos",
              n1 == 3 && strcmp(p1[0], "a") == 0 && strcmp(p1[1], "b") == 0
              && strcmp(p1[2], "c") == 0);

        char s2[] = "a,,b";
        char *p2[8];
        int n2 = partir(s2, ',', p2, 8);
        check("separadores seguidos dan pedazo vacio",
              n2 == 3 && strcmp(p2[1], "") == 0);

        char s3[] = "";
        char *p3[8];
        check("cadena vacia da 1 pedazo", partir(s3, ',', p3, 8) == 1);

        char s4[] = "uno";
        char *p4[8];
        int n4 = partir(s4, ',', p4, 8);
        check("sin separadores da 1", n4 == 1 && strcmp(p4[0], "uno") == 0);

        char s5[] = "a,b,c,d,e";
        char *p5[3];
        check("respeta el maximo", partir(s5, ',', p5, 3) == 3);

        char s6[] = "k=v";
        char *p6[2];
        int n6 = partir(s6, '=', p6, 2);
        check("sirve con otro separador",
              n6 == 2 && strcmp(p6[0], "k") == 0 && strcmp(p6[1], "v") == 0);
    }

    return resumen();
}
