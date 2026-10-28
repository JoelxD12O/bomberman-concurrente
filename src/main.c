/*
 * main.c — arranque del juego (sección 4). Joel.
 *
 * Pasos:
 *   1. Crear la "pizarra" (memoria compartida) que ven todos los procesos.
 *   2. Ponerle los valores iniciales (vidas, prioridades, ...).
 *   3. Crear los semáforos y mutex (también viven en la pizarra).
 *   4. Leer el mapa.
 *   5. Crear P1, P2 y P3 con fork (y P4 si se usa -r).
 *   6. Correr P0 (el árbitro) en este mismo proceso.
 *   7. Cuando el juego termina: esperar a los hijos y liberar todo.
 *
 * Uso: ./bomberman <carpeta_del_caso> <max_ticks> [-r]
 *   ej: ./bomberman tests/caso_ejemplo 20
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include "procs.h"

/* Crea los semáforos y mutex. El segundo argumento de sem_init en 1 significa
 * "este semáforo lo van a usar varios PROCESOS", no solo hilos. */
static void crear_sincronizacion(shared_t *sh)
{
    /* sem_init(semáforo, compartido_entre_procesos, valor_inicial) */
    sem_init(&sh->sem_bomber_turn, 1, 0);
    sem_init(&sh->sem_enemy_turn,  1, 0);
    sem_init(&sh->sem_bomb_turn,   1, 0);
    sem_init(&sh->sem_turn_done,   1, 0);
    sem_init(&sh->sem_render,      1, 0);

    /* Un mutex normal solo sirve entre hilos del mismo proceso.
     * Con PTHREAD_PROCESS_SHARED también sirve entre procesos distintos. */
    pthread_mutexattr_t opciones;
    pthread_mutexattr_init(&opciones);
    pthread_mutexattr_setpshared(&opciones, PTHREAD_PROCESS_SHARED);

    pthread_mutex_init(&sh->map_mtx,      &opciones);
    pthread_mutex_init(&sh->bomber_mtx,   &opciones);
    pthread_mutex_init(&sh->enemy_mtx,    &opciones);
    pthread_mutex_init(&sh->bomb_req_mtx, &opciones);
    pthread_mutex_init(&sh->prio_mtx,     &opciones);
    pthread_mutex_init(&sh->events_mtx,   &opciones);
    pthread_mutex_init(&sh->state_mtx,    &opciones);

    pthread_mutexattr_destroy(&opciones);
}

static void destruir_sincronizacion(shared_t *sh)
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

/* Lo llama cada hijo cuando termina. Se usa _exit (y no exit) para que el hijo
 * no ejecute la limpieza que le corresponde al padre. Antes se vacía printf. */
static void terminar_hijo(void)
{
    fflush(stdout);
    _exit(0);
}

/* Devuelve 1 si el juego terminó. La usan todos los procesos. */
int is_game_over(shared_t *sh)
{
    pthread_mutex_lock(&sh->state_mtx);
    int terminado = sh->game_over;
    pthread_mutex_unlock(&sh->state_mtx);
    return terminado;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        printf("uso: %s <carpeta_del_caso> <max_ticks> [-r]\n", argv[0]);
        return 1;
    }

    int usar_renderer = 0;
    if (argc > 3 && strcmp(argv[3], "-r") == 0) {
        usar_renderer = 1;
    }

    /* Que printf escriba cada línea al momento; así los mensajes de los
     * distintos procesos salen en orden. */
    setvbuf(stdout, NULL, _IOLBF, 0);

    /* ---- 1. Crear la pizarra compartida ----
     * mmap reserva memoria. Con MAP_SHARED, después del fork el padre y los
     * hijos ven LA MISMA memoria (sin esto, cada hijo tendría su propia copia). */
    shared_t *sh = mmap(NULL, sizeof(shared_t), PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (sh == MAP_FAILED) {
        printf("error: no se pudo crear la memoria compartida\n");
        return 1;
    }
    memset(sh, 0, sizeof(shared_t));   /* todo en 0 al empezar */

    /* ---- 2. Valores iniciales ---- */
    strncpy(sh->data_dir, argv[1], sizeof(sh->data_dir) - 1);
    sh->max_ticks        = atoi(argv[2]);
    sh->bomber_lives     = INITIAL_LIVES;
    sh->prioridad_bomber = 20;   /* sección 9 */
    sh->prioridad_enemy  = 30;
    sh->enemies_alive    = NUM_ENEMIES;
    for (int i = 0; i < NUM_ENEMIES; i++) {
        sh->enemy_alive[i] = 1;
    }

    /* ---- 3. Semáforos y mutex ---- */
    crear_sincronizacion(sh);

    /* ---- 4. Leer el mapa ---- */
    char ruta_mapa[300];
    snprintf(ruta_mapa, sizeof(ruta_mapa), "%s/map.txt", sh->data_dir);
    if (load_map(sh, ruta_mapa) != 0) {
        printf("error: mapa inválido en %s\n", ruta_mapa);
        return 1;
    }

    /* ---- 5. Crear los procesos hijos ----
     * fork() duplica el programa. En el hijo devuelve 0; en el padre
     * devuelve el número (pid) del hijo. */
    pid_t p1 = fork();
    if (p1 == 0) {
        run_bomber(sh);       /* esto solo lo ejecuta el hijo P1 */
        terminar_hijo();
    }

    pid_t p2 = fork();
    if (p2 == 0) {
        run_enemies(sh);      /* hijo P2 */
        terminar_hijo();
    }

    pid_t p3 = fork();
    if (p3 == 0) {
        run_bombs(sh);        /* hijo P3 */
        terminar_hijo();
    }

    pid_t p4 = 0;
    if (usar_renderer) {
        p4 = fork();
        if (p4 == 0) {
            run_renderer(sh); /* hijo P4 (bonus) */
            terminar_hijo();
        }
    }

    /* ---- 6. P0 corre aquí, en el proceso padre ---- */
    run_scheduler(sh);

    /* ---- 7. Esperar a que terminen los hijos y liberar todo ---- */
    waitpid(p1, NULL, 0);
    waitpid(p2, NULL, 0);
    waitpid(p3, NULL, 0);
    if (usar_renderer) {
        waitpid(p4, NULL, 0);
    }

    printf("\n=== FIN: resultado=%d tick=%d vidas=%d score=%d ===\n",
           sh->game_result, sh->global_tick, sh->bomber_lives, sh->score);

    destruir_sincronizacion(sh);
    munmap(sh, sizeof(shared_t));
    return 0;
}
