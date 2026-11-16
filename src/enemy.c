/*
 * enemy.c — P2: los 4 enemigos (secciones 2.3 y 6). Mariel.
 *
 * Cada enemigo tiene su propio archivo (enemy_1_moves.txt ... enemy_4_moves.txt).
 * Cada vez que P0 le da el turno a P2, cada enemigo VIVO ejecuta UNA instrucción.
 *
 * Reglas:
 *   - Un enemigo muerto (sh->enemy_alive[i] == 0) ya no se mueve.
 *   - Dos enemigos SÍ pueden estar en la misma celda.
 *   - BOMB no vale para enemigos (es una instrucción inválida).
 *   - P2 no quita vidas: solo avisa que hubo choque (collision_...) y P0 resta la vida.
 *   - SET_PRIORITY deja un pedido para P0 (pending_priority_enemy). Si dos
 *     enemigos lo piden en el mismo turno, gana el de número mayor.
 *
 * SEMANA 1: escribir la lógica dentro del while de abajo (sin hilos, sin mutex).
 * SEMANA 2: dividirla en 8 hilos:
 *   enemy_controller_thread   recibe el turno y despierta a los enemigos vivos
 *   enemy_thread x 4          uno por enemigo, cada uno lee su archivo
 *   bomber_tracker_thread     guarda una copia de la posición de Bomberman
 *   collision_thread          revisa si algún enemigo tocó a Bomberman
 *   enemy_publisher_thread    copia las posiciones a la memoria compartida
 */

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include "procs.h"

// --- ESTRUCTURAS Y VARIABLES GLOBALES (Protegidas por Mutex) ---

typedef struct {
    int id;
    shared_t *sh;
    FILE *archivo;
} enemy_data_t;

pthread_mutex_t local_pos_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t p2_mtx = PTHREAD_MUTEX_INITIALIZER;

sem_t sem_start_enemy[NUM_ENEMIES];
sem_t sem_enemy_done;
sem_t sem_start_tracker;
sem_t sem_tracker_done;
sem_t sem_start_collision;
sem_t sem_collision_done;
sem_t sem_start_publisher;

int todos_terminaron = 1;
int enemigo_del_pedido = -1;   // número del enemigo que pidió SET_PRIORITY en este turno
int running = 1;

// Copias de posición rastreadas de Bomberman (bomber_last_position)
int copia_bx = 0;
int copia_by = 0;

// Posiciones INTERNAS de los enemigos (enemy_positions[], sección 2.3).
// Los enemy_thread mueven estas; enemy_publisher_thread las copia a la memoria compartida.
int pos_x[NUM_ENEMIES];
int pos_y[NUM_ENEMIES];

/*
 * Orden de mutex en P2 (para no tener deadlock):
 *   primero los locales (p2_mtx o local_pos_mtx), después los compartidos
 *   en el orden de shared.h: map -> bomber -> enemy -> prio -> events -> state
 */

// Lee enemy_alive[i] (lo escribe P0) con su mutex compartido
static int esta_vivo(shared_t *sh, int i) {
    pthread_mutex_lock(&sh->state_mtx);
    int vivo = sh->enemy_alive[i];
    pthread_mutex_unlock(&sh->state_mtx);
    return vivo;
}

