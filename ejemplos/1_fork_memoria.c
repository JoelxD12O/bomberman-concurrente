/*
 * Ejemplo 1: fork copia la memoria; mmap la comparte.
 * Compilar: gcc 1_fork_memoria.c -o ej1 && ./ej1
 */
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>

int main(void)
{
    int normal = 0;   /* variable común: cada proceso tendrá SU copia */

    /* "pizarra" compartida: un int que ven padre e hijo */
    int *compartido = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE,
                           MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *compartido = 0;

    pid_t pid = fork();          /* desde aquí hay DOS procesos corriendo */

    if (pid == 0) {              /* esto lo ejecuta solo el HIJO */
        normal = 5;
        *compartido = 5;
        printf("hijo:  normal=%d compartido=%d\n", normal, *compartido);
        return 0;
    }

    wait(NULL);                  /* el PADRE espera a que el hijo termine */
    printf("padre: normal=%d compartido=%d\n", normal, *compartido);
    /* normal sigue en 0 (el hijo cambió su copia); compartido es 5 */
    return 0;
}
