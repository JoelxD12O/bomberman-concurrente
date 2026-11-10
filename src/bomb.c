/*
 * bomb.c — P3: gestor de bombas (Danilo). Sección 2.4.
 *
 * P3 trabaja con 3 hilos:
 *   bomb_controller_thread  espera el turno de P0 y despierta a los otros dos,
 *                           siempre en el orden: limpiar -> avanzar -> explotar -> registrar
 *   fuse_thread             baja el temporizador de cada bomba activa
 *   explosion_thread        hace explotar las bombas con temporizador en 0 (con cadena)
 *
 * Los hilos se avisan con semáforos LOCALES de P3 (pshared = 0).
 * La tabla bombs[] es privada de P3 y tiene su propio mutex: bombs_mtx.
 *
 * Orden de mutex dentro de P3 (para no tener deadlock):
 *   bombs_mtx -> map_mtx -> bomber_mtx -> enemy_mtx -> bomb_req_mtx -> events_mtx -> state_mtx
 * (bombs_mtx es local; después se sigue el orden de shared.h)
 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include "shared.h"
#include "procs.h"

static shared_t *shm;   /* memoria compartida, la conecta run_bombs */

/* Tabla interna de bombas de P3 */
typedef struct {
    int x;
    int y;
    int timer;
    int active;
} Bomb;

static Bomb bombs[MAX_BOMBS];
static pthread_mutex_t bombs_mtx = PTHREAD_MUTEX_INITIALIZER;  /* mutex propio de bombs[] */

/* Semáforos locales entre los hilos de P3 */
static sem_t sem_fuse_go,      sem_fuse_done;       /* controller <-> fuse      */
static sem_t sem_explosion_go, sem_explosion_done;  /* controller <-> explosion */

/* El controller lo pone en 1 cuando el juego termina, para que los otros hilos salgan */
static volatile int terminar = 0;

/* ---------------------------------------------------------------------------
 * Paso 1: limpiar el fuego del tick anterior
 * ------------------------------------------------------------------------- */
static void limpiar_fuego(void)
{
    pthread_mutex_lock(&shm->map_mtx);
    for (int y = 0; y < shm->rows; y++) {
        for (int x = 0; x < shm->cols; x++) {
            if (shm->map_grid[y][x] == CELL_FIRE) {
                shm->map_grid[y][x] = CELL_FREE;
            }
        }
    }
    pthread_mutex_unlock(&shm->map_mtx);
}

/* ---------------------------------------------------------------------------
 * Paso 2: avanzar los temporizadores (fuse_thread)
 * ------------------------------------------------------------------------- */
static void avanzar_temporizadores(void)
{
    pthread_mutex_lock(&bombs_mtx);
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (bombs[i].active && bombs[i].timer > 0) {
            bombs[i].timer--;
            shm->bomb_timer[i] = bombs[i].timer;   /* bonus P4: tabla pública */
        }
    }
    pthread_mutex_unlock(&bombs_mtx);
}

static void *fuse_thread(void *arg)
{
    (void)arg;
    while (1) {
        sem_wait(&sem_fuse_go);          /* dormir hasta que el controller me despierte */
        if (terminar) {
            break;
        }
        avanzar_temporizadores();
        sem_post(&sem_fuse_done);        /* avisar al controller "terminé" */
    }
    return NULL;
}

/* ---------------------------------------------------------------------------
 * Paso 3: explotar (explosion_thread)
 * ------------------------------------------------------------------------- */

/* Explota UNA bomba: pone fuego en cruz y marca para la cadena las bombas que toca.
 * Se llama con bombs_mtx y map_mtx tomados. */
