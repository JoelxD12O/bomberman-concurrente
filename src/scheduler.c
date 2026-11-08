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
 * SEMANA 2: P0 tiene 4 hilos (sección 2.1). Cada hilo hace UNA parte del tick
 * y le pasa la "posta" al siguiente con un semáforo interno:
 *
 *   tick_thread ──> scheduler_thread ──> signal_thread ──> event_thread ──┐
 *   (tick++)        (decide P1 o P2)     (da los turnos)   (vidas, fin)   │
 *        ^                                                                │
 *        └──────────────────────── siguiente tick ────────────────────────┘
 *
 * Los semáforos internos fijan el ORDEN de los hilos; los mutex protegen los
 * DATOS que comparten (global_tick, turno_actual, prioridades, vidas...).
 */
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include "procs.h"

/* Para los empates: 1 = en el próximo empate juega P1, 0 = juega P2.
 * Empieza en 1 para que P1 juegue en los ticks impares (sección 8.5).
 * Solo la usa scheduler_thread, por eso no necesita mutex. */
static int empate_le_toca_a_bomber = 1;

/* ---------------------------------------------------------------------------
 * Datos INTERNOS de P0 (no están en la memoria compartida)
 *
 * Solo los usan los 4 hilos de P0, que viven en el mismo proceso. Por eso:
 *   - los semáforos se crean con sem_init(..., 0, ...): el 0 significa
 *     "solo entre hilos de este proceso";
 *   - el mutex es uno normal, sin PTHREAD_PROCESS_SHARED.
 * En cambio, los semáforos y mutex de shared.h los usan procesos distintos
 * (P0, P1, P2, P3): esos sí necesitan el 1 y PTHREAD_PROCESS_SHARED (main.c).
 * ------------------------------------------------------------------------- */

/* La "posta" entre los hilos: cada semáforo despierta al hilo siguiente */
static sem_t sem_toca_tick;      /* event_thread     -> tick_thread      */
static sem_t sem_toca_decidir;   /* tick_thread      -> scheduler_thread */
static sem_t sem_toca_senal;     /* scheduler_thread -> signal_thread    */
static sem_t sem_toca_eventos;   /* signal_thread    -> event_thread     */

/* A quién le toca en este tick: lo escribe scheduler_thread y lo lee
 * signal_thread, así que va protegido con turno_mtx (sección 14). */
#define TURNO_BOMBER 1
#define TURNO_ENEMY  2
static int turno_actual = 0;
static pthread_mutex_t turno_mtx = PTHREAD_MUTEX_INITIALIZER;

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

/* Lee global_tick con su mutex (tick_thread lo escribe) */
static int leer_tick(shared_t *sh)
{
    pthread_mutex_lock(&sh->state_mtx);
    int tick = sh->global_tick;
    pthread_mutex_unlock(&sh->state_mtx);
    return tick;
}

/* ---------------------------------------------------------------------------
 * Cómo terminan los hilos
 *
 * Cada hilo, apenas se despierta, pregunta si el juego terminó. Si terminó,
 * igual le pasa la posta al hilo siguiente (para que ese también se despierte,
 * lo vea y salga) y después sale de su while. Así se apagan los 4 en cadena:
 *   event_thread marca game_over -> tick -> scheduler -> signal -> event
 * ------------------------------------------------------------------------- */

/* ---------------------------------------------------------------------------
 * Hilo 1: tick_thread — avanza el reloj del juego
 * ------------------------------------------------------------------------- */
static void *tick_thread(void *arg)
{
    shared_t *sh = arg;

    while (1) {
        sem_wait(&sem_toca_tick);          /* espero a que termine el tick anterior */

        if (is_game_over(sh)) {
            sem_post(&sem_toca_decidir);   /* aviso al siguiente para que también salga */
            break;
        }

        pthread_mutex_lock(&sh->state_mtx);
        sh->global_tick++;
        pthread_mutex_unlock(&sh->state_mtx);

        sem_post(&sem_toca_decidir);       /* posta -> scheduler_thread */
    }
    return NULL;
}

/* ---------------------------------------------------------------------------
 * Hilo 2: scheduler_thread — decide quién juega en este tick
 * ------------------------------------------------------------------------- */
static void *scheduler_thread(void *arg)
{
    shared_t *sh = arg;

    while (1) {
        sem_wait(&sem_toca_decidir);

        if (is_game_over(sh)) {
            sem_post(&sem_toca_senal);
            break;
        }

        process_priority_requests(sh);     /* paso 1: aplicar los buzones */

        int elegido;
        if (le_toca_a_bomber(sh)) {        /* paso 2: comparar prioridades */
            elegido = TURNO_BOMBER;
        } else {
            elegido = TURNO_ENEMY;
        }

        pthread_mutex_lock(&turno_mtx);
        turno_actual = elegido;
        pthread_mutex_unlock(&turno_mtx);

        sem_post(&sem_toca_senal);         /* posta -> signal_thread */
    }
    return NULL;
}

