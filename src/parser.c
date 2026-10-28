/*
 * parser.c — interpreta UNA línea de un archivo de movimientos (sección 6). Joel.
 *
 * Lo usan P1 (bomber_moves.txt) y P2 (enemy_i_moves.txt).
 * Líneas desconocidas -> INS_INVALID: se descartan SIN consumir turno y se reportan.
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "procs.h"

instruction parse_instruction(const char *line)
{
    instruction ins = { INS_INVALID, 0 };

    /* line es const: trabajamos sobre una copia local */
    char buf[64];
    snprintf(buf, sizeof(buf), "%s", line);

    /* quitar salto de línea (\n o \r\n de Windows) y espacios al final */
    buf[strcspn(buf, "\r\n")] = '\0';
    int len = (int)strlen(buf);
    while (len > 0 && isspace((unsigned char)buf[len - 1]))
        buf[--len] = '\0';

    if      (strcmp(buf, "UP")    == 0) ins.type = INS_UP;
    else if (strcmp(buf, "DOWN")  == 0) ins.type = INS_DOWN;
    else if (strcmp(buf, "LEFT")  == 0) ins.type = INS_LEFT;
    else if (strcmp(buf, "RIGHT") == 0) ins.type = INS_RIGHT;
    else if (strcmp(buf, "BOMB")  == 0) ins.type = INS_BOMB;
    else {
        /* "SET_PRIORITY <n>": sscanf devuelve cuántos campos leyó.
         * El %c extra detecta basura después del número ("SET_PRIORITY 5x"). */
        int value;
        char extra;
        if (sscanf(buf, "SET_PRIORITY %d %c", &value, &extra) == 1) {
            ins.type = INS_SET_PRIORITY;
            ins.value = value;
        }
    }
    return ins;
}
