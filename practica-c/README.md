# Practica de GNU C — Sesion 1: logica del lenguaje

Ejercicios con pruebas automaticas. Usted implementa las funciones marcadas
con `/* TODO */`; las pruebas al final de cada archivo verifican el resultado.
No hay que modificar `main()` ni `pruebas.h`.

## Como trabajar

```bash
make 01-punteros && ./01-punteros
```

Repita hasta ver `Todo pasa.` Para correr todos los bloques de una:

```bash
make test
```

## Bloques

| Archivo | Tema | Pruebas |
| --- | --- | --- |
| `00-diagnostico.c` | Prediga la salida antes de correrlo (`make diag`) | — |
| `01-punteros.c` | Punteros, aritmetica de punteros, parametros de salida | 22 |
| `02-memoria.c` | malloc, realloc, free, matrices, arreglo dinamico | 27 |
| `03-strings.c` | Reimplementar strlen/strcpy/strcmp/strdup, trim, split | 40 |
| `04-structs.c` | Structs, `->`, listas enlazadas, `Nodo **`, ownership | 34 |
| `05-bits.c` | and/or/xor/not, corrimientos, endianness, campos de bits | 33 |

Orden sugerido: 01 -> 02 -> 03 -> 04 -> 05. Cada bloque asume el anterior.

## Sanitizers

El Makefile compila con `-fsanitize=address,undefined`. Eso detecta en tiempo
de ejecucion:

- lecturas y escrituras fuera de rango
- uso de memoria ya liberada
- `free` doble
- **fugas de memoria** (se reportan al salir)
- desbordamientos y corrimientos invalidos

Si ve un reporte de ASan, lea la primera linea y el primer `#0` del stack:
ahi esta el archivo y la linea exacta. Un bloque no esta terminado si pasa
las pruebas pero deja fugas.

## Depurar

```bash
gdb ./02-memoria
```

Dentro de gdb: `break copiar_arreglo`, `run`, `next`, `print *p`, `bt`.
