/*
 * scheduler.c — P0 scheduler_process (secciones 2.1, 5, 9, 10, 11, 12). Joel.
 *
 * ESTADO ACTUAL (semana 1): un solo hilo. Ya aplica prioridades, Round Robin,
 * buzones de prioridad, eventos de daño, vidas, puntaje y fin del juego.
 *
 * META (semana 2): reemplazar por 4 hilos:
 *   tick_thread       incrementa global_tick
 *   scheduler_thread  decide quién juega este tick (prioridad + Round Robin)
 *   signal_thread     hace sem_post al semáforo que corresponde
 *   event_thread      procesa colisiones/daño -> vidas, enemy_alive, score, game_over
 * Los 4 hilos se coordinan entre ellos con semáforos/condvars internos de P0
 * (no hace falta que vivan en shared_t).
 */
#include <stdio.h>
#include "procs.h"

static const char *result_name(int r)
{
    switch (r) {
    case RESULT_MAX_TICKS: return "se alcanzó max_ticks";
    case RESULT_DEFEAT:    return "DERROTA: Bomberman sin vidas";
    case RESULT_VICTORY:   return "VICTORIA: todos los enemigos eliminados";
    case RESULT_NO_MOVES:  return "sin movimientos y sin bombas activas";
    default:               return "en curso";
    }
}

/* Sección 10: P1 y P2 no cambian su prioridad; dejan una solicitud y P0 la valida. */
static void process_priority_requests(shared_t *sh)
{
    pthread_mutex_lock(&sh->prio_mtx);

    if (sh->priority_request_active) {
        int p = sh->pending_priority_bomber;
        if (p >= PRIO_MIN && p <= PRIO_MAX) {
            printf("[P0] prioridad de P1: %d -> %d\n", sh->prioridad_bomber, p);
            sh->prioridad_bomber = p;
        } else {
            printf("[P0] prioridad %d para P1 fuera de rango, descartada\n", p);
        }
        sh->priority_request_active = 0;
    }

    if (sh->enemy_priority_request_active) {
        int p = sh->pending_priority_enemy;
        if (p >= PRIO_MIN && p <= PRIO_MAX) {
            printf("[P0] prioridad de P2: %d -> %d\n", sh->prioridad_enemy, p);
            sh->prioridad_enemy = p;
        } else {
            printf("[P0] prioridad %d para P2 fuera de rango, descartada\n", p);
        }
        sh->enemy_priority_request_active = 0;
    }

    pthread_mutex_unlock(&sh->prio_mtx);
}

/* Sección 9: devuelve 1 = turno de P1, 0 = turno de P2.
 * Gana la mayor prioridad; si empatan, se alternan (Round Robin). */
static int choose_turn(shared_t *sh, int *rr_next_bomber)
{
    pthread_mutex_lock(&sh->prio_mtx);
    int pb = sh->prioridad_bomber;
    int pe = sh->prioridad_enemy;
    pthread_mutex_unlock(&sh->prio_mtx);

    if (pb > pe) return 1;
    if (pe > pb) return 0;

    int bomber = *rr_next_bomber;
    *rr_next_bomber = !*rr_next_bomber;
    return bomber;
}

/* Secciones 8.4 y 12: P0 es el único que cambia vidas, enemigos vivos y puntaje. */
static void process_events(shared_t *sh)
{
    /* blocks_destroyed lo escribe P3 bajo map_mtx: se lee primero (map va antes en el orden) */
    pthread_mutex_lock(&sh->map_mtx);
    int blocks = sh->blocks_destroyed;
    pthread_mutex_unlock(&sh->map_mtx);

    pthread_mutex_lock(&sh->events_mtx);
    pthread_mutex_lock(&sh->state_mtx);

    int hit = 0;
    if (sh->collision_detected) {
        printf("[P0] colisión con el enemigo %d en el tick %d\n",
               sh->collision_enemy_id + 1, sh->collision_tick);
        hit = 1;
        sh->collision_detected = 0;
    }
    if (sh->blast_hit_bomber) {
        printf("[P0] explosión alcanzó a Bomberman en el tick %d\n", sh->blast_tick);
        hit = 1;
        sh->blast_hit_bomber = 0;
    }
    /* aunque haya colisión y explosión a la vez, se pierde como máximo 1 vida */
    if (hit && sh->bomber_lives > 0) {
        sh->bomber_lives--;
        printf("[P0] Bomberman pierde una vida, le quedan %d\n", sh->bomber_lives);
    }

    for (int i = 0; i < NUM_ENEMIES; i++) {
        if (sh->blast_hit_enemy[i]) {
            if (sh->enemy_alive[i]) {
                sh->enemy_alive[i] = 0;
                sh->enemies_alive--;
                printf("[P0] enemigo %d eliminado\n", i + 1);
            }
            sh->blast_hit_enemy[i] = 0;
        }
    }

    sh->score = 10 * blocks + 100 * (NUM_ENEMIES - sh->enemies_alive);

    pthread_mutex_unlock(&sh->state_mtx);
    pthread_mutex_unlock(&sh->events_mtx);
}

/* Sección 11: condiciones de fin. */
static void check_end(shared_t *sh)
{
    /* datos de otros procesos, tomando los mutex en el orden de shared.h */
    pthread_mutex_lock(&sh->map_mtx);
    int bombs = sh->active_bombs;
    pthread_mutex_unlock(&sh->map_mtx);

    pthread_mutex_lock(&sh->bomber_mtx);
    int bomber_done = sh->bomber_moves_done;
    pthread_mutex_unlock(&sh->bomber_mtx);

    pthread_mutex_lock(&sh->enemy_mtx);
    int enemy_done = sh->enemy_moves_done;
    pthread_mutex_unlock(&sh->enemy_mtx);

    pthread_mutex_lock(&sh->state_mtx);
    int result = RESULT_RUNNING;
    if (sh->bomber_lives <= 0)
        result = RESULT_DEFEAT;
    else if (sh->enemies_alive <= 0)
        result = RESULT_VICTORY;
    else if (bomber_done && enemy_done && bombs == 0)
        result = RESULT_NO_MOVES;
    else if (sh->global_tick >= sh->max_ticks)
        result = RESULT_MAX_TICKS;

    if (result != RESULT_RUNNING) {
        sh->game_over = 1;
        sh->game_result = result;
        printf("[P0] fin del juego: %s\n", result_name(result));
    }
    pthread_mutex_unlock(&sh->state_mtx);
}

void run_scheduler(shared_t *sh)
{
    int rr_next_bomber = 1;   /* con empate, P1 juega primero (ticks impares) */

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
        printf("[P0] vidas=%d enemigos=%d score=%d\n",
               sh->bomber_lives, sh->enemies_alive, sh->score);
        sem_post(&sh->sem_render);   /* despierta a P4 si existe */
    }

    /* Liberar a todos los que estén bloqueados para que vean game_over y salgan. */
    sem_post(&sh->sem_bomber_turn);
    sem_post(&sh->sem_enemy_turn);
    sem_post(&sh->sem_bomb_turn);
    sem_post(&sh->sem_render);
}
