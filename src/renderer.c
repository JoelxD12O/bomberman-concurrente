/*
 * renderer.c — P4: dibujar el juego en la terminal (BONUS, sección 3).
 * Abigail, en la semana 3.
 *
 * Se activa con:  ./bomberman tests/caso_ejemplo 20 -r
 * P0 lo despierta al final de cada tick.
 *
 * TODO(Abigail), semana 3:
 *   - dibujar el mapa con colores
 *   - mostrar tick, vidas, puntaje y el temporizador de cada bomba
 *   - que la pantalla no parpadee: printf("\033[H") vuelve el cursor arriba
 *     a la izquierda y se dibuja encima, en vez de borrar la pantalla
 */
#include <stdio.h>
#include "procs.h"

void run_renderer(shared_t *sh)
{
    while (1) {
        sem_wait(&sh->sem_render);        // dormir hasta que P0 termine el tick
        if (is_game_over(sh)) {
            break;
        }
        /* TODO(Abigail): dibujar aquí */
    }
}
