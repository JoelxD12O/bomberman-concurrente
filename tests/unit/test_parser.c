/*
 * test_parser.c — prueba de parse_instruction sin correr el juego.
 * Correr: make test
 */
#include <stdio.h>
#include "procs.h"

static int fallos = 0;

static void check(const char *line, ins_type tipo, int valor)
{
    instruction ins = parse_instruction(line);
    int ok = ins.type == tipo && (tipo != INS_SET_PRIORITY || ins.value == valor);
    if (!ok) fallos++;
    printf("%s  \"", ok ? "OK   " : "FALLA");
    for (int i = 0; line[i] != '\0'; i++) {    /* mostrar \n y \r como texto */
        if (line[i] == '\n') {
            printf("\\n");
        } else if (line[i] == '\r') {
            printf("\\r");
        } else {
            printf("%c", line[i]);
        }
    }
    printf("\"\n");
}

int main(void)
{
    check("UP\n",              INS_UP, 0);
    check("DOWN\r\n",          INS_DOWN, 0);      /* archivo guardado en Windows */
    check("LEFT",              INS_LEFT, 0);      /* última línea sin \n */
    check("RIGHT   \n",        INS_RIGHT, 0);     /* espacios al final */
    check("BOMB\n",            INS_BOMB, 0);
    check("SET_PRIORITY 25\n", INS_SET_PRIORITY, 25);
    check("SET_PRIORITY 8",    INS_SET_PRIORITY, 8);
    check("SET_PRIORITY\n",    INS_INVALID, 0);   /* falta el número */
    check("SET_PRIORITY 5x\n", INS_INVALID, 0);   /* basura después */
    check("up\n",              INS_INVALID, 0);   /* minúsculas no valen */
    check("JUMP\n",            INS_INVALID, 0);
    check("\n",                INS_INVALID, 0);   /* línea vacía */

    printf("\n%s (%d fallos)\n", fallos ? "HAY ERRORES" : "TODO BIEN", fallos);
    return fallos != 0;
}
