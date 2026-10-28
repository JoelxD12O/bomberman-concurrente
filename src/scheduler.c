/*
 * scheduler.c — P0, el árbitro (secciones 2.1, 5, 9, 10, 11 y 12). Joel.
 *
 * En cada tick:
 *   1. Inicio:  tick++ y aplicar los pedidos de cambio de prioridad.
 *   2. Acción:  le da el turno a P1 (Bomberman) o a P2 (enemigos) y espera.
 *   3. Bombas:  le da el turno a P3 (bombas) y espera.
 *   4. Cierre:  revisa los avisos (choques, explosiones), resta vidas,
 *               calcula el puntaje y decide si el juego terminó.
 *
 * SEMANA 1: todo en un solo hilo (este archivo).
 * SEMANA 2: dividirlo en 4 hilos: tick_thread, scheduler_thread,
 *           signal_thread y event_thread.
 */
#include <stdio.h>
#include "procs.h"

/* Para los empates: 1 = en el próximo empate juega P1, 0 = juega P2.
 * Empieza en 1 para que P1 juegue en los ticks impares (sección 8.5). */
static int empate_le_toca_a_bomber = 1;

/* Texto del resultado, para imprimirlo */
static const char *nombre_resultado(int resultado)
{
    if (resultado == RESULT_MAX_TICKS) return "se alcanzó el máximo de ticks";
    if (resultado == RESULT_DEFEAT)    return "DERROTA: Bomberman se quedó sin vidas";
    if (resultado == RESULT_VICTORY)   return "VICTORIA: todos los enemigos eliminados";
    if (resultado == RESULT_NO_MOVES)  return "no quedan movimientos ni bombas";
    return "en curso";
}

/* ---------------------------------------------------------------------------
 * Paso 1: pedidos de cambio de prioridad (sección 10)
 * P1 y P2 NO cambian su prioridad solos: dejan un pedido ("buzón") y P0 decide.
 * ------------------------------------------------------------------------- */
static void process_priority_requests(shared_t *sh)
{
    pthread_mutex_lock(&sh->prio_mtx);

    /* pedido de Bomberman */
    if (sh->priority_request_active == 1) {
        int pedida = sh->pending_priority_bomber;
        if (pedida >= PRIO_MIN && pedida <= PRIO_MAX) {
            printf("[P0] prioridad de P1: %d -> %d\n", sh->prioridad_bomber, pedida);
            sh->prioridad_bomber = pedida;
        } else {
            printf("[P0] prioridad %d para P1 fuera de rango (1-49), se ignora\n", pedida);
        }
        sh->priority_request_active = 0;   /* pedido atendido: bajar la bandera */
    }

    /* pedido de los enemigos */
    if (sh->enemy_priority_request_active == 1) {
        int pedida = sh->pending_priority_enemy;
        if (pedida >= PRIO_MIN && pedida <= PRIO_MAX) {
            printf("[P0] prioridad de P2: %d -> %d\n", sh->prioridad_enemy, pedida);
            sh->prioridad_enemy = pedida;
        } else {
            printf("[P0] prioridad %d para P2 fuera de rango (1-49), se ignora\n", pedida);
        }
        sh->enemy_priority_request_active = 0;
    }

    pthread_mutex_unlock(&sh->prio_mtx);
}

/* ---------------------------------------------------------------------------
 * Paso 2: ¿a quién le toca? (sección 9)
 * Devuelve 1 si le toca a P1 (Bomberman) y 0 si le toca a P2 (enemigos).
 * ------------------------------------------------------------------------- */
static int le_toca_a_bomber(shared_t *sh)
{
    pthread_mutex_lock(&sh->prio_mtx);
    int prioridad_bomber = sh->prioridad_bomber;
    int prioridad_enemy  = sh->prioridad_enemy;
    pthread_mutex_unlock(&sh->prio_mtx);

    if (prioridad_bomber > prioridad_enemy) {
        return 1;   /* gana Bomberman */
    }
    if (prioridad_enemy > prioridad_bomber) {
        return 0;   /* ganan los enemigos */
    }

    /* empate: se turnan (Round Robin) */
    if (empate_le_toca_a_bomber == 1) {
        empate_le_toca_a_bomber = 0;   /* el próximo empate es para P2 */
        return 1;
    } else {
        empate_le_toca_a_bomber = 1;   /* el próximo empate es para P1 */
        return 0;
    }
}

/* ---------------------------------------------------------------------------
 * Paso 4a: revisar los avisos de P2 y P3 (secciones 8.4 y 12)
 * P0 es el ÚNICO que cambia vidas, enemigos vivos y puntaje.
 * ------------------------------------------------------------------------- */
