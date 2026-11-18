/*
 * foto.h — copia del estado del juego que usa P4 para dibujar.
 *
 * La comparten los dos dibujos de P4:
 *   renderer.c      -> terminal   (opción -r)
 *   renderer_sdl.c  -> ventana 2D (opción -g, compilando con "make sdl")
 */
#ifndef FOTO_H
#define FOTO_H

#include "shared.h"

/* Copia del estado del juego en un tick */
typedef struct {
    char mapa[MAX_Y][MAX_X];
    int  filas, cols;
    int  bomb_x[MAX_BOMBS], bomb_y[MAX_BOMBS], bomb_timer[MAX_BOMBS];
    int  bomber_x, bomber_y;
    int  enemy_x[NUM_ENEMIES], enemy_y[NUM_ENEMIES], enemy_alive[NUM_ENEMIES];
    int  prio_bomber, prio_enemy;
    int  tick, max_ticks, vidas, score, enemigos_vivos;
    int  game_over, resultado;
} foto_t;

/* Copia todo lo que hace falta, tomando cada mutex en el orden de shared.h */
void tomar_foto(shared_t *sh, foto_t *f);

/* Temporizador de la bomba en (x, y), o -1 si ahí no hay bomba */
int timer_de_bomba(foto_t *f, int x, int y);

/* Ventana 2D con SDL. Devuelve 0 si este programa se compiló sin SDL
 * (entonces P4 dibuja en la terminal). */
int run_renderer_sdl(shared_t *sh);

#endif
