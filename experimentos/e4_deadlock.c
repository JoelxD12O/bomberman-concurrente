/*
 * Experimento 4 — Orden de adquisición de mutex (deadlock).
 *
 * Dos hilos que necesitan map_mtx y bomber_mtx a la vez, como explotar() de P3
 * (mapa + posición de Bomberman) y print_map() de P0.
 *
 *   Versión corregida:        los dos toman map_mtx -> bomber_mtx (orden de shared.h).
 *   -DSIN_ORDEN (con falla):  el segundo hilo los toma al revés (bomber -> map),
 *                             como pasaba en P3 en la semana 1.
 *
 * Si hay espera circular, el programa no termina: se mide con un timeout externo.
 * Imprime "ok" si terminó.
 */
#include <stdio.h>
#include <pthread.h>

#define VUELTAS 100000

pthread_mutex_t map_mtx    = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t bomber_mtx = PTHREAD_MUTEX_INITIALIZER;
int celdas_actualizadas = 0;

void *hilo_p3(void *arg)              /* explotar(): map -> bomber */
{
    for (int i = 0; i < VUELTAS; i++) {
        pthread_mutex_lock(&map_mtx);
        pthread_mutex_lock(&bomber_mtx);
        celdas_actualizadas++;
        pthread_mutex_unlock(&bomber_mtx);
        pthread_mutex_unlock(&map_mtx);
    }
    return arg;
}

void *hilo_p0(void *arg)              /* print_map() */
{
    for (int i = 0; i < VUELTAS; i++) {
#ifdef SIN_ORDEN
        pthread_mutex_lock(&bomber_mtx);   /* al revés: bomber -> map */
        pthread_mutex_lock(&map_mtx);
#else
        pthread_mutex_lock(&map_mtx);      /* mismo orden que P3 */
        pthread_mutex_lock(&bomber_mtx);
#endif
        celdas_actualizadas++;
        pthread_mutex_unlock(&bomber_mtx);
        pthread_mutex_unlock(&map_mtx);
    }
    return arg;
}

int main(void)
{
    pthread_t a, b;
    pthread_create(&a, NULL, hilo_p3, NULL);
    pthread_create(&b, NULL, hilo_p0, NULL);
    pthread_join(a, NULL);
    pthread_join(b, NULL);
    printf("ok %d\n", celdas_actualizadas);
    return 0;
}