/* ---------------------------------------------------------------------------
 * Hilo 3: signal_thread — da el permiso a P1/P2 y después a P3
 * Es el único hilo de P0 que habla con los otros procesos por semáforos.
 * ------------------------------------------------------------------------- */
static void *signal_thread(void *arg)
{
    shared_t *sh = arg;

    while (1) {
        sem_wait(&sem_toca_senal);

        if (is_game_over(sh)) {
            /* Los hijos están dormidos esperando su turno: los despierto
             * para que vean game_over = 1 y terminen. */
            sem_post(&sh->sem_bomber_turn);
            sem_post(&sh->sem_enemy_turn);
            sem_post(&sh->sem_bomb_turn);
            sem_post(&sh->sem_render);
            sem_post(&sem_toca_eventos);
            break;
        }

        pthread_mutex_lock(&turno_mtx);
        int turno = turno_actual;
        pthread_mutex_unlock(&turno_mtx);

        /* Fase de acción: le doy el turno a P1 o a P2 y espero que termine */
        if (turno == TURNO_BOMBER) {
            printf("[P0] tick %d -> turno de P1 (Bomberman)\n", leer_tick(sh));
            sem_post(&sh->sem_bomber_turn);
        } else {
            printf("[P0] tick %d -> turno de P2 (enemigos)\n", leer_tick(sh));
            sem_post(&sh->sem_enemy_turn);
        }
        sem_wait(&sh->sem_turn_done);      /* espero el "terminé" */

        /* Fase de bombas: P3 juega en TODOS los ticks */
        sem_post(&sh->sem_bomb_turn);
        sem_wait(&sh->sem_turn_done);

        sem_post(&sem_toca_eventos);       /* posta -> event_thread */
    }
    return NULL;
}

/* ---------------------------------------------------------------------------
 * Hilo 4: event_thread — aplica los avisos y decide si el juego terminó
 * ------------------------------------------------------------------------- */
static void *event_thread(void *arg)
{
    shared_t *sh = arg;

    while (1) {
        sem_wait(&sem_toca_eventos);

        if (is_game_over(sh)) {
            break;                         /* es el último de la cadena */
        }

        process_events(sh);                /* vidas, enemigos, puntaje */
        check_end(sh);                     /* si se cumple algo: game_over = 1 */
        print_map(sh);

        pthread_mutex_lock(&sh->state_mtx);
        printf("[P0] vidas=%d enemigos=%d score=%d\n",
               sh->bomber_lives, sh->enemies_alive, sh->score);
        pthread_mutex_unlock(&sh->state_mtx);

        sem_post(&sh->sem_render);         /* avisa al dibujador P4, si existe */
        sem_post(&sem_toca_tick);          /* posta -> tick_thread: siguiente tick */
    }
    return NULL;
}

/* ---------------------------------------------------------------------------
 * run_scheduler: crea los 4 hilos de P0 y espera a que terminen
 * ------------------------------------------------------------------------- */
void run_scheduler(shared_t *sh)
{
    /* sem_init(semáforo, 0 = solo hilos de este proceso, valor_inicial).
     * sem_toca_tick empieza en 1 para que el primer tick arranque solo;
     * los demás empiezan en 0: esperan su posta. */
    sem_init(&sem_toca_tick,    0, 1);
    sem_init(&sem_toca_decidir, 0, 0);
    sem_init(&sem_toca_senal,   0, 0);
    sem_init(&sem_toca_eventos, 0, 0);

    pthread_t hilo_tick, hilo_scheduler, hilo_signal, hilo_event;

    pthread_create(&hilo_tick,      NULL, tick_thread,      sh);
    pthread_create(&hilo_scheduler, NULL, scheduler_thread, sh);
    pthread_create(&hilo_signal,    NULL, signal_thread,    sh);
    pthread_create(&hilo_event,     NULL, event_thread,     sh);

    /* pthread_join espera a que cada hilo termine (como waitpid, pero para hilos) */
    pthread_join(hilo_tick,      NULL);
    pthread_join(hilo_scheduler, NULL);
    pthread_join(hilo_signal,    NULL);
    pthread_join(hilo_event,     NULL);

    sem_destroy(&sem_toca_tick);
    sem_destroy(&sem_toca_decidir);
    sem_destroy(&sem_toca_senal);
    sem_destroy(&sem_toca_eventos);
}
