/*
 * Familia 2b - Un semaforo por enemigo (sem_start_enemy[i]).
 *
 * normal: el controlador hace sem_post(&sem_start_enemy[i]) a cada enemigo.
 * BUGGY : un unico semaforo contador compartido, posteado 4 veces por ronda.
 * Metrica: turnos atendidos por cada enemigo y desbalance (max - min).
 */
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

#define NUM_ENEMIES 4
#define ROUNDS 25000

static sem_t sem_start_enemy[NUM_ENEMIES], sem_compartido, sem_enemy_done;
static long atendidos[NUM_ENEMIES];
static volatile int fin = 0;

static void *enemy_thread(void *arg)
{
    int i = (int)(long)arg;
    while (1) {
#ifdef BUGGY
        sem_wait(&sem_compartido);
#else
        sem_wait(&sem_start_enemy[i]);
#endif
        if (fin) break;
        atendidos[i]++;
        sem_post(&sem_enemy_done);
    }
    return NULL;
}

int main(void)
{
    pthread_t e[NUM_ENEMIES];
    sem_init(&sem_compartido, 0, 0);
    sem_init(&sem_enemy_done, 0, 0);
    for (int i = 0; i < NUM_ENEMIES; i++) {
        sem_init(&sem_start_enemy[i], 0, 0);
        pthread_create(&e[i], NULL, enemy_thread, (void *)(long)i);
    }
    for (int r = 0; r < ROUNDS; r++) {
        for (int i = 0; i < NUM_ENEMIES; i++) {
#ifdef BUGGY
            sem_post(&sem_compartido);
#else
            sem_post(&sem_start_enemy[i]);
#endif
        }
        for (int i = 0; i < NUM_ENEMIES; i++) sem_wait(&sem_enemy_done);
    }
    fin = 1;
    for (int i = 0; i < NUM_ENEMIES; i++) {
        sem_post(&sem_compartido);
        sem_post(&sem_start_enemy[i]);
    }
    for (int i = 0; i < NUM_ENEMIES; i++) pthread_join(e[i], NULL);
    long mx = atendidos[0], mn = atendidos[0];
    for (int i = 1; i < NUM_ENEMIES; i++) {
        if (atendidos[i] > mx) mx = atendidos[i];
        if (atendidos[i] < mn) mn = atendidos[i];
    }
    printf("%ld %ld %ld %ld %ld\n", atendidos[0], atendidos[1], atendidos[2], atendidos[3], mx - mn);
    return 0;
}
