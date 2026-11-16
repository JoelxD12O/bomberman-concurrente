/*
 * Familia 4 - Buzon de bomba (bomb_request_x, bomb_request_y, bomb_request_active).
 *
 * P1 escribe el pedido (x, y, active) y P3 lo lee y baja la bandera.
 * El pedido son VARIOS campos que deben leerse juntos. P1 siempre escribe x == y.
 *   normal: P1 y P3 toman bomb_req_mtx (PTHREAD_PROCESS_SHARED).
 *   BUGGY : sin mutex.
 * Metrica: lecturas rotas (P3 ve x != y: un campo nuevo con otro viejo).
 */
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define WRITES 200000

typedef struct {
    pthread_mutex_t bomb_req_mtx;
    volatile int bomb_request_x, bomb_request_y, bomb_request_active;
    volatile int go, done;
} shared_t;

static void lock(shared_t *sh)
{
#ifndef BUGGY
    pthread_mutex_lock(&sh->bomb_req_mtx);
#else
    (void)sh;
#endif
}

static void unlock(shared_t *sh)
{
#ifndef BUGGY
    pthread_mutex_unlock(&sh->bomb_req_mtx);
#else
    (void)sh;
#endif
}

int main(void)
{
    shared_t *sh = mmap(NULL, sizeof *sh, PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    pthread_mutexattr_t a;
    pthread_mutexattr_init(&a);
    pthread_mutexattr_setpshared(&a, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&sh->bomb_req_mtx, &a);

    pid_t p1 = fork();
    if (p1 == 0) {                         /* P1: escribe pedidos */
        while (!sh->go) { }
        for (int k = 1; k <= WRITES; k++) {
            lock(sh);
            sh->bomb_request_x = k;
            sh->bomb_request_y = k;
            sh->bomb_request_active = 1;
            unlock(sh);
        }
        sh->done = 1;
        _exit(0);
    }

    long rotas = 0;                        /* P3: atiende pedidos */
    sh->go = 1;
    while (!sh->done) {
        lock(sh);
        if (sh->bomb_request_active) {
            int x = sh->bomb_request_x;
            int y = sh->bomb_request_y;
            if (x != y) rotas++;
            sh->bomb_request_active = 0;
        }
        unlock(sh);
    }
    waitpid(p1, NULL, 0);
    printf("%ld\n", rotas);
    return 0;
}
