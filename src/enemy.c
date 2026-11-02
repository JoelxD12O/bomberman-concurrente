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
#include <string.h>
void run_enemies(shared_t *sh)
{
    /* SEMANA 1 — TODO(Mariel): abrir los 4 archivos aquí, antes del while:*/
     FILE *archivos[NUM_ENEMIES];
     for (int i = 0; i < NUM_ENEMIES; i++) {
          char ruta[300];
          snprintf(ruta, sizeof(ruta), "%s/enemy_%d_moves.txt", sh->data_dir, i + 1);
          archivos[i] = fopen(ruta, "r");
      }

    while (1) {
        sem_wait(&sh->sem_enemy_turn);    // NO TOCAR: dormir hasta que P0 dé el turno
        if (is_game_over(sh)) {           // NO TOCAR: si el juego terminó, salir
            break;
        }

        printf("[P2] turno en el tick %d\n", sh->global_tick);

        /* >>> SEMANA 1 — TODO(Mariel): tu código va AQUÍ <<<*/
        int todos_terminaron = 1;
	
	for (int i=0;i<NUM_ENEMIES; i++) {
	//Solo procesamos enemigos vivos
		if (sh->enemy_alive[i] == 0) {
		continue;
		}
		char linea[64];
		if (archivos[i] != NULL && fgets(linea, sizeof(linea), archivos[i]) != NULL) {
		todos_terminaron=0;
		linea[strcspn(linea, "\r\n")] = 0;
		int  dx=0, dy=0;
		if (strcmp(linea, "UP")==0) {dy=-1;}
		else if (strcmp(linea, "DOWN") ==0) {dy=1;}
		else if (strcmp(linea, "LEFT") ==0) {dx=-1;}
		else if (strcmp(linea, "RIGHT") ==0) {dx=1;}
		else if (strcmp(linea, "SET_PRIORITY") == 0) {
			if (i >= sh->pending_priority_enemy) {
				sh->pending_priority_enemy = i; }
		}
		//Verificar casilla caminable si hay intento de movimiento
		if (dx !=0 || dy!=0){
			int nx = sh->enemy_x[i] + dx;
			int ny = sh->enemy_y[i] + dy;
			if (is_walkable(sh, nx, ny)) { sh->enemy_x[i] = nx; sh->enemy_y[i] = ny; }
		}
	}
		if (sh->enemy_x[i] == sh->bomber_x && sh->enemy_y[i]==sh->bomber_y) {
                sh->collision_detected = 1;
                sh->collision_tick = sh->global_tick;
                sh->collision_enemy_id = i;
         	}
	}
        if (todos_terminaron) {
		 sh->enemy_moves_done = 1;
        }

        sem_post(&sh->sem_turn_done);     // NO TOCAR: avisar a P0 "terminé mi turno"
    }
}
