/*
 * Experimento 2 — Buzón de bomba entre procesos (sección 8.1).
 *
 * Usa la memoria compartida REAL del proyecto (shared_t de include/shared.h) y el
 * mutex compartido bomb_req_mtx con PTHREAD_PROCESS_SHARED, como main.c.
 *
 *   Proceso hijo  (como P1): escribe pedidos (bomb_request_x, bomb_request_y) = (k, k).
 *   Proceso padre (como P3): lee el pedido y lo limpia. Si x != y, leyó un pedido
 *                            a medio escribir: una lectura rota.
 *
 *   Versión corregida:       escritura y lectura dentro de bomb_req_mtx.
 *   -DSIN_MUTEX (con falla): sin el mutex.
 *
 * Amplificación de la ventana crítica: sched_yield() entre escribir x e y
 * (el hijo cede la CPU justo en medio de la sección crítica).
 *
 * Imprime: lecturas rotas.
 */
#include <stdio.h>
#include <string.h>
#include <sched.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include "../include/shared.h"

#define PEDIDOS 20000

void bloquear(pthread_mutex_t *m)
{
#ifndef SIN_MUTEX
    pthread_mutex_lock(m);
#endif
}

void desbloquear(pthread_mutex_t *m)
{
#ifndef SIN_MUTEX
    pthread_mutex_unlock(m);
#endif
}

int main(void)
{
    shared_t *sh = mmap(NULL, sizeof(shared_t), PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    memset(sh, 0, sizeof(shared_t));

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&sh->bomb_req_mtx, &attr);

    pid_t hijo = fork();
    if (hijo == 0) {                          /* "P1": escribe pedidos */
        for (int k = 1; k <= PEDIDOS; k++) {
            bloquear(&sh->bomb_req_mtx);
            sh->bomb_request_x = k;
            sched_yield();
            sh->bomb_request_y = k;
            sh->bomb_request_active = 1;
            desbloquear(&sh->bomb_req_mtx);
        }
        sh->game_over = 1;                    /* avisa que terminó */
        _exit(0);
    }

    int rotas = 0;                            /* "P3": lee y limpia el buzón */
    while (sh->game_over == 0) {
        bloquear(&sh->bomb_req_mtx);
        if (sh->bomb_request_active) {
            if (sh->bomb_request_x != sh->bomb_request_y) {
                rotas++;
            }
            sh->bomb_request_active = 0;
        }
        desbloquear(&sh->bomb_req_mtx);
    }
    waitpid(hijo, NULL, 0);
    printf("%d\n", rotas);
    return 0;
}
