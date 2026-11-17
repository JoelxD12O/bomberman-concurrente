/*
 * Experimento 3 — Orden entre bomber_tracker_thread y collision_thread (P2).
 *
 * Reproduce el final del turno de P2 en src/enemy.c: el tracker copia la posición
 * de Bomberman y collision la compara con la de cada enemigo.
 *
 * Escenario: en cada turno Bomberman se movió (en el turno de P1) y un enemigo
 * entra a su celda, así que SIEMPRE debería detectarse un choque.
 *
 *   Versión corregida:        el controller despierta al tracker, espera, y recién
 *                             después despierta a collision.
 *   -DSIN_ORDEN (con falla):  los despierta a la vez (como el PR #21 antes del #26);
 *                             collision puede usar la copia del turno anterior.
 *
 * Amplificación: el tracker hace sched_yield() antes de copiar.
 *
 * Imprime: choques perdidos.
 */
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <sched.h>

#define TURNOS 2000

int bomber_x = 0;                 /* posición publicada por P1 */
int enemigo_x = 0;                /* posición interna del enemigo */
int copia_bx = -1;                /* copia del tracker */
int detectados = 0;
int seguir = 1;

pthread_mutex_t local_pos_mtx = PTHREAD_MUTEX_INITIALIZER;
sem_t go_tracker, fin_tracker, go_collision, fin_collision;

void *tracker(void *arg)
{
    while (1) {
        sem_wait(&go_tracker);
        if (!seguir) break;
        sched_yield();
        pthread_mutex_lock(&local_pos_mtx);
        copia_bx = bomber_x;
        pthread_mutex_unlock(&local_pos_mtx);
        sem_post(&fin_tracker);
    }
    return arg;
}

void *collision(void *arg)
{
    while (1) {
        sem_wait(&go_collision);
        if (!seguir) break;
        pthread_mutex_lock(&local_pos_mtx);
        if (enemigo_x == copia_bx) {
            detectados++;
        }
        pthread_mutex_unlock(&local_pos_mtx);
        sem_post(&fin_collision);
    }
    return arg;
}

int main(void)
{
    sem_init(&go_tracker, 0, 0);
    sem_init(&fin_tracker, 0, 0);
    sem_init(&go_collision, 0, 0);
    sem_init(&fin_collision, 0, 0);

    pthread_t h1, h2;
    pthread_create(&h1, NULL, tracker, NULL);
    pthread_create(&h2, NULL, collision, NULL);

    for (int t = 1; t <= TURNOS; t++) {
        pthread_mutex_lock(&local_pos_mtx);
        bomber_x = t;              /* Bomberman se movió en el turno de P1 */
        enemigo_x = t;             /* el enemigo entra a su celda */
        pthread_mutex_unlock(&local_pos_mtx);

#ifdef SIN_ORDEN
        sem_post(&go_tracker);     /* los dos a la vez */
        sem_post(&go_collision);
        sem_wait(&fin_tracker);
        sem_wait(&fin_collision);
#else
        sem_post(&go_tracker);     /* primero el tracker... */
        sem_wait(&fin_tracker);
        sem_post(&go_collision);   /* ...después collision */
        sem_wait(&fin_collision);
#endif
    }

    seguir = 0;
    sem_post(&go_tracker);
    sem_post(&go_collision);
    pthread_join(h1, NULL);
    pthread_join(h2, NULL);

    printf("%d\n", TURNOS - detectados);
    return 0;
}
