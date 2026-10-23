/*
 * map.c — lectura del mapa (sección 7). Joel.
 *
 * Ya hace lo mínimo: lee map.txt, guarda dimensiones, ubica P y A..D y
 * deja esas celdas como 'O'.
 */
#include <stdio.h>
#include <string.h>
#include "procs.h"

int load_map(shared_t *sh, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return -1; }

    char line[MAX_X + 8];
    int y = 0;
    while (fgets(line, sizeof(line), f) && y < MAX_Y) {
        line[strcspn(line, "\r\n")] = '\0';
        int len = (int)strlen(line);
        if (len == 0) continue;
        if (y == 0) sh->cols = len;

        /* TODO(Joel): validar que todas las filas tengan el mismo largo (sh->cols)
         *          y que solo aparezcan los símbolos X # O P A B C D.
         *          Si algo falla: fclose(f) y return -1. */

        for (int x = 0; x < len && x < MAX_X; x++) {
            char c = line[x];
            if (c == 'P') {
                sh->bomber_x = x; sh->bomber_y = y; c = CELL_FREE;
            } else if (c >= 'A' && c <= 'D') {
                int i = c - 'A';
                sh->enemy_x[i] = x; sh->enemy_y[i] = y; c = CELL_FREE;
            }
            sh->map_grid[y][x] = c;
        }
        y++;
    }
    fclose(f);
    sh->rows = y;

    /* TODO(Joel): validar que exista exactamente una P y una de cada A..D. */
    return 0;
}

/* El llamador debe tener map_mtx tomado. */
int is_walkable(shared_t *sh, int x, int y)
{
    if (x < 0 || y < 0 || x >= sh->cols || y >= sh->rows) return 0;
    return sh->map_grid[y][x] == CELL_FREE;
}

/* Impresión simple para depurar. El bonus P4 hace una versión bonita. */
void print_map(shared_t *sh)
{
    pthread_mutex_lock(&sh->map_mtx);
    pthread_mutex_lock(&sh->bomber_mtx);
    pthread_mutex_lock(&sh->enemy_mtx);
    for (int y = 0; y < sh->rows; y++) {
        for (int x = 0; x < sh->cols; x++) {
            char c = sh->map_grid[y][x];
            for (int i = 0; i < NUM_ENEMIES; i++)
                if (sh->enemy_alive[i] && sh->enemy_x[i] == x && sh->enemy_y[i] == y)
                    c = 'A' + i;
            if (sh->bomber_x == x && sh->bomber_y == y) c = 'P';
            putchar(c);
        }
        putchar('\n');
    }
    pthread_mutex_unlock(&sh->enemy_mtx);
    pthread_mutex_unlock(&sh->bomber_mtx);
    pthread_mutex_unlock(&sh->map_mtx);
}
