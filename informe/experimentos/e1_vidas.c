/*
 * Familia 1 - Vidas de Bomberman: un solo escritor (P0).
 *
 * P2 (collision_thread) y P3 (explosion_thread) detectan golpes a Bomberman.
 *   BUGGY : cada uno hace bomber_lives-- directamente en la memoria compartida.
 *   normal: cada uno solo publica su evento en SU campo (collision_count /
 *           blast_count) y P0 es el unico que modifica bomber_lives.
 * Metrica: descuentos perdidos = vidas_finales - vidas_esperadas.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define HITS   1000000
#define LIVES0 10000000

typedef struct {
    volatile int go;
    volatile int listo_p2, listo_p3; /* cada hijo avisa que ya arranco */
    volatile int bomber_lives;      /* escribe P0 (en BUGGY: P2 y P3) */
    volatile int collision_count;   /* escribe solo P2 */
    volatile int blast_count;       /* escribe solo P3 */
} shared_t;

static void hijo(shared_t *sh, volatile int *mi_contador, volatile int *listo)
{
    *listo = 1;
    while (!sh->go) { }
    for (int k = 0; k < HITS; k++) {
#ifdef BUGGY
        sh->bomber_lives--;          /* leer-modificar-escribir sin exclusion */
        (void)mi_contador;
#else
        (*mi_contador)++;            /* solo publica el evento */
#endif
    }
    _exit(0);
}

int main(void)
{
    shared_t *sh = mmap(NULL, sizeof *sh, PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    sh->bomber_lives = LIVES0;

    pid_t p2 = fork();
    if (p2 == 0) hijo(sh, &sh->collision_count, &sh->listo_p2);
    pid_t p3 = fork();
    if (p3 == 0) hijo(sh, &sh->blast_count, &sh->listo_p3);

    while (!sh->listo_p2 || !sh->listo_p3) { }
    sh->go = 1;

#ifndef BUGGY
    /* P0: unico escritor. Aplica los eventos a medida que llegan. */
    int vistos_col = 0, vistos_blast = 0, terminados = 0;
    while (terminados < 2) {
        int c = sh->collision_count, b = sh->blast_count;
        sh->bomber_lives -= (c - vistos_col) + (b - vistos_blast);
        vistos_col = c;
        vistos_blast = b;
        if (waitpid(-1, NULL, WNOHANG) > 0) terminados++;
    }
    int c = sh->collision_count, b = sh->blast_count;
    sh->bomber_lives -= (c - vistos_col) + (b - vistos_blast);
#else
    waitpid(p2, NULL, 0);
    waitpid(p3, NULL, 0);
#endif
    int esperado = LIVES0 - 2 * HITS;
    printf("%d\n", sh->bomber_lives - esperado);
    return 0;
}
