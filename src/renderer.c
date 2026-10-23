/*
 * renderer.c — P4 renderer_process (BONUS, sección 3). Abigail cuando termine P1.
 *
 * Se activa con: ./bomberman <caso> <ticks> -r
 * P0 hace sem_post(&sem_render) al cierre de cada tick.
 *
 * TODO(Abigail):
 *   - limpiar pantalla sin parpadeo: "\033[H" (cursor al inicio) en vez de borrar todo
 *   - colores ANSI por símbolo, mostrar bomb_timer[] encima de cada 'Q'
 *   - mostrar tick, vidas, score, prioridades
 *   - leer TODO con los mutex en el orden de shared.h
 */
#include <stdio.h>
#include "procs.h"

void run_renderer(shared_t *sh)
{
    for (;;) {
        sem_wait(&sh->sem_render);
        if (is_game_over(sh)) break;
        /* TODO(Abigail): dibujar aquí */
    }
}
