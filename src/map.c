/*
 * map.c — lectura del mapa (sección 7). Joel.
 *
 * Lee map.txt, valida dimensiones y símbolos, ubica P y A..D y
 * deja esas celdas como 'O'.
 */
#include <stdio.h>
#include <string.h>
#include "procs.h"

static int is_valid_symbol(char c)
{
    return c == CELL_WALL || c == CELL_BLOCK || c == CELL_FREE ||
           c == 'P' || (c >= 'A' && c <= 'D');
}

int load_map(shared_t *sh, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return -1; }

    char line[MAX_X + 8];
    int y = 0;
    int bombers = 0;
    int enemies[NUM_ENEMIES] = { 0 };

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        int len = (int)strlen(line);
        if (len == 0) continue;

        if (y >= MAX_Y) {
            fprintf(stderr, "mapa: más de %d filas\n", MAX_Y);
            fclose(f);
            return -1;
        }
        if (len > MAX_X) {
            fprintf(stderr, "mapa: fila %d tiene más de %d columnas\n", y, MAX_X);
            fclose(f);
            return -1;
        }
        if (y == 0) sh->cols = len;
        if (len != sh->cols) {
            fprintf(stderr, "mapa: fila %d mide %d, se esperaba %d\n", y, len, sh->cols);
            fclose(f);
            return -1;
        }

        for (int x = 0; x < len; x++) {
            char c = line[x];
            if (!is_valid_symbol(c)) {
                fprintf(stderr, "mapa: símbolo '%c' inválido en (%d, %d)\n", c, x, y);
                fclose(f);
                return -1;
            }
            if (c == 'P') {
                sh->bomber_x = x; sh->bomber_y = y; c = CELL_FREE;
                bombers++;
            } else if (c >= 'A' && c <= 'D') {
                int i = c - 'A';
                sh->enemy_x[i] = x; sh->enemy_y[i] = y; c = CELL_FREE;
                enemies[i]++;
            }
            sh->map_grid[y][x] = c;
        }
        y++;
    }
    fclose(f);
    sh->rows = y;

    if (sh->rows == 0) {
        fprintf(stderr, "mapa: archivo vacío\n");
        return -1;
    }
    if (bombers != 1) {
        fprintf(stderr, "mapa: debe haber exactamente una P (hay %d)\n", bombers);
        return -1;
    }
    for (int i = 0; i < NUM_ENEMIES; i++) {
        if (enemies[i] != 1) {
            fprintf(stderr, "mapa: debe haber exactamente un '%c' (hay %d)\n", 'A' + i, enemies[i]);
            return -1;
        }
    }
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
