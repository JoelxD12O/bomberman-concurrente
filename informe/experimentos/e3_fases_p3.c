/*
 * Familia 3 - Orden de las fases de P3 (fuse_thread -> explosion_thread).
 *
 * bomb_controller_thread despierta a fuse_thread, ESPERA sem_fuse_done, y solo
 * entonces despierta a explosion_thread. En BUGGY despierta a los dos a la vez.
 * Ambos hilos usan bombs_mtx, asi que no hay carrera de datos: lo que se pierde
 * es el ORDEN. Una bomba registrada en el tick t debe explotar en t + 3.
 * Metrica: bombas que explotan en un tick distinto de t + 3.
 */
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

#define BOMB_TIMER 3
#define TICKS 800

typedef struct { int timer, active, tick_registro; } Bomb;
static Bomb bomba;
static pthread_mutex_t bombs_mtx = PTHREAD_MUTEX_INITIALIZER;
static sem_t sem_fuse_go, sem_fuse_done, sem_explosion_go, sem_explosion_done;
static int tick_actual = 0, explosiones = 0, fuera_de_tiempo = 0;
static volatile int terminar = 0;

static void *fuse_thread(void *arg)
{
    (void)arg;
    while (1) {
        sem_wait(&sem_fuse_go);
        if (terminar) break;
        for (volatile int k = 0; k < 2000; k++) { }   /* recorrer la tabla */
        pthread_mutex_lock(&bombs_mtx);
        if (bomba.active && bomba.timer > 0) bomba.timer--;
        pthread_mutex_unlock(&bombs_mtx);
        sem_post(&sem_fuse_done);
    }
    return NULL;
}

static void *explosion_thread(void *arg)
{
    (void)arg;
    while (1) {
        sem_wait(&sem_explosion_go);
        if (terminar) break;
        pthread_mutex_lock(&bombs_mtx);
        if (bomba.active && bomba.timer <= 0) {
            bomba.active = 0;
            explosiones++;
            if (tick_actual != bomba.tick_registro + BOMB_TIMER) fuera_de_tiempo++;
        }
        pthread_mutex_unlock(&bombs_mtx);
        sem_post(&sem_explosion_done);
    }
    return NULL;
}

int main(void)
{
    pthread_t f, x;
    sem_init(&sem_fuse_go, 0, 0);
    sem_init(&sem_fuse_done, 0, 0);
    sem_init(&sem_explosion_go, 0, 0);
    sem_init(&sem_explosion_done, 0, 0);
    pthread_create(&f, NULL, fuse_thread, NULL);
    pthread_create(&x, NULL, explosion_thread, NULL);

    for (int t = 1; t <= TICKS; t++) {           /* bomb_controller_thread */
        tick_actual = t;
#ifndef BUGGY
        sem_post(&sem_fuse_go);      sem_wait(&sem_fuse_done);
        sem_post(&sem_explosion_go); sem_wait(&sem_explosion_done);
#else
        sem_post(&sem_fuse_go);      sem_post(&sem_explosion_go);
        sem_wait(&sem_fuse_done);    sem_wait(&sem_explosion_done);
#endif
        pthread_mutex_lock(&bombs_mtx);           /* registrar_solicitudes */
        if (!bomba.active) {
            bomba.active = 1;
            bomba.timer = BOMB_TIMER;
            bomba.tick_registro = t;
        }
        pthread_mutex_unlock(&bombs_mtx);
    }
    terminar = 1;
    sem_post(&sem_fuse_go);
    sem_post(&sem_explosion_go);
    pthread_join(f, NULL);
    pthread_join(x, NULL);
    printf("%d %d\n", fuera_de_tiempo, explosiones);
    return 0;
}
