/*
 * Familia 6a - Cierre ordenado (signal_thread de P0 + bomb_controller_thread de P3).
 *
 * Al terminar el juego P1, P2 y P3 estan dormidos en sem_wait esperando turno,
 * y dentro de P3 fuse_thread y explosion_thread esperan su propio semaforo.
 *   normal: P0 hace sem_post a sem_bomber_turn, sem_enemy_turn y sem_bomb_turn
 *           antes de waitpid; P3 pone terminar = 1 y despierta a sus hilos.
 *   BUGGY : P0 marca game_over y llama a waitpid sin despertar a nadie.
 * Metrica: si el programa termina y en cuanto tiempo (vigilante interno de 1 s).
 */
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define TICKS 20

typedef struct {
    volatile int game_over;
    sem_t sem_bomber_turn, sem_enemy_turn, sem_bomb_turn, sem_turn_done;
} shared_t;

static shared_t *sh;

static void hijo_simple(sem_t *mi_turno)
{
    while (1) {
        sem_wait(mi_turno);
        if (sh->game_over) break;
        sem_post(&sh->sem_turn_done);
    }
    _exit(0);
}

/* ---- P3 con sus hilos internos ---- */
static sem_t fuse_go, fuse_done, expl_go, expl_done;
static volatile int terminar = 0;

static void *trabajador(void *arg)
{
    sem_t **s = arg;
    while (1) {
        sem_wait(s[0]);
        if (terminar) break;
        sem_post(s[1]);
    }
    return NULL;
}

static void p3(void)
{
    sem_init(&fuse_go, 0, 0);
    sem_init(&fuse_done, 0, 0);
    sem_init(&expl_go, 0, 0);
    sem_init(&expl_done, 0, 0);
    sem_t *a[2] = { &fuse_go, &fuse_done };
    sem_t *b[2] = { &expl_go, &expl_done };
    pthread_t f, x;
    pthread_create(&f, NULL, trabajador, a);
    pthread_create(&x, NULL, trabajador, b);
    while (1) {                               /* bomb_controller_thread */
        sem_wait(&sh->sem_bomb_turn);
        if (sh->game_over) break;
        sem_post(&fuse_go); sem_wait(&fuse_done);
        sem_post(&expl_go); sem_wait(&expl_done);
        sem_post(&sh->sem_turn_done);
    }
#ifndef BUGGY
    terminar = 1;
    sem_post(&fuse_go);
    sem_post(&expl_go);
#endif
    pthread_join(f, NULL);
    pthread_join(x, NULL);
    _exit(0);
}

/* ---- vigilante: si en 1 s no termino, es un interbloqueo ---- */
static pid_t hijos[3];
static double t0;

static double ahora_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}

static void vigilante(int sig)
{
    (void)sig;
    for (int i = 0; i < 3; i++) kill(hijos[i], SIGKILL);
    printf("0 %.3f\n", ahora_ms() - t0);    /* 0 = no cerro */
    fflush(stdout);
    _exit(0);
}

int main(void)
{
    t0 = ahora_ms();
    sh = mmap(NULL, sizeof *sh, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sem_init(&sh->sem_bomber_turn, 1, 0);
    sem_init(&sh->sem_enemy_turn, 1, 0);
    sem_init(&sh->sem_bomb_turn, 1, 0);
    sem_init(&sh->sem_turn_done, 1, 0);

    pid_t h1 = fork(); if (h1 == 0) hijo_simple(&sh->sem_bomber_turn);
    pid_t h2 = fork(); if (h2 == 0) hijo_simple(&sh->sem_enemy_turn);
    pid_t h3 = fork(); if (h3 == 0) p3();
    hijos[0] = h1; hijos[1] = h2; hijos[2] = h3;
    signal(SIGALRM, vigilante);
    alarm(1);

    for (int t = 1; t <= TICKS; t++) {       /* P0: lock-step */
        sem_post(t % 2 ? &sh->sem_bomber_turn : &sh->sem_enemy_turn);
        sem_wait(&sh->sem_turn_done);
        sem_post(&sh->sem_bomb_turn);
        sem_wait(&sh->sem_turn_done);
    }
    sh->game_over = 1;
#ifndef BUGGY
    sem_post(&sh->sem_bomber_turn);           /* signal_thread despierta a todos */
    sem_post(&sh->sem_enemy_turn);
    sem_post(&sh->sem_bomb_turn);
#endif
    waitpid(h1, NULL, 0);
    waitpid(h2, NULL, 0);
    waitpid(h3, NULL, 0);
    printf("1 %.3f\n", ahora_ms() - t0);     /* 1 = cerro */
    return 0;
}
