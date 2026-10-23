/*
 * bomb.c — P3 bomb_process (secciones 2.4, 8). Danilo.
 *
 * ESTADO ACTUAL: stub. Recibe turno en TODOS los ticks, no hace nada y avisa a P0.
 *
 * META: 3 hilos.
 *   bomb_controller_thread  espera sem_bomb_turn, coordina a los otros dos y
 *                           atiende la solicitud de bomba pendiente
 *   fuse_thread             decrementa el temporizador de cada bomba
 *   explosion_thread        limpia fuego, propaga explosiones, destruye bloques, publica daño
 *
 * Orden EXACTO en cada tick (sección 8.2):
 *   1. Limpiar   '*' del tick anterior -> 'O'          (explosion_thread)
 *   2. Avanzar   timer-- en cada bomba activa          (fuse_thread)
 *   3. Explotar  bombas con timer == 0, con cadena     (explosion_thread)
 *   4. Registrar bomb_request si active_bombs < MAX_BOMBS y la celda no tiene bomba;
 *                siempre limpiar bomb_request_active   (bomb_controller_thread)
 *
 * Explosión (8.3): centro + 4 direcciones hasta BOMB_RANGE.
 *   'X' corta sin cambiar | '#' se destruye (blocks_destroyed++) y corta |
 *   'Q' de otra bomba -> explota en el mismo tick | celdas alcanzadas -> '*'
 * Daño (8.4): comparar posiciones PUBLICADAS de bomber y enemigos vivos con el fuego.
 */
#include <stdio.h>
#include "procs.h"

typedef struct {
    int x, y, timer, active;
} bomb;

/* Tabla interna de P3. TODO(Danilo): protegerla con un pthread_mutex_t propio. */
__attribute__((unused))
static bomb bombs[MAX_BOMBS];

__attribute__((unused))
static void *fuse_thread(void *arg)            { (void)arg; return NULL; } /* TODO(Danilo) */
__attribute__((unused))
static void *explosion_thread(void *arg)       { (void)arg; return NULL; } /* TODO(Danilo) */
__attribute__((unused))
static void *bomb_controller_thread(void *arg) { (void)arg; return NULL; } /* TODO(Danilo) */

void run_bombs(shared_t *sh)
{
    /* SEMANA 2 — TODO(Danilo): reemplazar este stub por pthread_create de los 3 hilos + pthread_join.
     * Pista: el controller despierta a cada hilo con su propio sem_t y espera
     * a que termine antes del siguiente paso, así se respeta el orden 1-2-3-4. */
    for (;;) {
        sem_wait(&sh->sem_bomb_turn);            /* NO TOCAR: espera su turno */
        if (is_game_over(sh)) break;             /* NO TOCAR */

        /* >>> SEMANA 1 — TODO(Danilo): tu lógica va AQUÍ, sin hilos, en este orden <<<
         *   1. todo '*' del mapa -> 'O'
         *   2. timer-- de cada bomba activa en bombs[]
         *   3. las que llegan a 0 explotan: cruz de BOMB_RANGE, 'X' corta,
         *      '#' se rompe (sh->blocks_destroyed++) y corta, otra 'Q' explota también.
         *      Si (bomber_x, bomber_y) quedó en fuego -> sh->blast_hit_bomber = 1
         *      Si un enemigo vivo quedó en fuego    -> sh->blast_hit_enemy[i] = 1
         *   4. si sh->bomb_request_active: crear bomba (si hay espacio) y poner 'Q';
         *      siempre sh->bomb_request_active = 0
         *   mantener sh->active_bombs al día */

        sem_post(&sh->sem_turn_done);            /* NO TOCAR: avisa "terminé" */
    }
}
