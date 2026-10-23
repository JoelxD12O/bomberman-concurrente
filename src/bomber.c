/*
 * bomber.c — P1 bomber_process (secciones 2.2, 5, 8.1, 10). Abigail.
 *
 * ESTADO ACTUAL: stub. Espera su turno, no hace nada y avisa a P0.
 *
 * META: 3 hilos.
 *   movement_reader_thread    lee bomber_moves.txt y mete instrucciones en una cola
 *   movement_executor_thread  en cada turno saca UNA instrucción y la ejecuta
 *   bomber_publisher_thread   publica bomber_x/bomber_y en memoria compartida
 *
 * Reglas que hay que cumplir:
 *   - P1 NUNCA escribe map_grid. Para BOMB escribe el buzón bomb_request_* (bomb_req_mtx).
 *   - SET_PRIORITY no cambia la prioridad: escribe el buzón pending_priority_bomber (prio_mtx).
 *   - Un movimiento inválido (pared, bloque, bomba, fuego, fuera del mapa) TAMBIÉN consume turno.
 *   - Línea desconocida: se descarta y se lee la siguiente en el mismo turno.
 *   - Si se acaba el archivo: bomber_moves_done = 1.
 */
#include <stdio.h>
#include "procs.h"

/* ---- Cola interna de P1 (solo la ven los hilos de este proceso) ----
 * TODO(Abigail): buffer circular + pthread_mutex_t + sem_t "items disponibles".
 *          Es el problema productor-consumidor clásico. */

__attribute__((unused))
static void *movement_reader_thread(void *arg)
{
    shared_t *sh = arg;
    /* TODO(Abigail): abrir <data_dir>/bomber_moves.txt, por cada línea:
     *          parse_instruction -> encolar -> sem_post(items). Al final, marcar EOF. */
    (void)sh;
    return NULL;
}

__attribute__((unused))
static void *movement_executor_thread(void *arg)
{
    shared_t *sh = arg;
    /* TODO(Abigail): loop {
     *   sem_wait(&sh->sem_bomber_turn); if (is_game_over(sh)) break;
     *   sacar instrucción; mover (validar con is_walkable bajo map_mtx) / BOMB / SET_PRIORITY
     *   avisar al publisher
     *   sem_post(&sh->sem_turn_done);
     * } */
    (void)sh;
    return NULL;
}

__attribute__((unused))
static void *bomber_publisher_thread(void *arg)
{
    shared_t *sh = arg;
    /* TODO(Abigail): cuando el executor actualice la posición local, copiarla a
     *          sh->bomber_x / sh->bomber_y con bomber_mtx tomado. */
    (void)sh;
    return NULL;
}

void run_bomber(shared_t *sh)
{
    /* SEMANA 1: abrir bomber_moves.txt aquí (antes del for). */

    /* SEMANA 2 — TODO(Abigail): reemplazar este stub por pthread_create de los 3 hilos + pthread_join. */
    for (;;) {
        sem_wait(&sh->sem_bomber_turn);          /* NO TOCAR: espera su turno */
        if (is_game_over(sh)) break;             /* NO TOCAR */
        printf("[P1] turno en tick %d (stub)\n", sh->global_tick);

        /* >>> SEMANA 1 — TODO(Abigail): tu lógica va AQUÍ, sin hilos <<<
         *   leer UNA línea -> parse_instruction
         *   UP/DOWN/LEFT/RIGHT: si is_walkable(nueva celda) -> sh->bomber_x / sh->bomber_y
         *   BOMB: sh->bomb_request_x/y = posición actual; sh->bomb_request_active = 1
         *   SET_PRIORITY: sh->pending_priority_bomber = valor; sh->priority_request_active = 1
         *   línea inválida: imprimir aviso y leer la siguiente (no gasta el turno)
         *   fin de archivo: sh->bomber_moves_done = 1 */

        sem_post(&sh->sem_turn_done);            /* NO TOCAR: avisa "terminé" */
    }
}
