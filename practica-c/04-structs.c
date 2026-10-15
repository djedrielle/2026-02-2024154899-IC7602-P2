/* BLOQUE 4 — Structs, punteros a struct y listas enlazadas
 *
 *   make 04-structs && ./04-structs
 *
 * Temas: paso por valor vs por puntero, el operador ->, quien es dueno
 * de la memoria dentro de un struct, y punteros a puntero (Nodo **).
 */
#include "pruebas.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    int x, y;
} Punto;

typedef struct Nodo {
    int           valor;
    struct Nodo  *sig;
} Nodo;

typedef struct {
    char *nombre;   /* el struct es DUENO de esta memoria */
    int   edad;
} Persona;

/* 1. Devuelve un Punto nuevo con la suma. No modifica a ni b. */
Punto sumar(Punto a, Punto b) {
    /* TODO */
    Punto r = {0, 0};
    return r;
}

/* 2. Mueve el punto apuntado por p. Tolera p == NULL. */
void mover(Punto *p, int dx, int dy) {
    /* TODO */
}

/* 3. Agrega valor AL FINAL de la lista.
 *    Devuelve la cabeza de la lista resultante.
 *    Si cabeza es NULL, la lista nueva es el nodo creado.
 *    Devuelve la cabeza original si falla malloc. */
Nodo *lista_agregar(Nodo *cabeza, int valor) {
    /* TODO */
    return cabeza;
}

/* 4. Cuenta los nodos. Lista vacia -> 0. */
size_t lista_largo(const Nodo *cabeza) {
    /* TODO */
    return 0;
}

/* 5. Devuelve el PRIMER nodo cuyo valor sea 'valor', o NULL si no esta. */
Nodo *lista_buscar(Nodo *cabeza, int valor) {
    /* TODO */
    return NULL;
}

/* 6. Elimina la primera ocurrencia de 'valor' y libera ese nodo.
 *    Recibe Nodo** porque puede tener que cambiar la cabeza.
 *    Devuelve 1 si elimino algo, 0 si no lo encontro. */
int lista_eliminar(Nodo **cabeza, int valor) {
    /* TODO */
    return 0;
}

/* 7. Invierte la lista reusando los nodos que ya existen
 *    (no cree nodos nuevos, no use malloc). Devuelve la nueva cabeza. */
Nodo *lista_invertir(Nodo *cabeza) {
    /* TODO */
    return cabeza;
}

/* 8. Libera todos los nodos. Tolera NULL. */
void lista_liberar(Nodo *cabeza) {
    /* TODO */
}

/* 9. Inicializa p con una COPIA PROPIA del nombre en el heap.
 *    Devuelve 1 si ok, 0 si fallo. Si falla, p->nombre debe quedar NULL. */
int persona_init(Persona *p, const char *nombre, int edad) {
    /* TODO */
    return 0;
}

/* 10. Libera lo que el struct posee y lo deja en estado seguro
 *     (nombre en NULL). Tolera p == NULL y que se llame dos veces. */
void persona_liberar(Persona *p) {
    /* TODO */
}

/* ------------------------- pruebas ------------------------- */

static int lista_igual(const Nodo *c, const int *esp, size_t n) {
    size_t i = 0;
    for (; c && i < n; c = c->sig, i++)
        if (c->valor != esp[i]) return 0;
    return i == n && c == NULL;
}

static Nodo *construir(const int *vals, size_t n) {
    Nodo *c = NULL;
    for (size_t i = 0; i < n; i++) c = lista_agregar(c, vals[i]);
    return c;
}

int main(void) {
    bloque("1-2. Punto");
    {
        Punto a = {1, 2}, b = {10, 20};
        Punto s = sumar(a, b);
        check("suma correcta", s.x == 11 && s.y == 22);
        check("no modifica los originales", a.x == 1 && b.y == 20);

        mover(&a, 5, -1);
        check("mover modifica por puntero", a.x == 6 && a.y == 1);
        mover(NULL, 1, 1);
        check("mover(NULL) no revienta", 1);
    }

    bloque("3-4. agregar y largo");
    {
        check("lista vacia mide 0", lista_largo(NULL) == 0);

        Nodo *l = NULL;
        l = lista_agregar(l, 7);
        check("primer nodo", l != NULL && l->valor == 7 && l->sig == NULL);
        check("largo 1", lista_largo(l) == 1);

        l = lista_agregar(l, 8);
        l = lista_agregar(l, 9);
        int esp[] = {7, 8, 9};
        check("agrega AL FINAL en orden", lista_igual(l, esp, 3));
        check("largo 3", lista_largo(l) == 3);
        lista_liberar(l);
    }

    bloque("5. buscar");
    {
        int v[] = {4, 5, 6, 5};
        Nodo *l = construir(v, 4);
        Nodo *n = lista_buscar(l, 5);
        check("encuentra el 5", n != NULL && n->valor == 5);
        check("devuelve el PRIMERO", n == l->sig);
        check("no encuentra el 99", lista_buscar(l, 99) == NULL);
        check("buscar en lista vacia", lista_buscar(NULL, 1) == NULL);
        lista_liberar(l);
    }

    bloque("6. eliminar");
    {
        int v[] = {1, 2, 3};

        Nodo *l = construir(v, 3);
        check("elimina del medio", lista_eliminar(&l, 2) == 1);
        int e1[] = {1, 3};
        check("queda 1,3", lista_igual(l, e1, 2));
        lista_liberar(l);

        l = construir(v, 3);
        check("elimina la cabeza", lista_eliminar(&l, 1) == 1);
        int e2[] = {2, 3};
        check("la cabeza se actualiza", lista_igual(l, e2, 2));
        lista_liberar(l);

        l = construir(v, 3);
        check("elimina el ultimo", lista_eliminar(&l, 3) == 1);
        int e3[] = {1, 2};
        check("queda 1,2", lista_igual(l, e3, 2));
        check("no encuentra el 99", lista_eliminar(&l, 99) == 0);
        lista_liberar(l);

        l = construir(v, 1);
        check("elimina el unico nodo", lista_eliminar(&l, 1) == 1);
        check("la lista queda NULL", l == NULL);

        check("eliminar en lista vacia", lista_eliminar(&l, 1) == 0);
    }

    bloque("7. invertir");
    {
        int v[] = {1, 2, 3, 4};
        Nodo *l = construir(v, 4);
        l = lista_invertir(l);
        int e[] = {4, 3, 2, 1};
        check("invierte 4 nodos", lista_igual(l, e, 4));
        lista_liberar(l);

        int u[] = {9};
        Nodo *s = construir(u, 1);
        s = lista_invertir(s);
        check("un solo nodo", lista_igual(s, u, 1));
        lista_liberar(s);

        check("invertir NULL da NULL", lista_invertir(NULL) == NULL);
    }

    bloque("9-10. Persona (quien es dueno de la memoria)");
    {
        char buffer[32];
        strcpy(buffer, "Ana");

        Persona p;
        check("init devuelve 1", persona_init(&p, buffer, 30) == 1);
        check("guarda la edad", p.edad == 30);
        check("guarda el nombre", p.nombre != NULL && strcmp(p.nombre, "Ana") == 0);
        check("el nombre es copia propia", p.nombre != buffer);

        strcpy(buffer, "XXX");
        check("cambiar el origen no afecta la copia", strcmp(p.nombre, "Ana") == 0);

        persona_liberar(&p);
        check("liberar deja nombre en NULL", p.nombre == NULL);
        persona_liberar(&p);
        check("liberar dos veces no revienta", 1);
        persona_liberar(NULL);
        check("liberar(NULL) no revienta", 1);
    }

    return resumen();
}
