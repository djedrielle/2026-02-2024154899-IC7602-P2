/* BLOQUE 5 — Operadores de bits
 *
 *   make 05-bits && ./05-bits
 *
 * Convencion: el bit 0 es el MENOS significativo (el de la derecha).
 *
 *   valor 0b00001010 = 10
 *          ^      ^
 *       bit 7    bit 0
 *
 * Operadores:  &  (and)   |  (or)   ^  (xor)   ~  (not)
 *              << (corrimiento izquierda)   >> (corrimiento derecha)
 */
#include "pruebas.h"
#include <stdint.h>

/* 1. Devuelve v con el bit i puesto en 1. */
unsigned poner_bit(unsigned v, int i) {
    /* TODO */
    return v;
}

/* 2. Devuelve v con el bit i puesto en 0. */
unsigned quitar_bit(unsigned v, int i) {
    /* TODO */
    return v;
}

/* 3. Devuelve v con el bit i invertido. */
unsigned alternar_bit(unsigned v, int i) {
    /* TODO */
    return v;
}

/* 4. Devuelve 1 si el bit i esta encendido, 0 si no.
 *    Ojo: debe devolver exactamente 1, no cualquier valor distinto de 0. */
int leer_bit(unsigned v, int i) {
    /* TODO */
    return 0;
}

/* 5. Cuenta cuantos bits en 1 tiene v. */
int contar_bits(unsigned v) {
    /* TODO */
    return 0;
}

/* 6. Invierte el orden de los 2 bytes. Esto es exactamente lo que hay que
 *    hacer para pasar de orden de red (big endian) a orden de maquina.
 *    0x1234 -> 0x3412 */
uint16_t voltear16(uint16_t v) {
    /* TODO */
    return v;
}

/* 7. Lo mismo con 4 bytes.
 *    0x11223344 -> 0x44332211 */
uint32_t voltear32(uint32_t v) {
    /* TODO */
    return v;
}

/* 8. Extrae 'cantidad' bits de v empezando en el bit 'desde', y los
 *    devuelve alineados a la derecha.
 *    Asuma 1 <= cantidad <= 16.
 *
 *    extraer(0b11010110, 1, 3) -> los bits 1,2,3 -> 0b011 -> 3 */
unsigned extraer_campo(unsigned v, int desde, int cantidad) {
    /* TODO */
    return 0;
}

/* 9. Devuelve v con esos 'cantidad' bits a partir de 'desde' reemplazados
 *    por los bits bajos de 'nuevo'. El resto de v no cambia.
 *    Asuma 1 <= cantidad <= 16. */
unsigned poner_campo(unsigned v, int desde, int cantidad, unsigned nuevo) {
    /* TODO */
    return v;
}

/* ------------------------- pruebas ------------------------- */

int main(void) {
    bloque("1-4. bits sueltos");
    {
        check("poner bit 0 en 0 -> 1", poner_bit(0u, 0) == 1u);
        check("poner bit 3 en 0 -> 8", poner_bit(0u, 3) == 8u);
        check("poner un bit ya puesto no cambia", poner_bit(8u, 3) == 8u);
        check("poner bit 31", poner_bit(0u, 31) == 0x80000000u);

        check("quitar bit 1 de 0b111 -> 0b101", quitar_bit(7u, 1) == 5u);
        check("quitar un bit ya apagado no cambia", quitar_bit(5u, 1) == 5u);

        check("alternar enciende", alternar_bit(0u, 2) == 4u);
        check("alternar apaga", alternar_bit(4u, 2) == 0u);

        check("leer bit encendido da 1", leer_bit(0b1000u, 3) == 1);
        check("leer bit apagado da 0", leer_bit(0b1000u, 2) == 0);
        check("leer devuelve 1, no 8", leer_bit(0b1000u, 3) == 1);
    }

    bloque("5. contar_bits");
    {
        check("0 tiene 0 bits", contar_bits(0u) == 0);
        check("1 tiene 1 bit", contar_bits(1u) == 1);
        check("0b1011 tiene 3", contar_bits(0b1011u) == 3);
        check("0xFF tiene 8", contar_bits(0xFFu) == 8);
        check("0xFFFFFFFF tiene 32", contar_bits(0xFFFFFFFFu) == 32);
        check("0x80000000 tiene 1", contar_bits(0x80000000u) == 1);
    }

    bloque("6-7. orden de bytes");
    {
        check("voltear16(0x1234) == 0x3412", voltear16(0x1234) == 0x3412);
        check("voltear16(0x00FF) == 0xFF00", voltear16(0x00FF) == 0xFF00);
        check("voltear16 dos veces vuelve al original", voltear16(voltear16(0xABCD)) == 0xABCD);

        check("voltear32(0x11223344) == 0x44332211", voltear32(0x11223344u) == 0x44332211u);
        check("voltear32(0x000000FF) == 0xFF000000", voltear32(0x000000FFu) == 0xFF000000u);
        check("voltear32 dos veces vuelve al original", voltear32(voltear32(0xDEADBEEFu)) == 0xDEADBEEFu);
    }

    bloque("8. extraer_campo");
    {
        check("bits 1..3 de 0b11010110 -> 3", extraer_campo(0b11010110u, 1, 3) == 3u);
        check("bits 0..3 de 0xAB -> 0xB", extraer_campo(0xABu, 0, 4) == 0xBu);
        check("bits 4..7 de 0xAB -> 0xA", extraer_campo(0xABu, 4, 4) == 0xAu);
        check("1 solo bit", extraer_campo(0b100u, 2, 1) == 1u);
        check("no arrastra bits de arriba", extraer_campo(0xFFFFu, 0, 4) == 0xFu);
    }

    bloque("9. poner_campo");
    {
        check("mete 0xC en los bits 0..3 de 0xAB", poner_campo(0xABu, 0, 4, 0xCu) == 0xACu);
        check("mete 0x1 en los bits 4..7 de 0xAB", poner_campo(0xABu, 4, 4, 0x1u) == 0x1Bu);
        check("ignora los bits de sobra de 'nuevo'", poner_campo(0xABu, 0, 4, 0xFFCu) == 0xACu);
        check("poner ceros limpia el campo", poner_campo(0xFFu, 0, 4, 0u) == 0xF0u);
        check("no toca el resto", poner_campo(0xF0F0u, 8, 4, 0x5u) == 0xF5F0u);
    }

    return resumen();
}
