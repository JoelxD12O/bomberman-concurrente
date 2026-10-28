/*
 * procs.h — punto de entrada de cada proceso y utilidades comunes.
 */
#ifndef PROCS_H
#define PROCS_H

#include "shared.h"

/* Cada función corre DENTRO del proceso hijo, después del fork. */
void run_scheduler(shared_t *sh);   /* P0 - Joel */
void run_bomber(shared_t *sh);      /* P1 - Abigail */
void run_enemies(shared_t *sh);     /* P2 - Mariel */
void run_bombs(shared_t *sh);       /* P3 - Danilo */
void run_renderer(shared_t *sh);    /* P4 - bonus (Abigail) */

/* map.c — Joel */
int  load_map(shared_t *sh, const char *path);
void print_map(shared_t *sh);
int  is_walkable(shared_t *sh, int x, int y);  /* leer map_grid con map_mtx tomado */

/* parser.c — Joel (lo usan P1 y P2) */
typedef enum {
    INS_UP, INS_DOWN, INS_LEFT, INS_RIGHT,
    INS_BOMB, INS_SET_PRIORITY,
    INS_INVALID
} ins_type;

typedef struct {
    ins_type type;
    int value;          /* solo para SET_PRIORITY */
} instruction;

instruction parse_instruction(const char *line);

/* Borra el salto de línea y los espacios del final: "LEFT \r\n" -> "LEFT" */
void limpiar_linea(char *texto);

/* true si el juego terminó: los hilos deben revisarlo para salir */
int is_game_over(shared_t *sh);

#endif
