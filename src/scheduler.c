/*
 * scheduler.c — P0 scheduler_process (secciones 2.1, 5, 9, 10, 11, 12). Joel.
 *
 * ESTADO ACTUAL: versión STUB de un solo hilo. Ya reparte turnos alternando
 * P1/P2 y siempre da turno a P3, para que Abigail, Mariel y Danilo puedan probar desde el día 1.
 *
 * META: reemplazar por 4 hilos:
 *   tick_thread       incrementa global_tick
 *   scheduler_thread  decide quién juega este tick (prioridad + Round Robin)
 *   signal_thread     hace sem_post al semáforo que corresponde
 *   event_thread      procesa colisiones/daño -> vidas, enemy_alive, score, game_over
 * Los 4 hilos se coordinan entre ellos con semáforos/condvars internos de P0
 * (no hace falta que vivan en shared_t).
 */
#include <stdio.h>
#include "procs.h"

static void process_priority_requests(shared_t *sh)
{
    /* TODO(Joel) sección 10: con prio_mtx tomado,
     *   si priority_request_active: validar PRIO_MIN..PRIO_MAX, aplicar a
     *   prioridad_bomber, limpiar la bandera. Lo mismo para el buzón del enemigo. */
    (void)sh;
}

static int choose_turn(shared_t *sh, int *rr_next_bomber)
{
    /* TODO(Joel) sección 9: devolver 1 = turno de P1, 0 = turno de P2.
     *   mayor prioridad gana; empate -> alternar con *rr_next_bomber.
     *   Opcional: aging contra la inanición. */
    int bomber = *rr_next_bomber;
    *rr_next_bomber = !*rr_next_bomber;
    (void)sh;
    return bomber;
}

static void process_events(shared_t *sh)
{
    /* TODO(Joel) sección 8.4 y 12, con events_mtx y state_mtx tomados (en ese orden):
     *   - collision_detected o blast_hit_bomber -> perder UNA vida como máximo
     *   - blast_hit_enemy[i] -> enemy_alive[i] = 0, enemies_alive--
     *   - limpiar todas las banderas
     *   - score = 10 * blocks_destroyed + 100 * (NUM_ENEMIES - enemies_alive) */
    (void)sh;
}

static void check_end(shared_t *sh)
{
    pthread_mutex_lock(&sh->state_mtx);
    if (sh->global_tick >= sh->max_ticks) {
        sh->game_over = 1;
        sh->game_result = RESULT_MAX_TICKS;
    }
    /* TODO(Joel) sección 11: derrota, victoria, y archivos terminados sin bombas. */
    pthread_mutex_unlock(&sh->state_mtx);
}

void run_scheduler(shared_t *sh)
{
    int rr_next_bomber = 1;

    while (!is_game_over(sh)) {
        /* 1. Inicio */
        pthread_mutex_lock(&sh->state_mtx);
        sh->global_tick++;
        pthread_mutex_unlock(&sh->state_mtx);
        process_priority_requests(sh);

        /* 2. Fase de acción: P1 o P2 */
        if (choose_turn(sh, &rr_next_bomber)) {
            printf("[P0] tick %d -> P1\n", sh->global_tick);
            sem_post(&sh->sem_bomber_turn);
        } else {
            printf("[P0] tick %d -> P2\n", sh->global_tick);
            sem_post(&sh->sem_enemy_turn);
        }
        sem_wait(&sh->sem_turn_done);

        /* 3. Fase de bombas: siempre P3 */
        sem_post(&sh->sem_bomb_turn);
        sem_wait(&sh->sem_turn_done);

        /* 4. Cierre */
        process_events(sh);
        check_end(sh);
        print_map(sh);
        sem_post(&sh->sem_render);   /* despierta a P4 si existe */
    }

    /* Liberar a todos los que estén bloqueados para que vean game_over y salgan. */
    sem_post(&sh->sem_bomber_turn);
    sem_post(&sh->sem_enemy_turn);
    sem_post(&sh->sem_bomb_turn);
    sem_post(&sh->sem_render);
}
