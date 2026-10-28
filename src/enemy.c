/*
 * enemy.c — P2: los 4 enemigos (secciones 2.3 y 6). Mariel.
 *
 * Cada enemigo tiene su propio archivo (enemy_1_moves.txt ... enemy_4_moves.txt).
 * Cada vez que P0 le da el turno a P2, cada enemigo VIVO ejecuta UNA instrucción.
 *
 * Reglas:
 *   - Un enemigo muerto (sh->enemy_alive[i] == 0) ya no se mueve.
 *   - Dos enemigos SÍ pueden estar en la misma celda.
 *   - BOMB no vale para enemigos (es una instrucción inválida).
 *   - P2 no quita vidas: solo avisa que hubo choque (collision_...) y P0 resta la vida.
 *   - SET_PRIORITY deja un pedido para P0 (pending_priority_enemy). Si dos
 *     enemigos lo piden en el mismo turno, gana el de número mayor.
 *
 * SEMANA 1: escribir la lógica dentro del while de abajo (sin hilos, sin mutex).
 * SEMANA 2: dividirla en 8 hilos:
 *   enemy_controller_thread   recibe el turno y despierta a los enemigos vivos
 *   enemy_thread x 4          uno por enemigo, cada uno lee su archivo
 *   bomber_tracker_thread     guarda una copia de la posición de Bomberman
 *   collision_thread          revisa si algún enemigo tocó a Bomberman
 *   enemy_publisher_thread    copia las posiciones a la memoria compartida
 */
#include <stdio.h>
#include "procs.h"

void run_enemies(shared_t *sh)
{
    /* SEMANA 1 — TODO(Mariel): abrir los 4 archivos aquí, antes del while:
     *
     *   FILE *archivos[NUM_ENEMIES];
     *   for (int i = 0; i < NUM_ENEMIES; i++) {
     *       char ruta[300];
     *       snprintf(ruta, sizeof(ruta), "%s/enemy_%d_moves.txt", sh->data_dir, i + 1);
     *       archivos[i] = fopen(ruta, "r");
     *   }
     */

    while (1) {
        sem_wait(&sh->sem_enemy_turn);    // NO TOCAR: dormir hasta que P0 dé el turno
        if (is_game_over(sh)) {           // NO TOCAR: si el juego terminó, salir
            break;
        }

        printf("[P2] turno en el tick %d\n", sh->global_tick);

        /* >>> SEMANA 1 — TODO(Mariel): tu código va AQUÍ <<<
         *
         * 1. Para cada enemigo i (de 0 a 3) con sh->enemy_alive[i] == 1:
         *      - leer una línea de archivos[i] con fgets
         *      - instruction ins = parse_instruction(linea);
         *      - si es UP/DOWN/LEFT/RIGHT: calcular (nx, ny) y
         *        if (is_walkable(sh, nx, ny)) { sh->enemy_x[i] = nx; sh->enemy_y[i] = ny; }
         *        OJO: y crece hacia ABAJO (UP es y - 1).
         *
         * 2. Después de mover a todos: si algún enemigo vivo está en la misma
         *    celda que Bomberman (sh->bomber_x, sh->bomber_y):
         *        sh->collision_detected = 1;
         *        sh->collision_tick = sh->global_tick;
         *        sh->collision_enemy_id = i;
         *
         * 3. Cuando TODOS los enemigos vivos terminaron su archivo:
         *        sh->enemy_moves_done = 1;
         */

        sem_post(&sh->sem_turn_done);     // NO TOCAR: avisar a P0 "terminé mi turno"
    }
}
