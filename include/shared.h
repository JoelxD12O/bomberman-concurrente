/*
 * shared.h — CONTRATO entre los 4 procesos.
 *
 * Toda la comunicación entre P0, P1, P2 y P3 pasa por esta estructura.
 * Regla de oro (sección 13): cada variable tiene UN solo proceso que la escribe.
 * En buzones y eventos, el receptor solo limpia la bandera.
 *
 * Si alguien necesita cambiar este archivo, se avisa al grupo ANTES.
 */
#ifndef SHARED_H
#define SHARED_H

#include <pthread.h>
#include <semaphore.h>

/* ---------- Constantes del juego (sección 8 y 12) ---------- */
#define MAX_X        32
#define MAX_Y        32
#define NUM_ENEMIES  4
#define BOMB_TIMER   3      /* ticks entre registro y explosión */
#define BOMB_RANGE   2      /* celdas por dirección             */
#define MAX_BOMBS    2      /* bombas activas simultáneas       */
#define INITIAL_LIVES 3
#define PRIO_MIN     1
#define PRIO_MAX     49

/* ---------- Símbolos del mapa (sección 7) ---------- */
#define CELL_WALL    'X'
#define CELL_BLOCK   '#'
#define CELL_FREE    'O'
#define CELL_BOMB    'Q'
#define CELL_FIRE    '*'

/* ---------- Resultado del juego ---------- */
enum game_result {
    RESULT_RUNNING = 0,
    RESULT_MAX_TICKS,
    RESULT_DEFEAT,        /* bomber_lives == 0          */
    RESULT_VICTORY,       /* enemies_alive == 0         */
    RESULT_NO_MOVES       /* archivos vacíos y sin bombas */
};

typedef struct {
    /* ===== Escribe P0 ===== */
    int global_tick, max_ticks;
    int game_over, game_result;
    int bomber_lives, score;
    int prioridad_bomber, prioridad_enemy;
    int enemy_alive[NUM_ENEMIES], enemies_alive;

    /* ===== Escribe P1 ===== */
    int bomber_x, bomber_y, bomber_moves_done;
    int pending_priority_bomber, priority_request_active;     /* P0 limpia */
    int bomb_request_x, bomb_request_y, bomb_request_active;  /* P3 limpia */

    /* ===== Escribe P2 ===== */
    int enemy_x[NUM_ENEMIES], enemy_y[NUM_ENEMIES], enemy_moves_done;
    int collision_detected, collision_tick, collision_enemy_id; /* P0 limpia */
    int pending_priority_enemy, enemy_priority_request_active;  /* P0 limpia */

    /* ===== Escribe P0 al iniciar; luego P3 ===== */
    char map_grid[MAX_Y][MAX_X];
    int rows, cols;

    /* ===== Escribe P3 ===== */
    int active_bombs, blocks_destroyed;
    int blast_hit_bomber, blast_tick, blast_hit_enemy[NUM_ENEMIES]; /* P0 limpia */

    /* ===== Bonus P4: tabla de bombas pública (escribe P3) ===== */
    int bomb_x[MAX_BOMBS], bomb_y[MAX_BOMBS], bomb_timer[MAX_BOMBS];

    /* ===== Ruta de los archivos de entrada (P0 la copia al iniciar) ===== */
    char data_dir[256];

    /* ===== Semáforos entre procesos (pshared = 1) ===== */
    sem_t sem_bomber_turn;   /* P0 -> P1 */
    sem_t sem_enemy_turn;    /* P0 -> P2 */
    sem_t sem_bomb_turn;     /* P0 -> P3 */
    sem_t sem_turn_done;     /* P1/P2/P3 -> P0 */
    sem_t sem_render;        /* P0 -> P4 (bonus) */

    /*
     * ===== Mutex entre procesos (PTHREAD_PROCESS_SHARED) =====
     * ORDEN DE ADQUISICIÓN (para no tener deadlock), siempre de arriba hacia abajo:
     *   map_mtx -> bomber_mtx -> enemy_mtx -> bomb_req_mtx -> prio_mtx -> events_mtx -> state_mtx
     */
    pthread_mutex_t map_mtx;       /* map_grid, active_bombs, blocks_destroyed       */
    pthread_mutex_t bomber_mtx;    /* bomber_x, bomber_y, bomber_moves_done          */
    pthread_mutex_t enemy_mtx;     /* enemy_x[], enemy_y[], enemy_moves_done         */
    pthread_mutex_t bomb_req_mtx;  /* bomb_request_*                                 */
    pthread_mutex_t prio_mtx;      /* buzones de prioridad + prioridades vigentes    */
    pthread_mutex_t events_mtx;    /* collision_*, blast_*                           */
    pthread_mutex_t state_mtx;     /* global_tick, game_over, lives, score, alive[]  */
} shared_t;

#endif
