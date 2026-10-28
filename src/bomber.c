/*
 * bomber.c — P1: Bomberman (secciones 2.2, 6, 8.1 y 10). Abigail.
 *
 * Lee bomber_moves.txt y, cada vez que P0 le da el turno, ejecuta UNA instrucción.
 *
 * Reglas:
 *   - P1 NO escribe en el mapa. Para poner una bomba deja un pedido
 *     (bomb_request_...) y P3 es quien la pone.
 *   - SET_PRIORITY no cambia la prioridad: deja un pedido para P0
 *     (pending_priority_bomber).
 *   - Si la celda de destino está ocupada, Bomberman no se mueve pero
 *     IGUAL gasta el turno.
 *   - Una línea que no se entiende se salta y se lee la siguiente
 *     (eso NO gasta el turno).
 *
 * SEMANA 1: escribir la lógica dentro del while de abajo (sin hilos, sin mutex).
 * SEMANA 2: dividirla en 3 hilos:
 *   movement_reader_thread    lee el archivo y guarda las instrucciones en una cola
 *   movement_executor_thread  en cada turno saca UNA instrucción y la ejecuta
 *   bomber_publisher_thread   copia la posición de Bomberman a la memoria compartida
 */
#include <stdio.h>
#include "procs.h"

void run_bomber(shared_t *sh)
{
    /* SEMANA 1 — TODO(Abigail): abrir el archivo aquí, antes del while:
     *
     *   char ruta[300];
     *   snprintf(ruta, sizeof(ruta), "%s/bomber_moves.txt", sh->data_dir);
     *   FILE *archivo = fopen(ruta, "r");
     */

    while (1) {
        sem_wait(&sh->sem_bomber_turn);   // NO TOCAR: dormir hasta que P0 dé el turno
        if (is_game_over(sh)) {           // NO TOCAR: si el juego terminó, salir
            break;
        }

        printf("[P1] turno en el tick %d\n", sh->global_tick);

        /* >>> SEMANA 1 — TODO(Abigail): tu código va AQUÍ <<<
         *
         * 1. Leer una línea:   char linea[64];  fgets(linea, sizeof(linea), archivo)
         *    Si fgets devuelve NULL se acabó el archivo:  sh->bomber_moves_done = 1;
         *
         * 2. Entender la línea:   instruction ins = parse_instruction(linea);
         *
         * 3. Hacer lo que dice ins.type:
         *    INS_UP / INS_DOWN / INS_LEFT / INS_RIGHT:
         *        calcular la celda nueva (nx, ny). OJO: y crece hacia ABAJO,
         *        así que UP es y - 1 y DOWN es y + 1.
         *        if (is_walkable(sh, nx, ny))  { sh->bomber_x = nx; sh->bomber_y = ny; }
         *    INS_BOMB:
         *        sh->bomb_request_x = sh->bomber_x;
         *        sh->bomb_request_y = sh->bomber_y;
         *        sh->bomb_request_active = 1;
         *    INS_SET_PRIORITY:
         *        sh->pending_priority_bomber = ins.value;
         *        sh->priority_request_active = 1;
         *    INS_INVALID:
         *        printf un aviso y volver al paso 1 (leer otra línea)
         */

        sem_post(&sh->sem_turn_done);     // NO TOCAR: avisar a P0 "terminé mi turno"
    }
}
