/*
 * map.c — lectura del mapa (sección 7). Joel.
 *
 * Lee map.txt, revisa que esté bien hecho, guarda dónde empiezan Bomberman (P)
 * y los enemigos (A, B, C, D), y deja esas celdas como camino libre 'O'.
 */
#include <stdio.h>
#include <string.h>
#include "procs.h"

/* Devuelve 1 si c es un símbolo permitido en map.txt */
static int simbolo_valido(char c)
{
    if (c == 'X' || c == '#' || c == 'O' || c == 'P') return 1;
    if (c == 'A' || c == 'B' || c == 'C' || c == 'D') return 1;
    return 0;
}

/* Devuelve 0 si el mapa está bien, -1 si hay un error. */
int load_map(shared_t *sh, const char *path)
{
    FILE *archivo = fopen(path, "r");
    if (archivo == NULL) {
        printf("error: no se pudo abrir %s\n", path);
        return -1;
    }

    char linea[100];
    int y = 0;                                 /* fila actual */
    int cantidad_p = 0;                        /* cuántas P hay */
    int cantidad_enemigo[NUM_ENEMIES] = {0};   /* cuántas A, B, C, D hay */
    int error = 0;

    while (fgets(linea, sizeof(linea), archivo) != NULL) {
        limpiar_linea(linea);
        int largo = strlen(linea);
        if (largo == 0) {
            continue;   /* saltar líneas vacías */
        }

        /* --- revisar el tamaño --- */
        if (y >= MAX_Y) {
            printf("error mapa: tiene más de %d filas\n", MAX_Y);
            error = 1;
            break;
        }
        if (largo > MAX_X) {
            printf("error mapa: la fila %d tiene más de %d columnas\n", y, MAX_X);
            error = 1;
            break;
        }
        if (y == 0) {
            sh->cols = largo;   /* la primera fila define el ancho */
        }
        if (largo != sh->cols) {
            printf("error mapa: la fila %d mide %d, debería medir %d\n", y, largo, sh->cols);
            error = 1;
            break;
        }

        /* --- revisar y guardar cada celda --- */
        for (int x = 0; x < largo; x++) {
            char c = linea[x];

            if (!simbolo_valido(c)) {
                printf("error mapa: símbolo '%c' no permitido en (%d, %d)\n", c, x, y);
                error = 1;
                break;
            }

            if (c == 'P') {
                sh->bomber_x = x;
                sh->bomber_y = y;
                cantidad_p++;
                c = 'O';   /* donde empieza Bomberman es camino libre */
            }
            if (c == 'A' || c == 'B' || c == 'C' || c == 'D') {
                int i = c - 'A';   /* 'A' -> 0, 'B' -> 1, 'C' -> 2, 'D' -> 3 */
                sh->enemy_x[i] = x;
                sh->enemy_y[i] = y;
                cantidad_enemigo[i]++;
                c = 'O';
            }

            sh->map_grid[y][x] = c;   /* ojo: primero la fila (y), después la columna (x) */
        }
        if (error) {
            break;
        }
        y++;
    }
    fclose(archivo);
    if (error) {
        return -1;
    }
    sh->rows = y;

    /* --- revisar que estén todos los personajes --- */
    if (sh->rows == 0) {
        printf("error mapa: el archivo está vacío\n");
        return -1;
    }
    if (cantidad_p != 1) {
        printf("error mapa: tiene que haber exactamente una P (hay %d)\n", cantidad_p);
        return -1;
    }
    for (int i = 0; i < NUM_ENEMIES; i++) {
        if (cantidad_enemigo[i] != 1) {
            printf("error mapa: tiene que haber exactamente una '%c' (hay %d)\n",
                   'A' + i, cantidad_enemigo[i]);
            return -1;
        }
    }
    return 0;
}

/* Devuelve 1 si se puede caminar a (x, y): dentro del mapa y celda 'O'. */
int is_walkable(shared_t *sh, int x, int y)
{
    if (x < 0 || y < 0 || x >= sh->cols || y >= sh->rows) {
        return 0;   /* fuera del mapa */
    }
    if (sh->map_grid[y][x] == 'O') {
        return 1;
    }
    return 0;       /* pared X, bloque #, bomba Q o fuego * */
}

/* Dibuja el mapa en la consola con los personajes encima. */
void print_map(shared_t *sh)
{
    /* se toman los mutex en el orden de shared.h: map -> bomber -> enemy */
    pthread_mutex_lock(&sh->map_mtx);
    pthread_mutex_lock(&sh->bomber_mtx);
    pthread_mutex_lock(&sh->enemy_mtx);

    for (int y = 0; y < sh->rows; y++) {
        for (int x = 0; x < sh->cols; x++) {
            char c = sh->map_grid[y][x];

            for (int i = 0; i < NUM_ENEMIES; i++) {
                if (sh->enemy_alive[i] && sh->enemy_x[i] == x && sh->enemy_y[i] == y) {
                    c = 'A' + i;
                }
            }
            if (sh->bomber_x == x && sh->bomber_y == y) {
                c = 'P';
            }
            printf("%c", c);
        }
        printf("\n");
    }

    pthread_mutex_unlock(&sh->enemy_mtx);
    pthread_mutex_unlock(&sh->bomber_mtx);
    pthread_mutex_unlock(&sh->map_mtx);
}
