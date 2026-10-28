/*
 * parser.c — convierte UNA línea del archivo de movimientos en una instrucción
 * (sección 6). Joel.
 *
 * Lo usan P1 (bomber_moves.txt) y P2 (enemy_1_moves.txt ... enemy_4_moves.txt).
 *
 *   "LEFT"             ->  tipo INS_LEFT
 *   "SET_PRIORITY 25"  ->  tipo INS_SET_PRIORITY, valor 25
 *   "hola"             ->  tipo INS_INVALID  (quien lo usa la salta y lee otra)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "procs.h"

/* Borra el salto de línea y los espacios del final del texto.
 * Ejemplo: "LEFT  \r\n" queda como "LEFT".
 * (\r aparece cuando el archivo se guardó en Windows) */
void limpiar_linea(char *texto)
{
    int largo = strlen(texto);
    while (largo > 0) {
        char ultimo = texto[largo - 1];
        if (ultimo == '\n' || ultimo == '\r' || ultimo == ' ') {
            texto[largo - 1] = '\0';   /* cortar el texto aquí */
            largo--;
        } else {
            break;
        }
    }
}

/* Devuelve 1 si el texto son solo dígitos (ej: "25"), 0 si no (ej: "", "5x"). */
static int es_numero(const char *texto)
{
    if (texto[0] == '\0') {
        return 0;
    }
    for (int i = 0; texto[i] != '\0'; i++) {
        if (texto[i] < '0' || texto[i] > '9') {
            return 0;
        }
    }
    return 1;
}

instruction parse_instruction(const char *line)
{
    instruction ins;
    ins.type = INS_INVALID;
    ins.value = 0;

    /* copiamos la línea para poder limpiarla (line no se puede modificar) */
    char texto[64];
    strncpy(texto, line, sizeof(texto) - 1);
    texto[sizeof(texto) - 1] = '\0';
    limpiar_linea(texto);

    if (strcmp(texto, "UP") == 0) {
        ins.type = INS_UP;
    } else if (strcmp(texto, "DOWN") == 0) {
        ins.type = INS_DOWN;
    } else if (strcmp(texto, "LEFT") == 0) {
        ins.type = INS_LEFT;
    } else if (strcmp(texto, "RIGHT") == 0) {
        ins.type = INS_RIGHT;
    } else if (strcmp(texto, "BOMB") == 0) {
        ins.type = INS_BOMB;
    } else if (strncmp(texto, "SET_PRIORITY ", 13) == 0) {
        /* los primeros 13 caracteres son "SET_PRIORITY "; el número va después */
        char *numero = &texto[13];
        if (es_numero(numero)) {
            ins.type = INS_SET_PRIORITY;
            ins.value = atoi(numero);   /* atoi: texto -> int, como stoi en C++ */
        }
    }
    return ins;
}