static void explotar_bomba(int i)
{
    int dx[] = {0, 0, -1, 1};
    int dy[] = {-1, 1, 0, 0};
    int bx = bombs[i].x;
    int by = bombs[i].y;

    bombs[i].active = 0;
    shm->active_bombs--;
    shm->bomb_timer[i] = 0;              /* bonus P4 */
    shm->map_grid[by][bx] = CELL_FIRE;

    for (int d = 0; d < 4; d++) {
        for (int r = 1; r <= BOMB_RANGE; r++) {
            int nx = bx + dx[d] * r;
            int ny = by + dy[d] * r;

            if (nx < 0 || nx >= shm->cols || ny < 0 || ny >= shm->rows) break;

            char celda = shm->map_grid[ny][nx];

            if (celda == CELL_WALL) break;        /* el muro detiene el fuego */

            if (celda == CELL_BLOCK) {            /* el bloque se destruye y detiene el fuego */
                shm->map_grid[ny][nx] = CELL_FIRE;
                shm->blocks_destroyed++;
                break;
            }

            if (celda == CELL_BOMB) {             /* cadena: la otra bomba explotará también */
                for (int j = 0; j < MAX_BOMBS; j++) {
                    if (bombs[j].active && bombs[j].x == nx && bombs[j].y == ny) {
                        bombs[j].timer = 0;       /* solo se marca, NO se explota aquí */
                    }
                }
                break;                            /* el fuego se detiene en la otra Q */
            }

            shm->map_grid[ny][nx] = CELL_FIRE;
        }
    }
}

static void explotar(void)
{
    pthread_mutex_lock(&bombs_mtx);

    /* Orden de shared.h: map -> bomber -> enemy -> events (-> state) */
    pthread_mutex_lock(&shm->map_mtx);
    pthread_mutex_lock(&shm->bomber_mtx);
    pthread_mutex_lock(&shm->enemy_mtx);
    pthread_mutex_lock(&shm->events_mtx);
    pthread_mutex_lock(&shm->state_mtx);

    /*
     * Cadena: una explosión solo pone timer = 0 a las bombas que toca.
     * Así nunca se modifica la tabla "por dentro" del recorrido.
     * Se repite el recorrido completo mientras alguna bomba haya explotado,
     * para que las bombas marcadas (aunque estén antes en la tabla) también exploten.
     */
    int hubo_explosion = 0;
    int otra_vuelta = 1;
    while (otra_vuelta) {
        otra_vuelta = 0;
        for (int i = 0; i < MAX_BOMBS; i++) {
            if (bombs[i].active && bombs[i].timer <= 0) {
                printf("[P3] tick %d: explota la bomba en (%d,%d)\n",
                       shm->global_tick, bombs[i].x, bombs[i].y);
                explotar_bomba(i);
                hubo_explosion = 1;
                otra_vuelta = 1;
            }
        }
    }

    if (hubo_explosion) {
        /* ¿el fuego tocó a Bomberman? */
        if (shm->map_grid[shm->bomber_y][shm->bomber_x] == CELL_FIRE) {
            shm->blast_hit_bomber = 1;
            shm->blast_tick = shm->global_tick;
        }
        /* ¿el fuego tocó a algún enemigo vivo? */
        for (int e = 0; e < NUM_ENEMIES; e++) {
            if (shm->enemy_alive[e] &&
                shm->map_grid[shm->enemy_y[e]][shm->enemy_x[e]] == CELL_FIRE) {
                shm->blast_hit_enemy[e] = 1;
            }
        }
    }

    /* Liberar en orden inverso */
    pthread_mutex_unlock(&shm->state_mtx);
    pthread_mutex_unlock(&shm->events_mtx);
    pthread_mutex_unlock(&shm->enemy_mtx);
    pthread_mutex_unlock(&shm->bomber_mtx);
    pthread_mutex_unlock(&shm->map_mtx);

    pthread_mutex_unlock(&bombs_mtx);
}

static void *explosion_thread(void *arg)
{
    (void)arg;
    while (1) {
        sem_wait(&sem_explosion_go);
        if (terminar) {
            break;
        }
        explotar();
        sem_post(&sem_explosion_done);
    }
    return NULL;
}

/* ---------------------------------------------------------------------------
 * Paso 4: registrar la bomba que pidió P1
 * ------------------------------------------------------------------------- */
