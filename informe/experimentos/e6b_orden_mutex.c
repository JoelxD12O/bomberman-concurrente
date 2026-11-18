/*
 * Familia 6b - Orden global de adquisicion de mutex (shared.h).
 *
 * explosion_thread de P3 toma map_mtx -> ... -> events_mtx -> state_mtx.
 * event_thread de P0 (process_events) necesita map_mtx, events_mtx y state_mtx.
 *   normal: P0 respeta el mismo orden: map_mtx -> events_mtx -> state_mtx.
 *   BUGGY : P0 toma events_mtx -> state_mtx -> map_mtx (orden invertido).
 * Los dos procesos corren sin turnos (peor caso) K veces cada uno.
 * Metrica: si terminan (y en cuanto tiempo) o quedan en interbloqueo (vigilante 1 s).
 */
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define K 20000

typedef struct {
    pthread_mutex_t map_mtx, events_mtx, state_mtx;
    volatile int go, blocks, events, lives;
} shared_t;

static double ahora_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

int main(void)
{
    shared_t *sh = mmap(NULL, sizeof *sh, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    pthread_mutexattr_t a;
    pthread_mutexattr_init(&a);
    pthread_mutexattr_setpshared(&a, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&sh->map_mtx, &a);
    pthread_mutex_init(&sh->events_mtx, &a);
    pthread_mutex_init(&sh->state_mtx, &a);

    pid_t p3 = fork();
    if (p3 == 0) {                                  /* P3: explosion_thread */
        while (!sh->go) { }
        for (int k = 0; k < K; k++) {
            pthread_mutex_lock(&sh->map_mtx);
            sh->blocks++;
            pthread_mutex_lock(&sh->events_mtx);
            pthread_mutex_lock(&sh->state_mtx);
            sh->events++;
            pthread_mutex_unlock(&sh->state_mtx);
            pthread_mutex_unlock(&sh->events_mtx);
            pthread_mutex_unlock(&sh->map_mtx);
        }
        _exit(0);
    }
    pid_t p0 = fork();
    if (p0 == 0) {                                  /* P0: event_thread */
        while (!sh->go) { }
        for (int k = 0; k < K; k++) {
#ifndef BUGGY
            pthread_mutex_lock(&sh->map_mtx);
            int b = sh->blocks;
            pthread_mutex_lock(&sh->events_mtx);
            pthread_mutex_lock(&sh->state_mtx);
            sh->lives = b + sh->events;
            pthread_mutex_unlock(&sh->state_mtx);
            pthread_mutex_unlock(&sh->events_mtx);
            pthread_mutex_unlock(&sh->map_mtx);
#else
            pthread_mutex_lock(&sh->events_mtx);
            pthread_mutex_lock(&sh->state_mtx);
            pthread_mutex_lock(&sh->map_mtx);
            int b = sh->blocks;
            sh->lives = b + sh->events;
            pthread_mutex_unlock(&sh->map_mtx);
            pthread_mutex_unlock(&sh->state_mtx);
            pthread_mutex_unlock(&sh->events_mtx);
#endif
        }
        _exit(0);
    }

    double t0 = ahora_ms();
    sh->go = 1;
    int vivos = 2;
    while (vivos > 0 && ahora_ms() - t0 < 1000.0) {   /* vigilante: 1 s */
        if (waitpid(-1, NULL, WNOHANG) > 0) vivos--;
        else usleep(200);
    }
    double t = ahora_ms() - t0;
    if (vivos > 0) {                                   /* interbloqueo */
        kill(p0, SIGKILL);
        kill(p3, SIGKILL);
        while (waitpid(-1, NULL, 0) > 0) { }
        printf("0 %.3f\n", t);
    } else {
        printf("1 %.3f\n", t);
    }
    return 0;
}
