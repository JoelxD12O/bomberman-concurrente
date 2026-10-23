/*
 * parser.c — interpreta UNA línea de un archivo de movimientos (sección 6). Joel.
 *
 * Lo usan P1 (bomber_moves.txt) y P2 (enemy_i_moves.txt).
 * Líneas desconocidas -> INS_INVALID: se descartan SIN consumir turno y se reportan.
 */
#include <stdio.h>
#include <string.h>
#include "procs.h"

instruction parse_instruction(const char *line)
{
    instruction ins = { INS_INVALID, 0 };

    /* TODO(Joel):
     *   - quitar '\n' / '\r' y espacios al final
     *   - comparar con "UP", "DOWN", "LEFT", "RIGHT", "BOMB"   (strcmp)
     *   - "SET_PRIORITY <n>" -> sscanf(line, "SET_PRIORITY %d", &ins.value)
     *   - cualquier otra cosa -> INS_INVALID
     * Probarlo solo con un main de prueba antes de integrarlo. */
    (void)line;
    return ins;
}
