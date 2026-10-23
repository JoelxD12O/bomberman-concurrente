/*
 * enemy.c — P2 enemy_process (secciones 2.3, 5, 6). Mariel.
 *
 * ESTADO ACTUAL: stub. Espera su turno, no hace nada y avisa a P0.
 *
 * META: 8 hilos.
 *   enemy_controller_thread   espera sem_enemy_turn y despierta a los enemy_thread VIVOS
 *   enemy_thread_1..4         cada uno lee SU archivo enemy_<i>_moves.txt, 1 instrucción por turno
 *   bomber_tracker_thread     copia bomber_x/bomber_y a una copia local (bomber_last_position)
 *   collision_thread          al final del turno compara posiciones -> evento collision_*
 *   enemy_publisher_thread    copia posiciones locales a enemy_x[] / enemy_y[]
 *
 * Reglas:
 *   - Enemigo muerto (enemy_alive[i] == 0, lo escribe P0) no consume instrucciones.
 *   - Dos enemigos pueden estar en la misma celda.
 *   - P2 no toca bomber_lives ni enemy_alive: solo publica el evento de colisión.
 *   - BOMB no es válido para enemigos.
 *   - SET_PRIORITY -> buzón pending_priority_enemy; si dos lo piden en el mismo
 *     turno, gana el enemigo de mayor número.
 */
#include <stdio.h>
#include "procs.h"

/* ---- Sincronización interna de P2 ----
 * TODO(Mariel): un sem_t "go" por enemigo (controller -> enemy_i) y un sem_t "done"
 *          (enemy_i -> controller). Alternativa a investigar: pthread_barrier_t.
 *          Un pthread_mutex_t para el arreglo local enemy_positions[]. */

typedef struct {
    shared_t *sh;
    int id;           /* 0..3 */
} enemy_arg;

__attribute__((unused))
static void *enemy_thread(void *arg)
{
    enemy_arg *ea = arg;
    /* TODO(Mariel): abrir <data_dir>/enemy_<id+1>_moves.txt; loop {
     *   esperar "go"; si game_over salir;
     *   leer instrucción, mover validando con is_walkable (map_mtx)
     *   sem_post("done")
     * } */
    (void)ea;
    return NULL;
}

__attribute__((unused))
static void *enemy_controller_thread(void *arg)
{
    shared_t *sh = arg;
    /* TODO(Mariel): loop {
     *   sem_wait(&sh->sem_enemy_turn); if (is_game_over(sh)) { despertar a todos; break; }
     *   contar vivos, post "go" a cada vivo, esperar "done" de cada vivo
     *   pedir colisión + publicación
     *   sem_post(&sh->sem_turn_done);
     * } */
    (void)sh;
    return NULL;
}

__attribute__((unused))
static void *bomber_tracker_thread(void *arg)   { (void)arg; return NULL; } /* TODO(Mariel) */
__attribute__((unused))
static void *collision_thread(void *arg)        { (void)arg; return NULL; } /* TODO(Mariel) */
__attribute__((unused))
static void *enemy_publisher_thread(void *arg)  { (void)arg; return NULL; } /* TODO(Mariel) */

void run_enemies(shared_t *sh)
{
    /* SEMANA 1: abrir los 4 archivos enemy_<i>_moves.txt aquí (un FILE* por enemigo). */

    /* SEMANA 2 — TODO(Mariel): reemplazar este stub por pthread_create de los 8 hilos + pthread_join. */
    for (;;) {
        sem_wait(&sh->sem_enemy_turn);           /* NO TOCAR: espera su turno */
        if (is_game_over(sh)) break;             /* NO TOCAR */
        printf("[P2] turno en tick %d (stub)\n", sh->global_tick);

        /* >>> SEMANA 1 — TODO(Mariel): tu lógica va AQUÍ, sin hilos <<<
         *   for cada enemigo i con sh->enemy_alive[i] == 1:
         *     leer UNA línea de su archivo -> parse_instruction
         *     si is_walkable(nueva celda) -> sh->enemy_x[i] / sh->enemy_y[i]
         *   después: si algún enemigo vivo está en (bomber_x, bomber_y):
         *     sh->collision_detected = 1; collision_tick; collision_enemy_id
         *   si todos los vivos terminaron su archivo: sh->enemy_moves_done = 1 */

        sem_post(&sh->sem_turn_done);            /* NO TOCAR: avisa "terminé" */
    }
}
