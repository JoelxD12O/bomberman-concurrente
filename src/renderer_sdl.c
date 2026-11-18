/*
 * renderer_sdl.c — P4 en una ventana 2D con SDL2 (opción -g, BONUS).
 *
 *   make sdl
 *   ./bomberman tests/caso_demo 16 40 30 -g
 *
 * La sincronización es IGUAL a la de la terminal (renderer.c):
 *   P0 hace sem_post(sem_render) al terminar el tick y espera sem_turn_done.
 *   P4 copia el estado con tomar_foto (mutex en el orden de shared.h),
 *   lo dibuja durante PAUSA_MS y responde con sem_post(sem_turn_done).
 * Solo cambia CÓMO se dibuja: rectángulos y círculos en vez de texto.
 *
 * Durante la pausa, Bomberman y los enemigos se deslizan de la celda anterior
 * a la nueva (animación). Es solo dibujo: el juego ya avanzó de golpe.
 *
 * Variables de entorno opcionales (para grabar los videos de la exposición):
 *   BOMBER_PAUSA_MS=1000      cuánto dura cada tick en pantalla
 *   BOMBER_GRABAR=carpeta     guarda cada cuadro como carpeta/cuadro_00001.bmp
 *   BOMBER_TITULO="texto"     título del caso, arriba a la derecha
 *
 * Si se compila con "make" (sin SDL), este archivo solo trae una función
 * vacía que devuelve 0, y P4 sigue dibujando en la terminal.
 */
#include <stdio.h>
#include "procs.h"
#include "foto.h"

#ifndef USAR_SDL

int run_renderer_sdl(shared_t *sh)
{
    if (sh != NULL) {
        printf("[P4] -g necesita compilar con: make sdl. Se usa la terminal.\n");
    }
    return 0;
}

#else

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#define CELDA     64      /* lado de una celda en píxeles */
#define MARGEN    28
#define ARRIBA    112     /* alto de la barra con vidas, tick y puntaje */
#define ABAJO     64      /* alto de la barra de abajo (bombas) */
#define CUADRO_MS 33      /* ~30 cuadros por segundo */
#define PAUSA_MS_SDL 600  /* cuánto se ve cada tick (igual que en la terminal) */
#define FUENTE    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"

typedef struct { Uint8 r, g, b; } color_t;

/* Paleta */
static const color_t FONDO     = { 18, 22, 42 };
static const color_t PISO_A    = { 38, 45, 74 };
static const color_t PISO_B    = { 42, 50, 81 };
static const color_t PARED     = { 118, 128, 156 };
static const color_t PARED_LUZ = { 168, 178, 204 };
static const color_t PARED_OSC = { 78, 86, 110 };
static const color_t LADRILLO  = { 205, 125, 62 };
static const color_t JUNTA     = { 150, 84, 40 };
static const color_t FUEGO_1   = { 255, 112, 40 };
static const color_t FUEGO_2   = { 255, 214, 74 };
static const color_t BLANCO    = { 245, 245, 250 };
static const color_t GRIS      = { 150, 156, 180 };
static const color_t NEGRO     = { 20, 20, 26 };
static const color_t AZUL      = { 52, 132, 235 };
static const color_t ROJO      = { 235, 72, 86 };
static const color_t AMARILLO  = { 255, 197, 61 };
/* un color por enemigo: A, B, C, D */
static const color_t ENEMIGO[NUM_ENEMIES] = {
    { 231, 76, 60 }, { 162, 92, 214 }, { 26, 188, 156 }, { 236, 112, 160 }
};

static SDL_Renderer *pincel;
static TTF_Font *letra_grande, *letra_media, *letra_chica;
static int izquierda = MARGEN;   /* x donde empieza el tablero (centrado en la ventana) */

/* ---------------------------------------------------------------- figuras */

static void usar_color(color_t c, Uint8 alfa)
{
    SDL_SetRenderDrawColor(pincel, c.r, c.g, c.b, alfa);
}

static void rect(int x, int y, int w, int h, color_t c)
{
    SDL_Rect r = { x, y, w, h };
    usar_color(c, 255);
    SDL_RenderFillRect(pincel, &r);
}