// --- HILOS DE LOS ENEMIGOS ---
void* enemy_thread(void* arg) {
    enemy_data_t *data = (enemy_data_t*)arg;
    int i = data->id;
    shared_t *sh = data->sh;
    FILE *archivo = data->archivo;
    char linea[64];

    while (1) {
        sem_wait(&sem_start_enemy[i]);
        
        pthread_mutex_lock(&p2_mtx);
        int seguir = running;
        pthread_mutex_unlock(&p2_mtx);
        
        if (!seguir) break; 

        while (archivo != NULL && fgets(linea, sizeof(linea), archivo) != NULL) {
            pthread_mutex_lock(&p2_mtx);
            todos_terminaron = 0;
            pthread_mutex_unlock(&p2_mtx);
            
            linea[strcspn(linea, "\r\n")] = 0;
            instruction ins = parse_instruction(linea);

            /* Línea inválida (o BOMB, que no vale para enemigos): se reporta y
             * se lee la siguiente, sin gastar el turno (sección 6) */
            if (ins.type == INS_INVALID || ins.type == INS_BOMB) {
                printf("[P2] Aviso: enemigo %d, línea inválida descartada: \"%s\"\n", i + 1, linea);
                continue;
            }

            /* SET_PRIORITY: si dos enemigos lo piden en el mismo turno,
             * prevalece el del enemigo de MAYOR NÚMERO (sección 10) */
            if (ins.type == INS_SET_PRIORITY) {
                pthread_mutex_lock(&p2_mtx);
                if (i > enemigo_del_pedido) {
                    enemigo_del_pedido = i;
                    pthread_mutex_lock(&sh->prio_mtx);      // buzón compartido con P0
                    sh->pending_priority_enemy = ins.value;
                    sh->enemy_priority_request_active = 1;
                    pthread_mutex_unlock(&sh->prio_mtx);
                }
                pthread_mutex_unlock(&p2_mtx);
                break;
            }

            int dx = 0, dy = 0;
            if (ins.type == INS_UP) dy = -1;
            else if (ins.type == INS_DOWN) dy = 1;
            else if (ins.type == INS_LEFT) dx = -1;
            else if (ins.type == INS_RIGHT) dx = 1;

            if (dx != 0 || dy != 0) {
                pthread_mutex_lock(&local_pos_mtx);
                int nx = pos_x[i] + dx;
                int ny = pos_y[i] + dy;

                pthread_mutex_lock(&sh->map_mtx);           // P3 escribe el mapa
                int se_puede = is_walkable(sh, nx, ny);
                pthread_mutex_unlock(&sh->map_mtx);

                if (se_puede) {
                    pos_x[i] = nx;                          // posición interna
                    pos_y[i] = ny;
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
        
        pthread_mutex_lock(&p2_mtx);
        int seguir = running;
        pthread_mutex_unlock(&p2_mtx);
        if (!seguir) break;

        pthread_mutex_lock(&local_pos_mtx);
        pthread_mutex_lock(&sh->bomber_mtx);                // P1 publica la posición
        copia_bx = sh->bomber_x;
        copia_by = sh->bomber_y;
        pthread_mutex_unlock(&sh->bomber_mtx);
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
        
        pthread_mutex_lock(&p2_mtx);
        int seguir = running;
        pthread_mutex_unlock(&p2_mtx);
        if (!seguir) break;

        pthread_mutex_lock(&sh->state_mtx);                 // lo escribe P0
        int tick = sh->global_tick;
        pthread_mutex_unlock(&sh->state_mtx);

        /* Compara la copia de Bomberman con las posiciones INTERNAS de los enemigos vivos */
        pthread_mutex_lock(&local_pos_mtx);
        for (int i = 0; i < NUM_ENEMIES; i++) {
            if (esta_vivo(sh, i) && pos_x[i] == copia_bx && pos_y[i] == copia_by) {
                pthread_mutex_lock(&sh->events_mtx);        // evento compartido con P0
                sh->collision_detected = 1;
                sh->collision_tick = tick;
                sh->collision_enemy_id = i;
                pthread_mutex_unlock(&sh->events_mtx);
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
        
        pthread_mutex_lock(&p2_mtx);
        int seguir = running;
        pthread_mutex_unlock(&p2_mtx);
        if (!seguir) break;

        pthread_mutex_lock(&p2_mtx);
        int terminaron = todos_terminaron;
        pthread_mutex_unlock(&p2_mtx);

        /* Copia las posiciones internas a la memoria compartida (enemy_x[], enemy_y[]),
         * para que P3 sepa a qué enemigos alcanza una explosión */
        pthread_mutex_lock(&local_pos_mtx);
        pthread_mutex_lock(&sh->enemy_mtx);
        for (int i = 0; i < NUM_ENEMIES; i++) {
            sh->enemy_x[i] = pos_x[i];
            sh->enemy_y[i] = pos_y[i];
        }
        if (terminaron) {
            sh->enemy_moves_done = 1;
        }
        pthread_mutex_unlock(&sh->enemy_mtx);
        pthread_mutex_unlock(&local_pos_mtx);

        sem_post(&sh->sem_turn_done);
    }
    return NULL;
}

// --- HILO CONTROLADOR ---
void* enemy_controller_thread(void* arg) {
    shared_t *sh = (shared_t*)arg;
    while (1) {
        sem_wait(&sh->sem_enemy_turn);
        
        if (is_game_over(sh)) {
            pthread_mutex_lock(&p2_mtx);
            running = 0;
            pthread_mutex_unlock(&p2_mtx);
            
            for (int i = 0; i < NUM_ENEMIES; i++) sem_post(&sem_start_enemy[i]);
            sem_post(&sem_start_tracker);
            sem_post(&sem_start_collision);
            sem_post(&sem_start_publisher);
            break; 
        }

        pthread_mutex_lock(&p2_mtx);
        todos_terminaron = 1;
        enemigo_del_pedido = -1;
        pthread_mutex_unlock(&p2_mtx);

        int vivos_en_este_turno = 0;
        for (int i = 0; i < NUM_ENEMIES; i++) {
            if (esta_vivo(sh, i)) {
                vivos_en_este_turno++;
                sem_post(&sem_start_enemy[i]); 
            }
        }
        
        for (int i = 0; i < vivos_en_este_turno; i++) {
            sem_wait(&sem_enemy_done);
        }

        /* Primero el tracker copia la posición de Bomberman y RECIÉN DESPUÉS
         * collision la usa; si corrieran a la vez, podría usar la copia vieja */
        sem_post(&sem_start_tracker);
        sem_wait(&sem_tracker_done);
        sem_post(&sem_start_collision);
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

    /* Posiciones iniciales: las cargó P0 desde map.txt */
    pthread_mutex_lock(&sh->enemy_mtx);
    for (int i = 0; i < NUM_ENEMIES; i++) {
        pos_x[i] = sh->enemy_x[i];
        pos_y[i] = sh->enemy_y[i];
    }
    pthread_mutex_unlock(&sh->enemy_mtx);

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
