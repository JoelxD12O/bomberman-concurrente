#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include "shared.h"
#include "procs.h"

shared_t *shm; // Puntero global para que las funciones internas accedan a la memoria

// Estructura interna de P3 para llevar el control de las bombas
typedef struct {
    int x;
    int y;
    int timer;
    int active;
} Bomb;

Bomb bombs[MAX_BOMBS];
pthread_mutex_t local_bombs_mtx = PTHREAD_MUTEX_INITIALIZER;

void limpiar_fuego() {
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

void* fuse_thread(void* arg) {
    (void)arg; // Evita el warning de 'unused parameter'
    pthread_mutex_lock(&local_bombs_mtx);
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (bombs[i].active && bombs[i].timer > 0) {
            bombs[i].timer--;
            // Bonus P4: Actualizar la tabla pública
            shm->bomb_timer[i] = bombs[i].timer;
        }
    }
    pthread_mutex_unlock(&local_bombs_mtx);
    return NULL;
}

void* explosion_thread(void* arg) {
    (void)arg; // Evita el warning de 'unused parameter'
    pthread_mutex_lock(&local_bombs_mtx);
    
    // ORDEN ESTRICTO: map_mtx -> bomber_mtx -> enemy_mtx -> events_mtx -> state_mtx
    pthread_mutex_lock(&shm->map_mtx);
    pthread_mutex_lock(&shm->bomber_mtx);
    pthread_mutex_lock(&shm->enemy_mtx);
    pthread_mutex_lock(&shm->events_mtx);
    pthread_mutex_lock(&shm->state_mtx);

    int reevaluar = 1;
    while (reevaluar) {
        reevaluar = 0;
        for (int i = 0; i < MAX_BOMBS; i++) {
            if (bombs[i].active && bombs[i].timer <= 0) {
                bombs[i].active = 0;
                shm->active_bombs--;
                
                int bx = bombs[i].x;
                int by = bombs[i].y;
                shm->map_grid[by][bx] = CELL_FIRE;

                int dx[] = {0, 0, -1, 1};
                int dy[] = {-1, 1, 0, 0};

                for (int d = 0; d < 4; d++) {
                    for (int r = 1; r <= BOMB_RANGE; r++) {
                        int nx = bx + (dx[d] * r);
                        int ny = by + (dy[d] * r);

                        if (nx < 0 || nx >= shm->cols || ny < 0 || ny >= shm->rows) break;

                        char celda = shm->map_grid[ny][nx];

                        if (celda == CELL_WALL) break;
                        
                        if (celda == CELL_BLOCK) {
                            shm->map_grid[ny][nx] = CELL_FIRE;
                            shm->blocks_destroyed++;
                            break;
                        }

                        if (celda == CELL_BOMB) {
                            for (int j = 0; j < MAX_BOMBS; j++) {
                                if (bombs[j].active && bombs[j].x == nx && bombs[j].y == ny) {
                                    bombs[j].timer = 0;
                                    reevaluar = 1; 
                                }
                            }
                        }

                        shm->map_grid[ny][nx] = CELL_FIRE;
                    }
                }
            }
        }
    }

    // Evaluar daños a Bomberman
    if (shm->map_grid[shm->bomber_y][shm->bomber_x] == CELL_FIRE) {
        shm->blast_hit_bomber = 1;
        shm->blast_tick = shm->global_tick;
    }

    // Evaluar daños a Enemigos
    for (int e = 0; e < NUM_ENEMIES; e++) {
        if (shm->enemy_alive[e] == 1) {
            if (shm->map_grid[shm->enemy_y[e]][shm->enemy_x[e]] == CELL_FIRE) {
                shm->blast_hit_enemy[e] = 1;
            }
        }
    }

    // Liberar en orden inverso
    pthread_mutex_unlock(&shm->state_mtx);
    pthread_mutex_unlock(&shm->events_mtx);
    pthread_mutex_unlock(&shm->enemy_mtx);
    pthread_mutex_unlock(&shm->bomber_mtx);
    pthread_mutex_unlock(&shm->map_mtx);
    
    pthread_mutex_unlock(&local_bombs_mtx);
    return NULL;
}

void registrar_solicitudes() {
    // ORDEN ESTRICTO: map_mtx -> bomb_req_mtx
    pthread_mutex_lock(&shm->map_mtx);
    pthread_mutex_lock(&shm->bomb_req_mtx);
    
    if (shm->bomb_request_active) {
        pthread_mutex_lock(&local_bombs_mtx);
        
        if (shm->active_bombs < MAX_BOMBS) {
            int rx = shm->bomb_request_x;
            int ry = shm->bomb_request_y;

            if (shm->map_grid[ry][rx] != CELL_BOMB) {
                for (int i = 0; i < MAX_BOMBS; i++) {
                    if (!bombs[i].active) {
                        bombs[i].x = rx;
                        bombs[i].y = ry;
                        bombs[i].timer = BOMB_TIMER;
                        bombs[i].active = 1;
                        
                        shm->active_bombs++;
                        shm->map_grid[ry][rx] = CELL_BOMB;
                        
                        // Bonus P4: Actualizar la tabla pública
                        shm->bomb_x[i] = rx;
                        shm->bomb_y[i] = ry;
                        shm->bomb_timer[i] = BOMB_TIMER;
                        break;
                    }
                }
            }
        }
        pthread_mutex_unlock(&local_bombs_mtx);
        
        // P3 limpia la bandera
        shm->bomb_request_active = 0; 
    }
    
    pthread_mutex_unlock(&shm->bomb_req_mtx);
    pthread_mutex_unlock(&shm->map_mtx);
}

// ESTA ES LA FUNCIÓN PRINCIPAL DE TU PROCESO (Llamada por main.c)
void run_bombs(shared_t *sh) {
    // Conectar el puntero global de P3 a la memoria que creó P0
    shm = sh;

    // Inicializar el arreglo de bombas local
    for (int i = 0; i < MAX_BOMBS; i++) {
        bombs[i].active = 0;
    }

    // Ciclo de vida infinito del proceso P3
    while (1) {
        sem_wait(&shm->sem_bomb_turn);

        // Verificar si el juego terminó leyendo state_mtx
        pthread_mutex_lock(&shm->state_mtx);
        int game_over = shm->game_over;
        pthread_mutex_unlock(&shm->state_mtx);

        if (game_over) {
            sem_post(&shm->sem_turn_done);
            break;
        }

        limpiar_fuego();
        fuse_thread(NULL);
        explosion_thread(NULL);
        registrar_solicitudes();

        sem_post(&shm->sem_turn_done);
    }
}