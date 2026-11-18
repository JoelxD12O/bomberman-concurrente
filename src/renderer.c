/*
 * renderer.c — P4: dibuja el juego en la terminal (BONUS, sección 3).
 *
 * Se activa con -r:   ./bomberman tests/caso_demo 16 40 30 -r
 * Con -g dibuja en una ventana 2D (ver renderer_sdl.c); la sincronización es la misma.
 *
 * 1) Sincronizado con el tick
 *    Al terminar cada tick, P0 hace sem_post(sem_render) y se queda esperando
 *    en sem_turn_done. P4 copia el estado, lo dibuja, hace una pausa corta y
 *    responde con sem_post(sem_turn_done). Recién ahí P0 empieza el tick
 *    siguiente. Así P4 nunca dibuja un estado a medio cambiar.
 *
 * 2) Sin parpadeo
 *    No se borra la pantalla en cada tick: se vuelve el cursor arriba a la
 *    izquierda (\033[H) y se dibuja encima. \033[K borra lo que sobra de cada línea.
 *
 * 3) Sin mezclar texto
 *    Con -r, main.c manda los mensajes [P0], [P1], ... al archivo partida.log.
 *    Solo P4 escribe en la terminal (fd_terminal).
 *
 * Lee la memoria compartida con los mutex, en el orden de shared.h:
 *   map_mtx -> bomber_mtx -> enemy_mtx -> prio_mtx -> state_mtx
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "procs.h"
#include "foto.h"

#define PAUSA_MS 600   /* cuánto se ve cada tick en pantalla */

/* Colores ANSI: \033[...m cambia el color, \033[0m vuelve al normal */
#define RESET     "\033[0m"
#define PARED     "\033[47m  " RESET              /* fondo gris claro */
#define BLOQUE    "\033[43;30m##" RESET           /* fondo amarillo   */
#define LIBRE     "  "
#define FUEGO     "\033[101;93m**" RESET          /* fondo rojo       */
#define BOMBERMAN "\033[44;97;1m P" RESET         /* fondo azul       */
#define CHOQUE    "\033[41;97;1m!!" RESET         /* enemigo sobre Bomberman */

/* Copia todo lo que hace falta, tomando cada mutex en el orden de shared.h.
 * foto_t está en foto.h porque también la usa renderer_sdl.c */
void tomar_foto(shared_t *sh, foto_t *f)
{
    pthread_mutex_lock(&sh->map_mtx);
    f->filas = sh->rows;
    f->cols  = sh->cols;
    memcpy(f->mapa, sh->map_grid, sizeof(f->mapa));
    for (int i = 0; i < MAX_BOMBS; i++) {
        f->bomb_x[i]     = sh->bomb_x[i];
        f->bomb_y[i]     = sh->bomb_y[i];
        f->bomb_timer[i] = sh->bomb_timer[i];
    }
    pthread_mutex_unlock(&sh->map_mtx);

    pthread_mutex_lock(&sh->bomber_mtx);
    f->bomber_x = sh->bomber_x;
    f->bomber_y = sh->bomber_y;
    pthread_mutex_unlock(&sh->bomber_mtx);

    pthread_mutex_lock(&sh->enemy_mtx);
    for (int i = 0; i < NUM_ENEMIES; i++) {
        f->enemy_x[i] = sh->enemy_x[i];
        f->enemy_y[i] = sh->enemy_y[i];
    }
    pthread_mutex_unlock(&sh->enemy_mtx);

    pthread_mutex_lock(&sh->prio_mtx);
    f->prio_bomber = sh->prioridad_bomber;
    f->prio_enemy  = sh->prioridad_enemy;
    pthread_mutex_unlock(&sh->prio_mtx);

    pthread_mutex_lock(&sh->state_mtx);
    f->tick           = sh->global_tick;
    f->max_ticks      = sh->max_ticks;
    f->vidas          = sh->bomber_lives;
    f->score          = sh->score;
    f->enemigos_vivos = sh->enemies_alive;
    f->game_over      = sh->game_over;
    f->resultado      = sh->game_result;
    for (int i = 0; i < NUM_ENEMIES; i++) {
        f->enemy_alive[i] = sh->enemy_alive[i];
    }
    pthread_mutex_unlock(&sh->state_mtx);
}

/* ¿Hay una bomba activa en (x, y)? Devuelve su temporizador, o -1 si no hay */
int timer_de_bomba(foto_t *f, int x, int y)
{
    if (f->mapa[y][x] != CELL_BOMB) {
        return -1;
    }
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (f->bomb_x[i] == x && f->bomb_y[i] == y && f->bomb_timer[i] > 0) {
            return f->bomb_timer[i];
        }
    }
    return -1;
}

