#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include "procs.h"

// Estructura para pasar datos a los hilos de los enemigos
typedef struct {
    int id;
    shared_t *sh;
    FILE *archivo;
} enemy_data_t;

// 1. MUTEX ESPECÍFICOS SOLICITADOS EN EL ISSUE
pthread_mutex_t local_pos_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t enemy_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t events_mtx = PTHREAD_MUTEX_INITIALIZER;

// 2. SEMÁFOROS "GO" Y "DONE"
sem_t sem_start_enemy[NUM_ENEMIES];
sem_t sem_enemy_done;
sem_t sem_start_tracker;
sem_t sem_tracker_done;
sem_t sem_start_collision;
sem_t sem_collision_done;
sem_t sem_start_publisher;

int todos_terminaron = 1;
int pedido_en_este_turno = 0;
int running = 1;

// --- HILOS DE LOS ENEMIGOS ---
void* enemy_thread(void* arg) {
    enemy_data_t *data = (enemy_data_t*)arg;
    int i = data->id;
    shared_t *sh = data->sh;
    FILE *archivo = data->archivo;
    char linea[64];

    while (1) {
        sem_wait(&sem_start_enemy[i]);
        if (!running) break; // Salida limpia al terminar

        while (archivo != NULL && fgets(linea, sizeof(linea), archivo) != NULL) {
            pthread_mutex_lock(&enemy_mtx);
            todos_terminaron = 0;
            pthread_mutex_unlock(&enemy_mtx);
            
            linea[strcspn(linea, "\r\n")] = 0;
            instruction ins = parse_instruction(linea);

            if (ins.type == INS_INVALID || ins.type == INS_BOMB) {
                continue;
            }

            if (ins.type == INS_SET_PRIORITY) {
                pthread_mutex_lock(&enemy_mtx);
                if (ins.value > pedido_en_este_turno) {
                    pedido_en_este_turno = ins.value;
                    sh->pending_priority_enemy = ins.value;
                    sh->enemy_priority_request_active = 1;
                }
                pthread_mutex_unlock(&enemy_mtx);
                continue;
            }

            int dx = 0, dy = 0;
            if (ins.type == INS_UP) dy = -1;
            else if (ins.type == INS_DOWN) dy = 1;
            else if (ins.type == INS_LEFT) dx = -1;
            else if (ins.type == INS_RIGHT) dx = 1;

            if (dx != 0 || dy != 0) {
                pthread_mutex_lock(&local_pos_mtx);
                int nx = sh->enemy_x[i] + dx;
                int ny = sh->enemy_y[i] + dy;
                if (is_walkable(sh, nx, ny)) {
                    sh->enemy_x[i] = nx;
                    sh->enemy_y[i] = ny;
                }
                pthread_mutex_unlock(&local_pos_mtx);
            }
            break;
        }
        sem_post(&sem_enemy_done);
    }
    return NULL;
}

// --- HILO RASTREADOR ---
void* bomber_tracker_thread(void* arg) {
    shared_t *sh = (shared_t*)arg;
    while (1) {
        sem_wait(&sem_start_tracker);
        if (!running) break;
        // Lectura de posiciones locales
        pthread_mutex_lock(&local_pos_mtx);
        int bx = sh->bomber_x;
        int by = sh->bomber_y;
        pthread_mutex_unlock(&local_pos_mtx);
        sem_post(&sem_tracker_done);
    }
    return NULL;
}

// --- HILO COLISIONES ---
void* collision_thread(void* arg) {
    shared_t *sh = (shared_t*)arg;
    while (1) {
        sem_wait(&sem_start_collision);
        if (!running) break;

        pthread_mutex_lock(&local_pos_mtx);
        for (int i = 0; i < NUM_ENEMIES; i++) {
            if (sh->enemy_alive[i] && sh->enemy_x[i] == sh->bomber_x && sh->enemy_y[i] == sh->bomber_y) {
                pthread_mutex_lock(&events_mtx);
                sh->collision_detected = 1;
                sh->collision_tick = sh->global_tick;
                sh->collision_enemy_id = i;
                pthread_mutex_unlock(&events_mtx);
            }
        }
        pthread_mutex_unlock(&local_pos_mtx);
        sem_post(&sem_collision_done);
    }
    return NULL;
}