static void registrar_solicitudes(void)
{
    /* bombs_mtx -> map_mtx -> bomb_req_mtx (mismo orden que explotar) */
    pthread_mutex_lock(&bombs_mtx);
    pthread_mutex_lock(&shm->map_mtx);
    pthread_mutex_lock(&shm->bomb_req_mtx);

    if (shm->bomb_request_active) {
        int rx = shm->bomb_request_x;
        int ry = shm->bomb_request_y;

        if (shm->active_bombs < MAX_BOMBS && shm->map_grid[ry][rx] != CELL_BOMB) {
            for (int i = 0; i < MAX_BOMBS; i++) {
                if (!bombs[i].active) {
                    bombs[i].x = rx;
                    bombs[i].y = ry;
                    bombs[i].timer = BOMB_TIMER;
                    bombs[i].active = 1;

                    shm->active_bombs++;
                    shm->map_grid[ry][rx] = CELL_BOMB;

                    /* bonus P4: tabla pública */
                    shm->bomb_x[i] = rx;
                    shm->bomb_y[i] = ry;
                    shm->bomb_timer[i] = BOMB_TIMER;

                    printf("[P3] bomba registrada en (%d,%d)\n", rx, ry);
                    break;
                }
            }
        }

        shm->bomb_request_active = 0;   /* P3 limpia la bandera */
    }

    pthread_mutex_unlock(&shm->bomb_req_mtx);
    pthread_mutex_unlock(&shm->map_mtx);
    pthread_mutex_unlock(&bombs_mtx);
}

/* ---------------------------------------------------------------------------
 * Hilo controlador: recibe el turno de P0 y ordena a los otros hilos
 * ------------------------------------------------------------------------- */
static void *bomb_controller_thread(void *arg)
{
    (void)arg;
    while (1) {
        sem_wait(&shm->sem_bomb_turn);      /* dormir hasta que P0 dé el turno */
        if (is_game_over(shm)) {
            break;
        }

        /* 1. limpiar */
        limpiar_fuego();

        /* 2. avanzar: despierto a fuse_thread y espero que termine */
        sem_post(&sem_fuse_go);
        sem_wait(&sem_fuse_done);

        /* 3. explotar: despierto a explosion_thread y espero que termine */
        sem_post(&sem_explosion_go);
        sem_wait(&sem_explosion_done);

        /* 4. registrar (después de explotar: una bomba nueva no explota en su mismo tick) */
        registrar_solicitudes();

        sem_post(&shm->sem_turn_done);      /* avisar a P0 "terminé mi turno" */
    }

    /* El juego terminó: despierto a los otros hilos para que salgan */
    terminar = 1;
    sem_post(&sem_fuse_go);
    sem_post(&sem_explosion_go);
    return NULL;
}

/* ---------------------------------------------------------------------------
 * Punto de entrada de P3 (lo llama main.c en el hijo)
 * ------------------------------------------------------------------------- */
void run_bombs(shared_t *sh)
{
    shm = sh;

    for (int i = 0; i < MAX_BOMBS; i++) {
        bombs[i].active = 0;
    }

    /* semáforos locales del proceso P3 (pshared = 0), todos empiezan en 0 */
    sem_init(&sem_fuse_go,        0, 0);
    sem_init(&sem_fuse_done,      0, 0);
    sem_init(&sem_explosion_go,   0, 0);
    sem_init(&sem_explosion_done, 0, 0);

    pthread_t controller, fuse, explosion;
    pthread_create(&fuse,       NULL, fuse_thread,            NULL);
    pthread_create(&explosion,  NULL, explosion_thread,       NULL);
    pthread_create(&controller, NULL, bomb_controller_thread, NULL);

    pthread_join(controller, NULL);
    pthread_join(fuse,       NULL);
    pthread_join(explosion,  NULL);

    sem_destroy(&sem_fuse_go);
    sem_destroy(&sem_fuse_done);
    sem_destroy(&sem_explosion_go);
    sem_destroy(&sem_explosion_done);
    pthread_mutex_destroy(&bombs_mtx);
}
