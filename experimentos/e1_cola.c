/*
 * Experimento 1 — Cola de instrucciones de P1 (productor-consumidor).
 *
 * Reproduce movement_reader_thread y movement_executor_thread de src/bomber.c:
 * el lector mete N instrucciones en una cola circular de QUEUE_SIZE casilleros
 * y el ejecutor saca una por turno.
 *
 *   Versión corregida:          queue_mtx + sem_items + sem_espacios
 *   -DSIN_ESPACIOS (con falla): sin sem_espacios, como el PR #22 antes de corregirlo.
 *                               Si el lector va más rápido, da la vuelta a la cola y
 *                               escribe encima de instrucciones que aún no se ejecutaron.
 *
 * Imprime: instrucciones perdidas (nunca ejecutadas).
 */
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define N          300     /* instrucciones del archivo (como val/bomber_largo) */
#define QUEUE_SIZE 100     /* igual que en bomber.c */

int cola[QUEUE_SIZE];
int q_head = 0;
int q_tail = 0;
pthread_mutex_t queue_mtx = PTHREAD_MUTEX_INITIALIZER;
sem_t sem_items;
sem_t sem_espacios;

int ejecutada[N];          /* ejecutada[k] = 1 si la instrucción k llegó al ejecutor */

void *lector(void *arg)
{
    for (int k = 0; k < N; k++) {
#ifndef SIN_ESPACIOS
        sem_wait(&sem_espacios);        /* si la cola está llena, espera */
#endif
        pthread_mutex_lock(&queue_mtx);
        cola[q_tail] = k;
        q_tail = (q_tail + 1) % QUEUE_SIZE;
        pthread_mutex_unlock(&queue_mtx);
        sem_post(&sem_items);
    }
    return arg;
}

void *ejecutor(void *arg)
{
    for (int k = 0; k < N; k++) {
        sem_wait(&sem_items);
        pthread_mutex_lock(&queue_mtx);
        int instruccion = cola[q_head];
        q_head = (q_head + 1) % QUEUE_SIZE;
        pthread_mutex_unlock(&queue_mtx);
#ifndef SIN_ESPACIOS
        sem_post(&sem_espacios);        /* se liberó un casillero */
#endif
        ejecutada[instruccion] = 1;
        usleep(20);                     /* un turno: P0 tarda en volver a darle el turno */
    }
    return arg;
}

int main(void)
{
    sem_init(&sem_items, 0, 0);
    sem_init(&sem_espacios, 0, QUEUE_SIZE);

    pthread_t h_lector, h_ejecutor;
    pthread_create(&h_ejecutor, NULL, ejecutor, NULL);
    pthread_create(&h_lector, NULL, lector, NULL);
    pthread_join(h_lector, NULL);
    pthread_join(h_ejecutor, NULL);

    int perdidas = 0;
    for (int k = 0; k < N; k++) {
        if (ejecutada[k] == 0) {
            perdidas++;
        }
    }
    printf("%d\n", perdidas);
    return 0;
}