/* Dibuja una celda del mapa (2 caracteres de ancho para que se vea cuadrada) */
static void dibujar_celda(FILE *pantalla, foto_t *f, int x, int y)
{
    int hay_bomber = (f->bomber_x == x && f->bomber_y == y);

    int enemigo = -1;
    for (int i = 0; i < NUM_ENEMIES; i++) {
        if (f->enemy_alive[i] && f->enemy_x[i] == x && f->enemy_y[i] == y) {
            enemigo = i;
        }
    }

    if (hay_bomber && enemigo >= 0) {
        fprintf(pantalla, CHOQUE);
    } else if (hay_bomber) {
        fprintf(pantalla, BOMBERMAN);
    } else if (enemigo >= 0) {
        fprintf(pantalla, "\033[45;97;1m %c" RESET, 'A' + enemigo);   /* fondo morado */
    } else if (f->mapa[y][x] == CELL_WALL) {
        fprintf(pantalla, PARED);
    } else if (f->mapa[y][x] == CELL_BLOCK) {
        fprintf(pantalla, BLOQUE);
    } else if (f->mapa[y][x] == CELL_FIRE) {
        fprintf(pantalla, FUEGO);
    } else if (f->mapa[y][x] == CELL_BOMB) {
        int t = timer_de_bomba(f, x, y);
        fprintf(pantalla, "\033[40;91;1mQ%d" RESET, t > 0 ? t : 0);    /* Q + cuenta regresiva */
    } else {
        fprintf(pantalla, LIBRE);
    }
}

static const char *texto_resultado(int resultado)
{
    if (resultado == RESULT_VICTORY)  return "VICTORIA: todos los enemigos eliminados";
    if (resultado == RESULT_DEFEAT)   return "DERROTA: Bomberman se quedo sin vidas";
    if (resultado == RESULT_NO_MOVES) return "No quedan movimientos ni bombas";
    return "Se alcanzo el maximo de ticks";
}

/* Dibuja un tick completo. \033[K al final de cada línea borra restos del tick anterior */
static void dibujar(FILE *pantalla, foto_t *f)
{
    fprintf(pantalla, "\033[H");   /* cursor arriba a la izquierda: se dibuja encima */

    fprintf(pantalla, "\033[1m BOMBERMAN CONCURRENTE\033[0m      tick %d / %d\033[K\n",
            f->tick, f->max_ticks);
    fprintf(pantalla, " Vidas: ");
    for (int i = 0; i < INITIAL_LIVES; i++) {
        fprintf(pantalla, i < f->vidas ? "\033[91m<3\033[0m " : "\033[90m--\033[0m ");
    }
    fprintf(pantalla, "  Puntaje: %d   Enemigos vivos: %d\033[K\n", f->score, f->enemigos_vivos);
    fprintf(pantalla, " Prioridad  P1 (Bomberman): %2d   P2 (enemigos): %2d\033[K\n",
            f->prio_bomber, f->prio_enemy);
    fprintf(pantalla, "\033[K\n");

    for (int y = 0; y < f->filas; y++) {
        fprintf(pantalla, " ");
        for (int x = 0; x < f->cols; x++) {
            dibujar_celda(pantalla, f, x, y);
        }
        fprintf(pantalla, "\033[K\n");
    }

    fprintf(pantalla, "\033[K\n Bombas: ");
    int hay_bombas = 0;
    for (int i = 0; i < MAX_BOMBS; i++) {
        int t = timer_de_bomba(f, f->bomb_x[i], f->bomb_y[i]);
        if (t > 0) {
            fprintf(pantalla, "(%d,%d) explota en %d tick(s)   ", f->bomb_x[i], f->bomb_y[i], t);
            hay_bombas = 1;
        }
    }
    if (!hay_bombas) {
        fprintf(pantalla, "ninguna");
    }
    fprintf(pantalla, "\033[K\n");

    fprintf(pantalla, " " BOMBERMAN " Bomberman  \033[45;97;1m A" RESET " enemigo  "
            "\033[40;91;1mQ3" RESET " bomba  " FUEGO " fuego  " BLOQUE " bloque  "
            PARED " pared\033[K\n");

    if (f->game_over) {
        fprintf(pantalla, "\033[K\n \033[1m=== FIN: %s ===\033[0m\033[K\n", texto_resultado(f->resultado));
        fprintf(pantalla, " Mensajes de cada proceso: partida.log\033[K\n");
    }
    fprintf(pantalla, "\033[J");   /* borra lo que quede debajo */
    fflush(pantalla);
}

void run_renderer(shared_t *sh)
{
    /* Con -g se dibuja en una ventana 2D (renderer_sdl.c). Si el programa se
     * compiló sin SDL, run_renderer_sdl devuelve 0 y se sigue con la terminal. */
    if (modo_grafico && run_renderer_sdl(sh)) {
        return;
    }

    /* La terminal original (main.c la guardó antes de mandar stdout a partida.log) */
    FILE *pantalla = fdopen(fd_terminal, "w");
    if (pantalla == NULL) {
        return;
    }
    fprintf(pantalla, "\033[2J\033[?25l");   /* limpiar UNA vez y ocultar el cursor */

    while (1) {
        sem_wait(&sh->sem_render);           /* dormir hasta que P0 termine el tick */

        foto_t foto;
        tomar_foto(sh, &foto);
        dibujar(pantalla, &foto);
        usleep(PAUSA_MS * 1000);

        sem_post(&sh->sem_turn_done);        /* "terminé de dibujar": P0 sigue */

        if (foto.game_over) {
            break;                           /* ya dibujé el resultado final */
        }
    }

    fprintf(pantalla, "\033[?25h\n");        /* volver a mostrar el cursor */
    fflush(pantalla);
}
