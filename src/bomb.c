/*
 * bomb.c — P3: bombas y explosiones (secciones 2.4 y 8). Danilo.
 *
 * P3 juega en TODOS los ticks, después de P1 o P2.
 *
 * En cada turno hace 4 pasos, en este orden (sección 8.2):
 *   1. Limpiar:   el fuego '*' del tick anterior vuelve a ser 'O'
 *   2. Avanzar:   a cada bomba activa se le resta 1 a su temporizador
 *   3. Explotar:  las bombas que llegaron a 0 explotan
 *   4. Registrar: si P1 pidió una bomba, se crea (si se puede)
 *
 * Cómo explota una bomba (sección 8.3):
 *   - el fuego va en cruz (arriba, abajo, izquierda, derecha) hasta BOMB_RANGE celdas
 *   - una pared 'X' frena el fuego y no se rompe
 *   - un bloque '#' se rompe (sh->blocks_destroyed++) y frena el fuego
 *   - si el fuego toca otra bomba 'Q', esa también explota (reacción en cadena)
 *   - las celdas con fuego se marcan con '*'
 *
 * SEMANA 1: escribir la lógica dentro del while de abajo (sin hilos, sin mutex).
 * SEMANA 2: dividirla en 3 hilos:
 *   bomb_controller_thread  recibe el turno, coordina a los otros dos y crea bombas
 *   fuse_thread             baja los temporizadores
 *   explosion_thread        limpia el fuego, hace las explosiones y avisa el daño
 */
#include <stdio.h>
#include "procs.h"

/* Una bomba: dónde está, cuántos ticks le faltan y si está en uso */
typedef struct {
    int x;
    int y;
    int timer;
    int active;    /* 1 = bomba en el mapa, 0 = lugar libre en la tabla */
} bomb;

/* Tabla de bombas de P3 (como máximo MAX_BOMBS al mismo tiempo) */
bomb bombs[MAX_BOMBS];

void run_bombs(shared_t *sh)
{
    while (1) {
        sem_wait(&sh->sem_bomb_turn);     // NO TOCAR: dormir hasta que P0 dé el turno
        if (is_game_over(sh)) {           // NO TOCAR: si el juego terminó, salir
            break;
        }

        /* >>> SEMANA 1 — TODO(Danilo): tu código va AQUÍ, en este orden <<<
         *
         * 1. Recorrer todo sh->map_grid[y][x]: cada '*' se cambia por 'O'.
         *
         * 2. Para cada bomba de la tabla con active == 1:  timer--
         *
         * 3. Cada bomba con timer == 0 explota:
         *      - marcar '*' en su celda y en cruz hasta BOMB_RANGE celdas
         *        (parar en 'X'; en '#' romperlo, sumar blocks_destroyed y parar;
         *         si hay otra 'Q', hacer que esa bomba también explote)
         *      - quitarla de la tabla (active = 0) y restar sh->active_bombs
         *      - si Bomberman está en una celda con fuego:
         *            sh->blast_hit_bomber = 1;  sh->blast_tick = sh->global_tick;
         *      - si el enemigo i (vivo) está en una celda con fuego:
         *            sh->blast_hit_enemy[i] = 1;
         *
         * 4. Si sh->bomb_request_active == 1:
         *      - si hay menos de MAX_BOMBS bombas y en esa celda no hay otra bomba:
         *            guardarla en la tabla con timer = BOMB_TIMER,
         *            poner 'Q' en el mapa y sumar sh->active_bombs
         *      - SIEMPRE, se haya creado o no:  sh->bomb_request_active = 0;
         */

        sem_post(&sh->sem_turn_done);     // NO TOCAR: avisar a P0 "terminé mi turno"
    }
}
