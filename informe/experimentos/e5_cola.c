/*
 * Familia 5 - Cola acotada de P1 (movement_reader_thread -> movement_executor_thread).
 *
 * instruction_queue[QUEUE_SIZE], q_head, q_tail con queue_mtx,
 * sem_items (instrucciones listas) y sem_espacios (casilleros libres).
 *   normal: el lector hace sem_wait(&sem_espacios) antes de escribir.
 *   BUGGY : sin sem_espacios: el lector (rapido, lee todo el archivo) da la
 *           vuelta al buffer y sobrescribe instrucciones que el ejecutor
 *           (1 por turno) todavia no consumio.
 * Metrica: instrucciones perdidas (nunca ejecutadas).
 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

#define QUEUE_SIZE 100
#define N 200000

static int instruction_queue[QUEUE_SIZE];
static int q_head = 0, q_tail = 0;
static pthread_mutex_t queue_mtx = PTHREAD_MUTEX_INITIALIZER;
static sem_t sem_items, sem_espacios;
static unsigned char *ejecutada;

static void *movement_reader_thread(void *arg)
{
    (void)arg;
    for (int k = 0; k < N; k++) {
#ifndef BUGGY
        sem_wait(&sem_espacios);
#endif
        pthread_mutex_lock(&queue_mtx);
        instruction_queue[q_tail] = k;
        q_tail = (q_tail + 1) % QUEUE_SIZE;
        pthread_mutex_unlock(&queue_mtx);
        sem_post(&sem_items);
    }
    return NULL;
}

static void *movement_executor_thread(void *arg)
{
    (void)arg;
    for (int c = 0; c < N; c++) {
        sem_wait(&sem_items);
        pthread_mutex_lock(&queue_mtx);
        int ins = instruction_queue[q_head];
        q_head = (q_head + 1) % QUEUE_SIZE;
        pthread_mutex_unlock(&queue_mtx);
        sem_post(&sem_espacios);
        for (volatile int k = 0; k < 200; k++) { }   /* ejecutar 1 instruccion */
        ejecutada[ins] = 1;
    }
    return NULL;
}

int main(void)
{
    ejecutada = calloc(N, 1);
    sem_init(&sem_items, 0, 0);
    sem_init(&sem_espacios, 0, QUEUE_SIZE);
    pthread_t r, e;
    pthread_create(&r, NULL, movement_reader_thread, NULL);
    pthread_create(&e, NULL, movement_executor_thread, NULL);
    pthread_join(r, NULL);
    pthread_join(e, NULL);
    int perdidas = 0;
    for (int k = 0; k < N; k++)
        if (!ejecutada[k]) perdidas++;
    printf("%d\n", perdidas);
    return 0;
}
