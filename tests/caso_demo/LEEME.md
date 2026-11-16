# Caso demo: una partida que muestra todas las reglas

```bash
make
./bomberman tests/caso_demo 16 40 30
```

Bomberman empieza con prioridad 40 y los enemigos con 30, así que al principio juega solo Bomberman.

## Mapa inicial

```
XXXXXXXXXXX      P = Bomberman (1,1)
XPOO#OOOOOX      A = enemigo 1 (3,2)    B = enemigo 2 (3,3)
XOXAXOXOXOX      C = enemigo 3 (5,3)    D = enemigo 4 (9,5)
XOOBOCOOOOX      # = bloque en (4,1)
XOXOXOXOXOX
XOOOOOOOODX
XXXXXXXXXXX
```

## Qué pasa en cada tick

| Tick | Turno | Qué pasa | Regla que se ve |
|---|---|---|---|
| 1 | P1 | `SALTAR` no es una instrucción: se descarta y se lee la siguiente, `RIGHT` → (2,1) | Línea inválida no gasta el turno |
| 2 | P1 | `BOMB`: P3 registra la bomba 1 en (2,1) | P1 pide, P3 pone la bomba |
| 3 | P1 | `RIGHT` → (3,1) | |
| 4 | P1 | `BOMB`: bomba 2 en (3,1) | Hasta 2 bombas a la vez |
| 5 | P1 | `RIGHT` choca con el bloque (4,1): no se mueve, pero gasta el turno. La bomba 1 explota, su fuego alcanza a la bomba 2 y explota también. Se destruye el bloque (4,1) y mueren los enemigos 1 y 2. Bomberman está en el fuego y pierde **una sola** vida, aunque explotaron dos bombas | Reacción en cadena, bloque destruido, enemigos eliminados, máximo una vida por tick |
| 6 | P1 | `SET_PRIORITY 30`: deja el pedido. P3 limpia el fuego | Solicitud de prioridad |
| 7 | P1 | P0 aplica 30: ahora empatan. `DOWN` → (3,2) | Cambio de prioridad al inicio del tick |
| 8–11 | P2, P1, P2, P1 | Se alternan. Bomberman intenta `LEFT` y `RIGHT` hacia paredes y no se mueve. El enemigo 4 se salta `BOMB` y `HOLA` (inválidas para enemigos) y sube | Round Robin con prioridades iguales, paredes |
| 12 | P2 | El enemigo 3 entra a (3,2), donde está Bomberman: **choque**, le quedan 1 vida. El enemigo 4 pide `SET_PRIORITY 35` | Colisión, solicitud de P2 |
| 13–16 | P2 | P0 aplica 35 a los enemigos y desde ahí P1 no vuelve a jugar. Su `SET_PRIORITY 49` nunca se ejecuta | Inanición: con menor prioridad no puede ni pedir prioridad (sección 9) |
| 16 | — | Se alcanza `max_ticks` | Fin del juego |

**Resultado:** `=== FIN: resultado=1 tick=16 vidas=1 score=210 ===`.
El puntaje es 1 bloque × 10 + 2 enemigos × 100.

## Los otros finales

```bash
./bomberman tests/caso_victoria 20 40 30          # victoria: una bomba elimina a los 4 enemigos
./bomberman tests/caso_derrota 20 40 30           # derrota: pierde las 3 vidas
./bomberman tests/caso_sin_movimientos 30 30 30   # se acaban los movimientos y no hay bombas
./bomberman tests/caso_ejemplo 9 40 30            # ejemplo 8.5 del enunciado
```

Para revisar condiciones de carrera: `make tsan` y correr cualquiera con `./bomberman_tsan`.
