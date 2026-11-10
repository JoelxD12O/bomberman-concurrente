/*
 * bomber.c — P1: Bomberman (secciones 2.2, 6, 8.1 y 10). Abigail.
 *
 * Lee bomber_moves.txt y, cada vez que P0 le da el turno, ejecuta UNA instrucción.
 *
 * Reglas:
 *   - P1 NO escribe en el mapa. Para poner una bomba deja un pedido
 *     (bomb_request_...) y P3 es quien la pone.
 *   - SET_PRIORITY no cambia la prioridad: deja un pedido para P0
 *     (pending_priority_bomber).
 *   - Si la celda de destino está ocupada, Bomberman no se mueve pero
 *     IGUAL gasta el turno.
 *   - Una línea que no se entiende se salta y se lee la siguiente
 *     (eso NO gasta el turno).
 *
 * SEMANA 1: escribir la lógica dentro del while de abajo (sin hilos, sin mutex).
 * SEMANA 2: dividirla en 3 hilos:
 *   movement_reader_thread    lee el archivo y guarda las instrucciones en una cola
 *   movement_executor_thread  en cada turno saca UNA instrucción y la ejecuta
 *   bomber_publisher_thread   copia la posición de Bomberman a la memoria compartida
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include "procs.h"

shared_t *shared_mem;

#define QUEUE_SIZE 100

instruction instruction_queue[QUEUE_SIZE];
int q_head = 0;
int q_tail = 0;
int reader_finished = 0;

pthread_mutex_t queue_mtx;
sem_t sem_items;

int local_nx, local_ny;
int local_moves_done = 0;
pthread_mutex_t pos_mtx;
sem_t sem_publish;
sem_t sem_publish_done;

void* movement_reader_thread(void* arg){
char ruta[300];
snprintf(ruta, sizeof(ruta), "%s/bomber_moves.txt", shared_mem->data_dir);
FILE *archivo = fopen(ruta, "r");

if (!archivo) {
printf("[P1] Error: No se pudo abrir %s\n", ruta);
reader_finished = 1;
sem_post(&sem_items);
return NULL;
}

char linea[64];
while (fgets(linea, sizeof(linea), archivo) != NULL){
linea[strcspn(linea, "\r\n")] = 0;
instruction ins = parse_instruction(linea);

pthread_mutex_lock(&queue_mtx);
instruction_queue[q_tail] = ins;
q_tail = (q_tail + 1) % QUEUE_SIZE;
pthread_mutex_unlock(&queue_mtx);

sem_post(&sem_items);
}
fclose(archivo);
reader_finished = 1;
sem_post(&sem_items);
return NULL;
}

void* bomber_publisher_thread(void *arg){
while(1){
sem_wait(&sem_publish);
if (is_game_over(shared_mem) && local_moves_done){
break;
}

pthread_mutex_lock(&pos_mtx);
int final_x = local_nx;
int final_y = local_ny;
int final_done = local_moves_done;
pthread_mutex_unlock(&pos_mtx);

shared_mem->bomber_x = final_x;
shared_mem->bomber_y = final_y;
if (final_done){
shared_mem->bomber_moves_done = 1;
}

sem_post(&sem_publish_done);

if(final_done) break;
}
return NULL;
}

void* movement_executor_thread(void *arg){
local_nx = shared_mem->bomber_x;
local_ny = shared_mem->bomber_y;

while (1) {
sem_wait(&shared_mem->sem_bomber_turn);
if (is_game_over(shared_mem)) {
local_moves_done = 1;
sem_post(&sem_publish);
break;
}

int turno_gastado = 0;
while (!turno_gastado){
sem_wait(&sem_items);
pthread_mutex_lock(&queue_mtx);
if (q_head == q_tail && reader_finished){
pthread_mutex_unlock(&queue_mtx);
pthread_mutex_lock(&pos_mtx);
local_moves_done = 1;
pthread_mutex_unlock(&pos_mtx);
sem_post(&sem_publish);
sem_wait(&sem_publish_done);
turno_gastado = 1;
break;
}

instruction ins = instruction_queue[q_head];
q_head = (q_head + 1) % QUEUE_SIZE;
pthread_mutex_unlock(&queue_mtx);

int nx = local_nx;
int ny = local_ny;

switch (ins.type){
case INS_UP:
ny--;
break;
case INS_DOWN:
ny++;
break;
case INS_LEFT:
nx--;
break;
case INS_RIGHT:
nx++;
break;
case INS_BOMB:
shared_mem->bomb_request_x = local_nx;
shared_mem->bomb_request_y = local_ny;
shared_mem->bomb_request_active = 1;
turno_gastado = 1;
continue;
case INS_SET_PRIORITY:
shared_mem->pending_priority_bomber = ins.value;
shared_mem->priority_request_active = 1;
turno_gastado = 1;
continue;
case INS_INVALID:
default:
printf("[P1] Aviso: Linea invalida descartada\n");
continue;
}
if (is_walkable(shared_mem, nx, ny)){
pthread_mutex_lock(&pos_mtx);
local_nx = nx;
local_ny = ny;
pthread_mutex_unlock(&pos_mtx);

sem_post(&sem_publish);
sem_wait(&sem_publish_done);
}
turno_gastado = 1;
}

sem_post(&shared_mem->sem_turn_done);
if (local_moves_done) break;
}
return NULL;
}

void run_bomber(shared_t *sh){
shared_mem = sh;

pthread_mutex_init(&queue_mtx, NULL);
pthread_mutex_init(&pos_mtx, NULL);
sem_init(&sem_items, 0, 0);
sem_init(&sem_publish, 0, 0);
sem_init(&sem_publish_done, 0, 0);

pthread_t reader_tid, executor_tid, publisher_tid;
pthread_create(&reader_tid, NULL, movement_reader_thread, NULL);
pthread_create(&publisher_tid, NULL, bomber_publisher_thread, NULL);
pthread_create(&executor_tid, NULL, movement_executor_thread, NULL);

pthread_join(executor_tid, NULL);
pthread_join(reader_tid, NULL);
pthread_join(publisher_tid, NULL);

pthread_mutex_destroy(&queue_mtx);
pthread_mutex_destroy(&pos_mtx);
sem_destroy(&sem_items);
sem_destroy(&sem_publish);
sem_destroy(&sem_publish_done);
}







