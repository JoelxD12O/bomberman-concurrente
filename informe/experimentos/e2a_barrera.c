/*
 * Familia 2a - Barrera de P2 antes de collision_thread.
 *
 * enemy_controller_thread despierta a los 4 enemy_thread y, en la version
 * normal, espera sem_enemy_done una vez por cada enemigo vivo antes de
 * despertar a collision_thread. En BUGGY despierta a collision_thread de
 * inmediato (sin barrera).
 * Metrica: turnos en los que collision_thread leyo una posicion que todavia
 *          no era la final del turno (lectura sucia).
 */
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

#define NUM_ENEMIES 4
#define TURNS 200

static int pos[NUM_ENEMIES];
static int turno = 0;
static pthread_mutex_t local_pos_mtx = PTHREAD_MUTEX_INITIALIZER;
static sem_t sem_start_enemy[NUM_ENEMIES], sem_enemy_done;
static sem_t sem_start_collision, sem_collision_done;
static int lecturas_sucias = 0;

static void *enemy_thread(void *arg)
{
    int i = (int)(long)arg;
    for (int t = 1; t <= TURNS; t++) {
        sem_wait(&sem_start_enemy[i]);
        /* leer la linea del archivo, parsear, is_walkable... */
        for (volatile int k = 0; k < 3000; k++) { }
        pthread_mutex_lock(&local_pos_mtx);
        pos[i] = t;                       /* posicion final de este turno */
        pthread_mutex_unlock(&local_pos_mtx);
        sem_post(&sem_enemy_done);
    }
    return NULL;
}

static void *collision_thread(void *arg)
{
    (void)arg;
    for (int t = 1; t <= TURNS; t++) {
        sem_wait(&sem_start_collision);
        int sucia = 0;
        pthread_mutex_lock(&local_pos_mtx);
        for (int i = 0; i < NUM_ENEMIES; i++)
            if (pos[i] != turno) sucia = 1;
        pthread_mutex_unlock(&local_pos_mtx);
        lecturas_sucias += sucia;
        sem_post(&sem_collision_done);
    }
    return NULL;
}

int main(void)
{
    pthread_t e[NUM_ENEMIES], c;
    sem_init(&sem_enemy_done, 0, 0);
    sem_init(&sem_start_collision, 0, 0);
    sem_init(&sem_collision_done, 0, 0);
    for (int i = 0; i < NUM_ENEMIES; i++) {
        sem_init(&sem_start_enemy[i], 0, 0);
        pthread_create(&e[i], NULL, enemy_thread, (void *)(long)i);
    }
    pthread_create(&c, NULL, collision_thread, NULL);

    for (int t = 1; t <= TURNS; t++) {           /* enemy_controller_thread */
        turno = t;
        for (int i = 0; i < NUM_ENEMIES; i++) sem_post(&sem_start_enemy[i]);
#ifndef BUGGY
        for (int i = 0; i < NUM_ENEMIES; i++) sem_wait(&sem_enemy_done);   /* barrera */
#endif
        sem_post(&sem_start_collision);
        sem_wait(&sem_collision_done);
#ifdef BUGGY
        for (int i = 0; i < NUM_ENEMIES; i++) sem_wait(&sem_enemy_done);
#endif
    }
    for (int i = 0; i < NUM_ENEMIES; i++) pthread_join(e[i], NULL);
    pthread_join(c, NULL);
    printf("%d\n", lecturas_sucias);
    return 0;
}
