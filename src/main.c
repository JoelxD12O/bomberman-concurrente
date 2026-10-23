/*
 * main.c — arranque del sistema (P0, sección 4).
 *
 * Ya funciona: crea la memoria compartida, los semáforos y mutex, carga el mapa,
 * crea P1, P2, P3 (y P4 con -r) y al final libera todo.
 * Joel: revisar y entender cada línea; aquí se defienden las preguntas de IPC.
 *
 * Uso: ./bomberman <carpeta_caso> <max_ticks> [-r]
 *   ej: ./bomberman tests/caso_ejemplo 20
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include "procs.h"

static void init_sync(shared_t *sh)
{
    /* pshared = 1: el semáforo vive en memoria compartida y sirve entre procesos */
    sem_init(&sh->sem_bomber_turn, 1, 0);
    sem_init(&sh->sem_enemy_turn,  1, 0);
    sem_init(&sh->sem_bomb_turn,   1, 0);
    sem_init(&sh->sem_turn_done,   1, 0);
    sem_init(&sh->sem_render,      1, 0);

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&sh->map_mtx,      &attr);
    pthread_mutex_init(&sh->bomber_mtx,   &attr);
    pthread_mutex_init(&sh->enemy_mtx,    &attr);
    pthread_mutex_init(&sh->bomb_req_mtx, &attr);
    pthread_mutex_init(&sh->prio_mtx,     &attr);
    pthread_mutex_init(&sh->events_mtx,   &attr);
    pthread_mutex_init(&sh->state_mtx,    &attr);
    pthread_mutexattr_destroy(&attr);
}

static void destroy_sync(shared_t *sh)
{
    sem_destroy(&sh->sem_bomber_turn);
    sem_destroy(&sh->sem_enemy_turn);
    sem_destroy(&sh->sem_bomb_turn);
    sem_destroy(&sh->sem_turn_done);
    sem_destroy(&sh->sem_render);
    pthread_mutex_destroy(&sh->map_mtx);
    pthread_mutex_destroy(&sh->bomber_mtx);
    pthread_mutex_destroy(&sh->enemy_mtx);
    pthread_mutex_destroy(&sh->bomb_req_mtx);
    pthread_mutex_destroy(&sh->prio_mtx);
    pthread_mutex_destroy(&sh->events_mtx);
    pthread_mutex_destroy(&sh->state_mtx);
}

static pid_t spawn(void (*fn)(shared_t *), shared_t *sh)
{
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(1); }
    if (pid == 0) {          /* hijo: corre su proceso y termina */
        fn(sh);
        fflush(stdout);      /* _exit no vacía el buffer de stdio */
        _exit(0);
    }
    return pid;
}

int is_game_over(shared_t *sh)
{
    pthread_mutex_lock(&sh->state_mtx);
    int over = sh->game_over;
    pthread_mutex_unlock(&sh->state_mtx);
    return over;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "uso: %s <carpeta_caso> <max_ticks> [-r]\n", argv[0]);
        return 1;
    }
    int with_renderer = (argc > 3 && strcmp(argv[3], "-r") == 0);
    setvbuf(stdout, NULL, _IOLBF, 0);   /* línea a línea: logs de procesos en orden */

    /* MAP_SHARED | MAP_ANONYMOUS: los hijos creados con fork heredan esta región */
    shared_t *sh = mmap(NULL, sizeof(shared_t), PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (sh == MAP_FAILED) { perror("mmap"); return 1; }
    memset(sh, 0, sizeof(shared_t));

    snprintf(sh->data_dir, sizeof(sh->data_dir), "%s", argv[1]);
    sh->max_ticks        = atoi(argv[2]);
    sh->bomber_lives     = INITIAL_LIVES;
    sh->prioridad_bomber = 20;      /* sección 9 */
    sh->prioridad_enemy  = 30;
    sh->enemies_alive    = NUM_ENEMIES;
    for (int i = 0; i < NUM_ENEMIES; i++) sh->enemy_alive[i] = 1;

    init_sync(sh);

    char path[300];
    snprintf(path, sizeof(path), "%s/map.txt", sh->data_dir);
    if (load_map(sh, path) != 0) {
        fprintf(stderr, "mapa inválido: %s\n", path);
        return 1;
    }

    pid_t p1 = spawn(run_bomber,  sh);
    pid_t p2 = spawn(run_enemies, sh);
    pid_t p3 = spawn(run_bombs,   sh);
    pid_t p4 = with_renderer ? spawn(run_renderer, sh) : -1;

    run_scheduler(sh);    /* P0 corre en el proceso padre */

    waitpid(p1, NULL, 0);
    waitpid(p2, NULL, 0);
    waitpid(p3, NULL, 0);
    if (p4 > 0) waitpid(p4, NULL, 0);

    printf("\n=== FIN: resultado=%d tick=%d vidas=%d score=%d ===\n",
           sh->game_result, sh->global_tick, sh->bomber_lives, sh->score);

    destroy_sync(sh);
    munmap(sh, sizeof(shared_t));
    return 0;
}