/* Círculo relleno: una línea horizontal por cada fila */
static void circulo(int cx, int cy, int radio, color_t c)
{
    usar_color(c, 255);
    for (int dy = -radio; dy <= radio; dy++) {
        int dx = (int) sqrt((double) (radio * radio - dy * dy));
        SDL_RenderDrawLine(pincel, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

/* Rectángulo con esquinas redondeadas */
static void rect_redondo(int x, int y, int w, int h, int radio, color_t c)
{
    rect(x + radio, y, w - 2 * radio, h, c);
    rect(x, y + radio, w, h - 2 * radio, c);
    circulo(x + radio, y + radio, radio, c);
    circulo(x + w - radio - 1, y + radio, radio, c);
    circulo(x + radio, y + h - radio - 1, radio, c);
    circulo(x + w - radio - 1, y + h - radio - 1, radio, c);
}

/* Escribe texto; si centrado = 1, x es el centro */
static void texto(TTF_Font *letra, const char *t, int x, int y, color_t c, int centrado)
{
    if (letra == NULL || t[0] == '\0') {
        return;
    }
    SDL_Color col = { c.r, c.g, c.b, 255 };
    SDL_Surface *sup = TTF_RenderUTF8_Blended(letra, t, col);
    if (sup == NULL) {
        return;
    }
    SDL_Texture *tex = SDL_CreateTextureFromSurface(pincel, sup);
    SDL_Rect r = { centrado ? x - sup->w / 2 : x, y, sup->w, sup->h };
    SDL_RenderCopy(pincel, tex, NULL, &r);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(sup);
}

/* ---------------------------------------------------------------- celdas */

static void dibujar_piso(int px, int py, int x, int y)
{
    rect(px, py, CELDA, CELDA, (x + y) % 2 ? PISO_A : PISO_B);
}

static void dibujar_pared(int px, int py)
{
    rect(px, py, CELDA, CELDA, PARED_OSC);
    rect(px, py, CELDA - 6, CELDA - 6, PARED_LUZ);
    rect(px + 6, py + 6, CELDA - 12, CELDA - 12, PARED);
}

static void dibujar_bloque(int px, int py)
{
    rect(px, py, CELDA, CELDA, JUNTA);
    int alto = (CELDA - 6) / 3;
    for (int fila = 0; fila < 3; fila++) {
        int y = py + 2 + fila * (alto + 2);
        int corrido = (fila % 2) ? CELDA / 2 : 0;   /* ladrillos intercalados */
        for (int i = -1; i < 2; i++) {
            int x0 = px + corrido + i * (CELDA / 2) + 2;
            int x1 = x0 + CELDA / 2 - 4;
            if (x0 < px + 2) x0 = px + 2;
            if (x1 > px + CELDA - 2) x1 = px + CELDA - 2;
            if (x1 > x0) {
                rect(x0, y, x1 - x0, alto, LADRILLO);
            }
        }
    }
}

static void dibujar_fuego(int px, int py)
{
    rect_redondo(px + 3, py + 3, CELDA - 6, CELDA - 6, 14, FUEGO_1);
    rect_redondo(px + 14, py + 14, CELDA - 28, CELDA - 28, 9, FUEGO_2);
}

static void dibujar_bomba(int px, int py, int timer, int cuadro)
{
    int cx = px + CELDA / 2, cy = py + CELDA / 2 + 4;
    int radio = CELDA * 30 / 100;
    /* mecha */
    usar_color(GRIS, 255);
    SDL_RenderDrawLine(pincel, cx + 8, cy - radio + 4, cx + 16, cy - radio - 8);
    SDL_RenderDrawLine(pincel, cx + 9, cy - radio + 4, cx + 17, cy - radio - 8);
    /* chispa que titila */
    circulo(cx + 17, cy - radio - 9, (cuadro / 4) % 2 ? 6 : 4, AMARILLO);
    circulo(cx, cy, radio, NEGRO);
    circulo(cx - radio / 3, cy - radio / 3, radio / 4, (color_t) { 70, 70, 84 });
    char t[12];
    snprintf(t, sizeof(t), "%d", timer);
    texto(letra_media, t, cx, cy - 13, BLANCO, 1);
}

/* Bomberman: cuerpo azul, cara blanca y dos ojos */
static void dibujar_bomberman(int cx, int cy)
{
    circulo(cx, cy + 4, CELDA * 34 / 100, AZUL);
    rect_redondo(cx - 15, cy - 6, 30, 18, 7, BLANCO);
    rect(cx - 8, cy - 2, 5, 10, NEGRO);
    rect(cx + 3, cy - 2, 5, 10, NEGRO);
}

/* Enemigo: cuerpo redondo de su color con "patitas" y su letra */
static void dibujar_enemigo(int cx, int cy, int i)
{
    color_t c = ENEMIGO[i];
    int r = CELDA * 30 / 100;
    circulo(cx, cy - 2, r, c);
    rect(cx - r, cy - 2, 2 * r + 1, r, c);
    for (int k = 0; k < 3; k++) {
        circulo(cx - r + 7 + k * (r - 6), cy + r - 3, 7, c);
    }
    circulo(cx - 8, cy - 6, 6, BLANCO);
    circulo(cx + 8, cy - 6, 6, BLANCO);
    circulo(cx - 7, cy - 5, 3, NEGRO);
    circulo(cx + 9, cy - 5, 3, NEGRO);
    char t[2] = { (char) ('A' + i), '\0' };
    texto(letra_chica, t, cx, cy + 3, BLANCO, 1);
}

/* ---------------------------------------------------------------- pantalla */

static const char *titulo_final(int resultado)
{
    if (resultado == RESULT_VICTORY)  return "¡VICTORIA!";
    if (resultado == RESULT_DEFEAT)   return "DERROTA";
    if (resultado == RESULT_NO_MOVES) return "SIN MOVIMIENTOS";
    return "FIN: MÁXIMO DE TICKS";
}

/* Posición en pantalla de una entidad que se mueve de (x0,y0) a (x1,y1).
 * avance va de 0 a 1. Si saltó más de una celda, no se anima. */
static void posicion(int x0, int y0, int x1, int y1, double avance, int *px, int *py)
{
    double x = x1, y = y1;
    if (abs(x1 - x0) + abs(y1 - y0) == 1) {
        x = x0 + (x1 - x0) * avance;
        y = y0 + (y1 - y0) * avance;
    }
    *px = izquierda + (int) (x * CELDA) + CELDA / 2;
    *py = ARRIBA + (int) (y * CELDA) + CELDA / 2;
}

static void dibujar(foto_t *antes, foto_t *f, double avance, int cuadro, int ancho, int alto)
{
    usar_color(FONDO, 255);
    SDL_RenderClear(pincel);

    /* --- barra superior --- */
    char t[96];
    texto(letra_grande, "BOMBERMAN CONCURRENTE", MARGEN, 16, BLANCO, 0);
    snprintf(t, sizeof(t), "Tick %d / %d     Prioridad  P1: %d   P2: %d",
             f->tick, f->max_ticks, f->prio_bomber, f->prio_enemy);
    texto(letra_chica, t, MARGEN, 58, GRIS, 0);
    snprintf(t, sizeof(t), "Puntaje %d    Enemigos %d", f->score, f->enemigos_vivos);
    texto(letra_chica, t, MARGEN, 82, GRIS, 0);
    for (int i = 0; i < INITIAL_LIVES; i++) {   /* vidas: corazones */
        int cx = ancho - MARGEN - 22 - (INITIAL_LIVES - 1 - i) * 44;
        color_t c = i < f->vidas ? ROJO : (color_t) { 70, 76, 100 };
        circulo(cx - 8, 34, 10, c);
        circulo(cx + 8, 34, 10, c);
        for (int k = 0; k < 18; k++) {          /* punta del corazón */
            usar_color(c, 255);
            SDL_RenderDrawLine(pincel, cx - 18 + k, 38 + k, cx + 18 - k, 38 + k);
        }
    }
    /* Título opcional del caso (BOMBER_TITULO), alineado a la derecha */
    const char *titulo = getenv("BOMBER_TITULO");
    if (titulo != NULL && letra_chica != NULL) {
        int w = 0, h = 0;
        TTF_SizeUTF8(letra_chica, titulo, &w, &h);
        texto(letra_chica, titulo, ancho - MARGEN - w, 82, AMARILLO, 0);
    }

    /* --- tablero --- */
    for (int y = 0; y < f->filas; y++) {
        for (int x = 0; x < f->cols; x++) {
            int px = izquierda + x * CELDA, py = ARRIBA + y * CELDA;
            char c = f->mapa[y][x];
            if (c == CELL_WALL) {
                dibujar_pared(px, py);
                continue;
            }
            dibujar_piso(px, py, x, y);
            if (c == CELL_BLOCK) {
                dibujar_bloque(px, py);
            } else if (c == CELL_FIRE) {
                dibujar_fuego(px, py);
            } else if (c == CELL_BOMB) {
                int tb = timer_de_bomba(f, x, y);
                dibujar_bomba(px, py, tb > 0 ? tb : 0, cuadro);
            }
        }
    }

    /* --- enemigos y Bomberman (con animación de movimiento) --- */
    /* ¿Algún enemigo está en la misma celda que Bomberman? */
    int choque = 0;
    for (int i = 0; i < NUM_ENEMIES; i++) {
        if (f->enemy_alive[i] && f->enemy_x[i] == f->bomber_x && f->enemy_y[i] == f->bomber_y) {
            choque = 1;
        }
    }
    if (choque && f->vidas > 0 && avance >= 1.0) {   /* fondo rojo en la celda del choque */
        rect_redondo(izquierda + f->bomber_x * CELDA + 2, ARRIBA + f->bomber_y * CELDA + 2,
                     CELDA - 4, CELDA - 4, 12, ROJO);
    }
    /* En un choque se dibujan separados (enemigo a la izquierda, Bomberman
     * a la derecha) para que se vean los dos */
    int corrido = (choque && avance >= 1.0) ? CELDA / 5 : 0;

    for (int i = 0; i < NUM_ENEMIES; i++) {
        if (!f->enemy_alive[i]) {
            continue;
        }
        int cx, cy;
        posicion(antes->enemy_x[i], antes->enemy_y[i], f->enemy_x[i], f->enemy_y[i], avance, &cx, &cy);
        int encima = (f->enemy_x[i] == f->bomber_x && f->enemy_y[i] == f->bomber_y);
        dibujar_enemigo(encima ? cx - corrido : cx, cy, i);
    }
    if (f->vidas > 0) {
        int cx, cy;
        posicion(antes->bomber_x, antes->bomber_y, f->bomber_x, f->bomber_y, avance, &cx, &cy);
        dibujar_bomberman(cx + corrido, cy);
        if (choque && avance >= 1.0) {          /* aviso de choque */
            texto(letra_media, "¡choque!", cx, cy - CELDA + 4, ROJO, 1);
        }
    }

    /* Si alguien está parado sobre una bomba, el número se muestra en la esquina */
    for (int i = 0; i < MAX_BOMBS; i++) {
        int tb = timer_de_bomba(f, f->bomb_x[i], f->bomb_y[i]);
        if (tb <= 0) {
            continue;
        }
        int ocupada = (f->bomber_x == f->bomb_x[i] && f->bomber_y == f->bomb_y[i]);
        for (int k = 0; k < NUM_ENEMIES; k++) {
            if (f->enemy_alive[k] && f->enemy_x[k] == f->bomb_x[i] && f->enemy_y[k] == f->bomb_y[i]) {
                ocupada = 1;
            }
        }
        if (ocupada) {
            int px = izquierda + f->bomb_x[i] * CELDA + CELDA - 14;
            int py = ARRIBA + f->bomb_y[i] * CELDA + 14;
            circulo(px, py, 13, AMARILLO);
            circulo(px, py, 11, NEGRO);
            char nb[12];
            snprintf(nb, sizeof(nb), "%d", tb);
            texto(letra_chica, nb, px, py - 10, BLANCO, 1);
        }
    }

    /* --- barra inferior: bombas activas --- */
    char linea[160] = "Bombas: ninguna";
    int n = 0;
    for (int i = 0; i < MAX_BOMBS; i++) {
        int tb = timer_de_bomba(f, f->bomb_x[i], f->bomb_y[i]);
        if (tb > 0) {
            if (n == 0) {
                snprintf(linea, sizeof(linea), "Bombas:");
            }
            char una[48];
            snprintf(una, sizeof(una), "  (%d,%d) explota en %d", f->bomb_x[i], f->bomb_y[i], tb);
            strncat(linea, una, sizeof(linea) - strlen(linea) - 1);
            n++;
        }
    }
    texto(letra_chica, linea, MARGEN, alto - ABAJO + 20, GRIS, 0);

    /* --- fin del juego: franja oscura con el resultado --- */
    if (f->game_over && avance >= 1.0) {
        SDL_SetRenderDrawBlendMode(pincel, SDL_BLENDMODE_BLEND);
        SDL_Rect banda = { 0, ARRIBA + f->filas * CELDA / 2 - 60, ancho, 120 };
        SDL_SetRenderDrawColor(pincel, 10, 12, 24, 215);
        SDL_RenderFillRect(pincel, &banda);
        color_t c = f->resultado == RESULT_VICTORY ? AMARILLO : (f->resultado == RESULT_DEFEAT ? ROJO : BLANCO);
        texto(letra_grande, titulo_final(f->resultado), ancho / 2, banda.y + 22, c, 1);
        snprintf(t, sizeof(t), "Tick %d  ·  %d vida(s)  ·  %d puntos", f->tick, f->vidas, f->score);
        texto(letra_chica, t, ancho / 2, banda.y + 74, BLANCO, 1);
    }
}

/* Guarda el cuadro actual como BMP (solo si se pidió BOMBER_GRABAR) */
static void grabar_cuadro(const char *carpeta, int numero, int ancho, int alto)
{
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, ancho, alto, 32, SDL_PIXELFORMAT_ARGB8888);
    if (s == NULL) {
        return;
    }
    SDL_RenderReadPixels(pincel, NULL, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
    char ruta[300];
    snprintf(ruta, sizeof(ruta), "%s/cuadro_%05d.bmp", carpeta, numero);
    SDL_SaveBMP(s, ruta);
    SDL_FreeSurface(s);
}

/* Muestra un tick durante "ms" milisegundos (con la animación) */
static void mostrar(foto_t *antes, foto_t *f, int ms, int *cuadro, const char *grabar,
                    int ancho, int alto, int *cerrar)
{
    for (int t = 0; t <= ms; t += CUADRO_MS) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {            /* la ventana debe atender eventos */
            if (e.type == SDL_QUIT) {
                *cerrar = 1;                   /* se cerró la ventana: se deja de animar */
            }
        }
        double avance = t / 250.0;             /* el movimiento dura 250 ms */
        if (avance > 1.0) {
            avance = 1.0;
        }
        dibujar(antes, f, *cerrar ? 1.0 : avance, *cuadro, ancho, alto);
        SDL_RenderPresent(pincel);
        if (grabar != NULL) {
            grabar_cuadro(grabar, *cuadro, ancho, alto);
        }
        (*cuadro)++;
        if (!*cerrar) {
            SDL_Delay(CUADRO_MS);
        }
    }
}

int run_renderer_sdl(shared_t *sh)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0 || TTF_Init() != 0) {
        printf("[P4] no se pudo abrir la ventana (%s). Se usa la terminal.\n", SDL_GetError());
        return 0;
    }
    letra_grande = TTF_OpenFont(FUENTE, 30);
    letra_media  = TTF_OpenFont(FUENTE, 22);
    letra_chica  = TTF_OpenFont(FUENTE, 17);

    int pausa = PAUSA_MS_SDL;
    if (getenv("BOMBER_PAUSA_MS") != NULL) {
        pausa = atoi(getenv("BOMBER_PAUSA_MS"));
    }
    const char *grabar = getenv("BOMBER_GRABAR");

    /* El tamaño de la ventana depende del mapa */
    foto_t antes, foto;
    tomar_foto(sh, &antes);
    int ancho = 2 * MARGEN + antes.cols * CELDA;
    if (ancho < 760) {
        ancho = 760;                 /* mínimo para que el título y las vidas no se encimen */
    }
    izquierda = (ancho - antes.cols * CELDA) / 2;
    int alto = ARRIBA + antes.filas * CELDA + ABAJO;

    SDL_Window *ventana = SDL_CreateWindow("Bomberman concurrente", SDL_WINDOWPOS_CENTERED,
                                           SDL_WINDOWPOS_CENTERED, ancho, alto, 0);
    if (ventana == NULL) {
        printf("[P4] no se pudo abrir la ventana (%s). Se usa la terminal.\n", SDL_GetError());
        SDL_Quit();
        return 0;
    }
    pincel = SDL_CreateRenderer(ventana, -1, SDL_RENDERER_SOFTWARE);

    int cuadro = 0, cerrar = 0;
    while (1) {
        sem_wait(&sh->sem_render);           /* dormir hasta que P0 termine el tick */

        tomar_foto(sh, &foto);
        mostrar(&antes, &foto, pausa, &cuadro, grabar, ancho, alto, &cerrar);
        antes = foto;

        sem_post(&sh->sem_turn_done);        /* "terminé de dibujar": P0 sigue */

        if (foto.game_over) {
            break;
        }
    }

    /* El resultado final queda en pantalla un momento antes de cerrar */
    mostrar(&antes, &foto, 2500, &cuadro, grabar, ancho, alto, &cerrar);

    TTF_Quit();
    SDL_DestroyRenderer(pincel);
    SDL_DestroyWindow(ventana);
    SDL_Quit();
    return 1;
}

#endif