// --- HILO PUBLICADOR ---
void* enemy_publisher_thread(void* arg) {
    shared_t *sh = (shared_t*)arg;
    while (1) {
        sem_wait(&sem_start_publisher);
        if (!running) break;

        pthread_mutex_lock(&enemy_mtx);
        if (todos_terminaron) {
            sh->enemy_moves_done = 1;
        }
        pthread_mutex_unlock(&enemy_mtx);

        sem_post(&sh->sem_turn_done);
    }
    return NULL;
}

// --- HILO CONTROLADOR ---
void* enemy_controller_thread(void* arg) {
    shared_t *sh = (shared_t*)arg;
    while (1) {
        sem_wait(&sh->sem_enemy_turn);
        
        // 3. AL TERMINAR, DESPERTAR A TODOS PARA QUE SALGAN (Que ninguno quede colgado)
        if (is_game_over(sh)) {
            running = 0;
            for (int i = 0; i < NUM_ENEMIES; i++) sem_post(&sem_start_enemy[i]);
            sem_post(&sem_start_tracker);
            sem_post(&sem_start_collision);
            sem_post(&sem_start_publisher);
            break; 
        }

        pthread_mutex_lock(&enemy_mtx);
        todos_terminaron = 1;
        pedido_en_este_turno = 0;
        pthread_mutex_unlock(&enemy_mtx);

        // 4. EL CONTROLLER DESPIERTA SOLO A LOS VIVOS...
        int vivos_en_este_turno = 0;
        for (int i = 0; i < NUM_ENEMIES; i++) {
            if (sh->enemy_alive[i]) {
                vivos_en_este_turno++;
                sem_post(&sem_start_enemy[i]); 
            }
        }
        
        // ... Y ESPERA A TODOS (los que despertó)
        for (int i = 0; i < vivos_en_este_turno; i++) {
            sem_wait(&sem_enemy_done);
        }

        // Fases restantes
        sem_post(&sem_start_tracker);
        sem_post(&sem_start_collision);
        sem_wait(&sem_tracker_done);
        sem_wait(&sem_collision_done);

        sem_post(&sem_start_publisher);
    }
    return NULL;
}

// --- FUNCIÓN PRINCIPAL DE P2 ---
void run_enemies(shared_t *sh) {
    FILE *archivos[NUM_ENEMIES];
    for (int i = 0; i < NUM_ENEMIES; i++) {
        char ruta[300];
        snprintf(ruta, sizeof(ruta), "%s/enemy_%d_moves.txt", sh->data_dir, i + 1);
        archivos[i] = fopen(ruta, "r");
    }

    sem_init(&sem_enemy_done, 0, 0);
    sem_init(&sem_tracker_done, 0, 0);
    sem_init(&sem_collision_done, 0, 0);
    sem_init(&sem_start_tracker, 0, 0);
    sem_init(&sem_start_collision, 0, 0);
    sem_init(&sem_start_publisher, 0, 0);
    for (int i = 0; i < NUM_ENEMIES; i++) sem_init(&sem_start_enemy[i], 0, 0);

    pthread_t t_enemies[NUM_ENEMIES], t_controller, t_tracker, t_collision, t_publisher;
    enemy_data_t e_data[NUM_ENEMIES];

    for (int i = 0; i < NUM_ENEMIES; i++) {
        e_data[i].id = i;
        e_data[i].sh = sh;
        e_data[i].archivo = archivos[i];
        pthread_create(&t_enemies[i], NULL, enemy_thread, &e_data[i]);
    }
    pthread_create(&t_tracker, NULL, bomber_tracker_thread, sh);
    pthread_create(&t_collision, NULL, collision_thread, sh);
    pthread_create(&t_publisher, NULL, enemy_publisher_thread, sh);
    pthread_create(&t_controller, NULL, enemy_controller_thread, sh);

    pthread_join(t_controller, NULL);

    for (int i = 0; i < NUM_ENEMIES; i++) {
        pthread_join(t_enemies[i], NULL);
        if (archivos[i]) fclose(archivos[i]);
        sem_destroy(&sem_start_enemy[i]);
    }
    pthread_join(t_tracker, NULL);
    pthread_join(t_collision, NULL);
    pthread_join(t_publisher, NULL);

    sem_destroy(&sem_enemy_done);
    sem_destroy(&sem_tracker_done);
    sem_destroy(&sem_collision_done);
    sem_destroy(&sem_start_tracker);
    sem_destroy(&sem_start_collision);
    sem_destroy(&sem_start_publisher);
}
