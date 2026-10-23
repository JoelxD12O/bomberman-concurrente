/*
 * Ejemplo 2: el padre (como P0) le da turnos a un hijo (como P1) con semáforos.
 * Es exactamente el patrón del proyecto, en chiquito.
 * Compilar: gcc 2_turnos_semaforo.c -o ej2 -pthread && ./ej2
 */
#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/wait.h>

typedef struct {          /* nuestra mini "shared_t" */
    int tick;
    int game_over;
    sem_t turno;          /* padre -> hijo: "te toca"  */
    sem_t listo;          /* hijo -> padre: "terminé"  */
} pizarra_t;

int main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    pizarra_t *p = mmap(NULL, sizeof(pizarra_t), PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    p->tick = 0;
    p->game_over = 0;
    sem_init(&p->turno, 1, 0);   /* 1 = compartido entre procesos, 0 = valor inicial */
    sem_init(&p->listo, 1, 0);

    if (fork() == 0) {           /* HIJO = "P1" */
        for (;;) {
            sem_wait(&p->turno);             /* duerme hasta que el padre diga "te toca" */
            if (p->game_over) break;
            printf("  [hijo] jugando en tick %d\n", p->tick);
            sem_post(&p->listo);             /* "terminé" */
        }
        return 0;
    }

    for (int i = 1; i <= 3; i++) {   /* PADRE = "P0" */
        p->tick = i;
        printf("[padre] tick %d: le doy el turno\n", i);
        sem_post(&p->turno);
        sem_wait(&p->listo);         /* duerme hasta que el hijo termine */
    }
    p->game_over = 1;
    sem_post(&p->turno);             /* despertarlo para que vea game_over y salga */
    wait(NULL);
    printf("[padre] fin\n");
    return 0;
}