static void process_events(shared_t *sh)
{
    /* Los mutex se toman en el orden de shared.h: map -> events -> state */

    /* bloques destruidos (lo escribe P3) */
    pthread_mutex_lock(&sh->map_mtx);
    int bloques = sh->blocks_destroyed;
    pthread_mutex_unlock(&sh->map_mtx);

    pthread_mutex_lock(&sh->events_mtx);
    pthread_mutex_lock(&sh->state_mtx);

    /* ¿Le pasó algo a Bomberman en este tick? */
    int bomber_golpeado = 0;
    if (sh->collision_detected == 1) {
        printf("[P0] el enemigo %d tocó a Bomberman (tick %d)\n",
               sh->collision_enemy_id + 1, sh->collision_tick);
        bomber_golpeado = 1;
        sh->collision_detected = 0;   /* aviso atendido */
    }
    if (sh->blast_hit_bomber == 1) {
        printf("[P0] una explosión alcanzó a Bomberman (tick %d)\n", sh->blast_tick);
        bomber_golpeado = 1;
        sh->blast_hit_bomber = 0;
    }
    /* aunque lo toquen y lo exploten en el mismo tick, pierde UNA sola vida */
    if (bomber_golpeado == 1 && sh->bomber_lives > 0) {
        sh->bomber_lives--;
        printf("[P0] Bomberman pierde una vida, le quedan %d\n", sh->bomber_lives);
    }

    /* ¿Alguna explosión alcanzó a un enemigo? */
    for (int i = 0; i < NUM_ENEMIES; i++) {
        if (sh->blast_hit_enemy[i] == 1) {
            if (sh->enemy_alive[i] == 1) {
                sh->enemy_alive[i] = 0;
                sh->enemies_alive--;
                printf("[P0] enemigo %d eliminado\n", i + 1);
            }
            sh->blast_hit_enemy[i] = 0;
        }
    }

    /* puntaje: 10 por bloque destruido y 100 por enemigo eliminado */
    int enemigos_muertos = NUM_ENEMIES - sh->enemies_alive;
    sh->score = 10 * bloques + 100 * enemigos_muertos;

    pthread_mutex_unlock(&sh->state_mtx);
    pthread_mutex_unlock(&sh->events_mtx);
}

/* ---------------------------------------------------------------------------
 * Paso 4b: ¿terminó el juego? (sección 11)
 * ------------------------------------------------------------------------- */
static void check_end(shared_t *sh)
{
    /* leer lo que publican P1, P2 y P3 (en el orden de shared.h) */
    pthread_mutex_lock(&sh->map_mtx);
    int bombas_activas = sh->active_bombs;
    pthread_mutex_unlock(&sh->map_mtx);

    pthread_mutex_lock(&sh->bomber_mtx);
    int bomber_sin_movimientos = sh->bomber_moves_done;
    pthread_mutex_unlock(&sh->bomber_mtx);

    pthread_mutex_lock(&sh->enemy_mtx);
    int enemigos_sin_movimientos = sh->enemy_moves_done;
    pthread_mutex_unlock(&sh->enemy_mtx);

    pthread_mutex_lock(&sh->state_mtx);

    int resultado = RESULT_RUNNING;
    if (sh->bomber_lives <= 0) {
        resultado = RESULT_DEFEAT;
    } else if (sh->enemies_alive <= 0) {
        resultado = RESULT_VICTORY;
    } else if (bomber_sin_movimientos && enemigos_sin_movimientos && bombas_activas == 0) {
        resultado = RESULT_NO_MOVES;
    } else if (sh->global_tick >= sh->max_ticks) {
        resultado = RESULT_MAX_TICKS;
    }

    if (resultado != RESULT_RUNNING) {
        sh->game_over = 1;
        sh->game_result = resultado;
        printf("[P0] fin del juego: %s\n", nombre_resultado(resultado));
    }

    pthread_mutex_unlock(&sh->state_mtx);
}

/* ---------------------------------------------------------------------------
 * El ciclo principal de P0
 * ------------------------------------------------------------------------- */
void run_scheduler(shared_t *sh)
{
    while (!is_game_over(sh)) {

        /* 1. Inicio */
        pthread_mutex_lock(&sh->state_mtx);
        sh->global_tick++;
        pthread_mutex_unlock(&sh->state_mtx);

        process_priority_requests(sh);

        /* 2. Fase de acción: le doy el turno a P1 o a P2 y espero que termine */
        if (le_toca_a_bomber(sh)) {
            printf("[P0] tick %d -> turno de P1 (Bomberman)\n", sh->global_tick);
            sem_post(&sh->sem_bomber_turn);
        } else {
            printf("[P0] tick %d -> turno de P2 (enemigos)\n", sh->global_tick);
            sem_post(&sh->sem_enemy_turn);
        }
        sem_wait(&sh->sem_turn_done);   /* espero el "terminé" */

        /* 3. Fase de bombas: P3 juega en TODOS los ticks */
        sem_post(&sh->sem_bomb_turn);
        sem_wait(&sh->sem_turn_done);

        /* 4. Cierre */
        process_events(sh);
        check_end(sh);
        print_map(sh);
        printf("[P0] vidas=%d enemigos=%d score=%d\n",
               sh->bomber_lives, sh->enemies_alive, sh->score);
        sem_post(&sh->sem_render);   /* avisa al dibujador P4, si existe */
    }

    /* El juego terminó. Los hijos están dormidos esperando su turno:
     * los despierto para que vean game_over = 1 y terminen. */
    sem_post(&sh->sem_bomber_turn);
    sem_post(&sh->sem_enemy_turn);
    sem_post(&sh->sem_bomb_turn);
    sem_post(&sh->sem_render);
}
