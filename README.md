# P2 — Bomberman Concurrente (IS2021)

**Grupo:** Joel · Abigail · Mariel · Danilo

Este es el esqueleto del proyecto. **Ya compila y corre.** La parte difícil de conectar los procesos
(memoria compartida, turnos) ya está hecha. A cada uno le toca llenar **su parte del juego**.

---

## La idea del plan

> **Primero que el juego funcione SIN hilos. Después le agregamos los hilos.**

La concurrencia (hilos, mutex, semáforos) la estamos viendo recién en clase. Pero la mayor parte del trabajo
es **lógica del juego en C normal** (como C++, sin clases), y eso lo podemos hacer desde ya.

| Semana | Meta | Qué hace falta saber |
|---|---|---|
| **1 — primer avance** (aprox. hasta el 30/10) | El juego funciona sin hilos: Bomberman se mueve, los enemigos se mueven y las bombas explotan | C básico: `if`, `for`, leer archivos |
| **2** (aprox. hasta el 06/11) | Cada uno divide su parte en hilos y le pone candados (mutex) | Lo que veamos en clase |
| **3** (aprox. hasta el 13/11) | Juntar todo, probar, bonus y preparar la exposición | — |

---

## Reparto

| Quién | Proceso | Archivos | En una frase |
|---|---|---|---|
| **Joel** | P0 — el "árbitro" | `src/scheduler.c`, `src/map.c` | Decide a quién le toca, cuenta vidas y puntaje, y dice cuándo termina el juego. También coordina la integración |
| **Abigail** | P1 — Bomberman | `src/parser.c`, `src/bomber.c` (+ `src/renderer.c` bonus) | Lee los movimientos de Bomberman y lo mueve |
| **Mariel** | P2 — enemigos | `src/enemy.c` | Mueve a los 4 enemigos y detecta si tocan a Bomberman |
| **Danilo** | P3 — bombas | `src/bomb.c` + `tests/` | Temporizadores, explosiones y daño. También arma los casos de prueba |

Cada uno toca **solo sus archivos**, así no nos pisamos. Para ver lo que te falta:
```bash
grep -n "TODO(Abigail)" src/*.c      # cambia el nombre por el tuyo
```

---

## Antes de empezar (todos)

1. **Instalar Linux dentro de Windows (WSL).** En PowerShell: `wsl --install -d Ubuntu`, reiniciar, abrir "Ubuntu" y correr:
   ```bash
   sudo apt update && sudo apt install build-essential git
   ```
2. **Compilar y correr:**
   ```bash
   make
   ./bomberman tests/caso_ejemplo 5
   ```
   Tiene que mostrar `[P0] tick 1 -> P1`, `[P1] turno...` y el mapa varias veces.
3. **Ejemplos para entender la idea** (5 minutos, en `ejemplos/`):
   ```bash
   cd ejemplos
   gcc 1_fork_memoria.c -o ej1 && ./ej1                # por qué hace falta memoria compartida
   gcc 2_turnos_semaforo.c -o ej2 -pthread && ./ej2    # cómo se dan los turnos
   ```
4. **Leer solo tu parte del enunciado** (`26-2_S10 Proyecto P2.pdf`):

   | Quién | Secciones |
   |---|---|
   | Joel | 4, 5, 9, 10, 11, 12 |
   | Abigail | 2.2, 6, 8.1, 10 |
   | Mariel | 2.3, 6 |
   | Danilo | 2.4, 7, 8 |

---

## Cómo funciona (versión corta)

- El programa crea **4 procesos** (P0 a P3). Son como 4 programas separados que **no ven las variables de los otros**.
- Para comunicarse usan una **"pizarra compartida"**: el `struct shared_t` de `include/shared.h`. Lo que uno escribe ahí, los demás lo ven.
- Para leer o escribir la pizarra se usa `sh->`. Por ejemplo `sh->bomber_x = 3;` o `sh->map_grid[y][x]`.
- **Cuidado con las coordenadas:** `map_grid[y][x]`, primero la fila y después la columna.
- **Regla:** cada dato de la pizarra tiene **un solo dueño que lo escribe**. Si necesitas que otro cambie algo, le dejas un "papelito" (por ejemplo, P1 no pone la bomba: escribe `bomb_request_active = 1` y P3 la pone).

**Un tick (una "ronda" del juego):**
```
1. P0: tick++
2. P0 le da el turno a P1 (Bomberman) o a P2 (enemigos), según la prioridad → ese proceso juega
3. P0 le da el turno a P3 (bombas)                                         → P3 juega
4. P0 revisa los avisos (colisiones, explosiones) → resta vidas, suma puntos, ¿terminó el juego?
```

**Dónde va tu código:** dentro del `for` de tu archivo, donde dice `>>> SEMANA 1 <<<`.
Las líneas marcadas `NO TOCAR` son las que esperan y ceden el turno, y ya funcionan.

```c
for (;;) {
    sem_wait(&sh->sem_bomber_turn);    // NO TOCAR: espera su turno
    if (is_game_over(sh)) break;       // NO TOCAR
    // >>> SEMANA 1: TU LÓGICA AQUÍ <<<
    sem_post(&sh->sem_turn_done);      // NO TOCAR: avisa "terminé"
}
```

### C que necesitas esta semana

| C++ / Python | C |
|---|---|
| `ifstream f("a.txt")` | `FILE *f = fopen("a.txt", "r");` |
| `getline(f, linea)` | `char linea[64]; fgets(linea, sizeof(linea), f)` (devuelve `NULL` al final del archivo) |
| `linea == "LEFT"` | `strcmp(linea, "LEFT") == 0` (incluye `<string.h>`) |
| `cout << x` | `printf("x=%d\n", x);` |
| armar la ruta del archivo | `char ruta[300]; snprintf(ruta, sizeof(ruta), "%s/bomber_moves.txt", sh->data_dir);` |
| `stoi` | `sscanf(linea, "SET_PRIORITY %d", &valor);` |

---

## Semana 1: tarea de cada uno (primer avance)

### Joel — P0 árbitro (`scheduler.c`, `map.c`)
- [ ] `map.c`: validar el mapa (todas las filas del mismo largo, solo símbolos válidos, una P y una de cada A–D).
- [ ] `choose_turn`: le toca al de **mayor** prioridad; si empatan, se alternan.
- [ ] `process_priority_requests`: aplicar `SET_PRIORITY` si el valor está entre 1 y 49.
- [ ] `process_events`: colisión o explosión → `bomber_lives--` (**máximo 1 vida por tick**); enemigo alcanzado → `enemy_alive[i] = 0`; limpiar los avisos; `score = 10*bloques + 100*enemigos_muertos`.
- [ ] `check_end`: victoria, derrota y "se acabaron los movimientos y no quedan bombas".
- [ ] Coordinar: juntar las ramas de todos en `main` y revisar que compile.

### Abigail — P1 Bomberman (`parser.c`, `bomber.c`)
- [ ] **Primero (día 1–2) `parser.c`**, porque Mariel también lo usa: `"LEFT"` → `INS_LEFT`, `"SET_PRIORITY 25"` → `INS_SET_PRIORITY` con valor 25, cualquier otra cosa → `INS_INVALID`. Subirlo apenas funcione.
- [ ] `bomber.c`: abrir `bomber_moves.txt` y en cada turno leer **una** línea.
- [ ] Mover: calcular la nueva celda y moverse solo si `is_walkable(sh, nx, ny)`. Si no se puede, se pierde el turno igual.
- [ ] `BOMB`: escribir `bomb_request_x/y` y `bomb_request_active = 1` (**no** tocar el mapa).
- [ ] `SET_PRIORITY`: escribir `pending_priority_bomber` y `priority_request_active = 1`.
- [ ] Línea inválida: imprimir aviso y leer la siguiente (no gasta el turno). Fin del archivo → `bomber_moves_done = 1`.

### Mariel — P2 enemigos (`enemy.c`)
- [ ] Abrir los 4 archivos `enemy_1_moves.txt` … `enemy_4_moves.txt` (un `FILE*` por enemigo, en un arreglo).
- [ ] En cada turno: para cada enemigo **vivo** (`sh->enemy_alive[i]`), leer una línea y moverlo si `is_walkable`.
- [ ] Después de moverlos: si algún enemigo vivo está en la celda de Bomberman → `collision_detected = 1`, `collision_tick`, `collision_enemy_id`.
- [ ] `SET_PRIORITY` → `pending_priority_enemy`; si lo piden dos en el mismo turno, gana el de número mayor.
- [ ] Cuando todos los vivos terminan su archivo → `enemy_moves_done = 1`.
- [ ] Mientras Abigail termina el parser, puedes usar `strcmp` directo para avanzar.

### Danilo — P3 bombas (`bomb.c`, `tests/`)
- [ ] En cada turno, **en este orden**: (1) borrar el fuego `*` → `O`; (2) bajar el temporizador de cada bomba; (3) explotar las que llegan a 0; (4) atender la solicitud de bomba.
- [ ] Explosión en cruz hasta 2 celdas por lado: `X` corta el fuego, `#` se rompe (`blocks_destroyed++`) y corta, otra `Q` explota también (cadena).
- [ ] Daño: si Bomberman o un enemigo vivo están en una celda con fuego → `blast_hit_bomber = 1` / `blast_hit_enemy[i] = 1`.
- [ ] Solicitud: si `bomb_request_active` y hay menos de 2 bombas → crear la bomba con temporizador 3 y poner `Q`. **Siempre** limpiar `bomb_request_active`.
- [ ] Revisar que `tests/caso_ejemplo` reproduzca la tabla 8.5 del enunciado.

### Primer avance = esto funcionando
```bash
./bomberman tests/caso_ejemplo 10
```
Bomberman camina, pone la bomba en (3,1), la bomba explota en el tick 6, se rompen (4,1) y (3,2), y los enemigos se mueven.

---

## Semana 2: agregar hilos (adelanto)

Cuando lo veamos en clase, cada uno divide su código en los hilos que pide el enunciado. Los nombres ya están en cada archivo:

| Quién | Hilos |
|---|---|
| Joel | `tick_thread`, `scheduler_thread`, `signal_thread`, `event_thread` |
| Abigail | `movement_reader_thread`, `movement_executor_thread`, `bomber_publisher_thread` |
| Mariel | `enemy_controller_thread`, 4 × `enemy_thread`, `bomber_tracker_thread`, `collision_thread`, `enemy_publisher_thread` |
| Danilo | `bomb_controller_thread`, `fuse_thread`, `explosion_thread` |

Temas para ir investigando: `pthread_create`, `pthread_join`, `pthread_mutex_lock`, `sem_wait` / `sem_post`, y el problema "productor-consumidor".

---

## Reglas del grupo

1. Cada uno trabaja en **su rama**, nunca directo en `main`.
2. **Commits chicos y seguidos**: la nota individual revisa quién hizo qué en el repo.
3. Antes de pedir el merge a `main`: `make` sin errores y `./bomberman tests/caso_ejemplo 10` sin colgarse.
4. `include/shared.h` (la pizarra) **no se cambia sin avisar al grupo**.

---

## Guía de Git paso a paso

Git guarda el historial del código y nos deja trabajar los 4 a la vez sin pisarnos.
**Todos los comandos van en la terminal de Ubuntu (WSL).**

### Paso 0 — Configurar git (una sola vez)

Usa **el mismo correo de tu cuenta de GitHub**. Si no, tus commits no aparecen con tu nombre, y eso cuenta para la nota individual.
```bash
git config --global user.name "Tu Nombre"
git config --global user.email "tu_correo_de_github@ejemplo.com"
```

### Paso 1 — Descargar el proyecto (una sola vez)
```bash
cd ~
git clone https://github.com/JoelxD12O/bomberman-concurrente.git
cd bomberman-concurrente
make && ./bomberman tests/caso_ejemplo 5     # comprobar que funciona
```
Para que puedas subir cambios, Joel te tiene que agregar como colaborador. Te llega una invitación al correo o a GitHub y tienes que **aceptarla**.

### Paso 2 — Crear tu rama (una sola vez)

Una rama es tu propia copia para trabajar tranquilo.
```bash
git checkout -b p0-joel        # Joel
git checkout -b p1-abigail     # Abigail
git checkout -b p2-mariel      # Mariel
git checkout -b p3-danilo      # Danilo
```
Para ver en qué rama estás: `git branch` (la que tiene `*`).

### Paso 3 — Trabajar y guardar (todos los días)
```bash
# ... editas tu archivo ...
make                                  # que compile
./bomberman tests/caso_ejemplo 10     # que corra
git status                            # ver qué archivos cambiaste
git add src/bomber.c                  # elegir qué guardar (tus archivos)
git commit -m "P1: Bomberman se mueve con UP/DOWN/LEFT/RIGHT"
```
Un **commit** es una "foto" del código. Haz uno cada vez que algo funcione, aunque sea chiquito.
El mensaje tiene que decir **qué hiciste**: "P3: explosión en cruz", no "cambios" ni "avance".

### Paso 4 — Subir tu rama a GitHub
```bash
git push -u origin p1-abigail     # la primera vez (con el nombre de tu rama)
git push                          # las siguientes veces
```
La primera vez te pide usuario y contraseña. **La contraseña no es la de GitHub**: es un *token*.
Para crearlo: GitHub → Settings → Developer settings → Personal access tokens → Tokens (classic) → Generate, con el permiso `repo`. Guárdalo bien.
Para no tener que escribirlo cada vez: `git config --global credential.helper store`.

### Paso 5 — Juntar tu trabajo con el de los demás (Pull Request)

Cuando tu parte funcione:
1. Entra al repo en GitHub. Va a aparecer el botón **"Compare & pull request"**.
2. Elige: base `main` ← compare `tu-rama`.
3. Escribe qué hiciste y dale **Create pull request**.
4. Joel lo revisa y lo junta (*merge*) con `main`.

### Paso 6 — Traer lo que subieron los demás

Hazlo cada vez que alguien junte algo a `main` (por ejemplo, cuando Abigail suba el parser):
```bash
git checkout main
git pull                      # traer lo nuevo de main
git checkout p2-mariel        # volver a tu rama
git merge main                # mezclar lo nuevo con lo tuyo
```

### Si algo sale mal

| Problema | Qué hacer |
|---|---|
| `git push` dice *rejected* | `git pull` primero y después `git push` |
| Aparece un *CONFLICT* al hacer merge | Abre el archivo y busca `<<<<<<<`. Deja la versión correcta, borra las marcas `<<<<<<<`, `=======` y `>>>>>>>`, y después `git add archivo` y `git commit` |
| Cambié un archivo que no era mío | `git checkout -- src/archivo.c` (lo deja como estaba en el último commit) |
| No sé qué pasa | `git status` y le mandas captura al grupo |

### Resumen diario
```bash
git checkout tu-rama
# ...programar...
make && ./bomberman tests/caso_ejemplo 10
git add tus_archivos
git commit -m "Px: qué hiciste"
git push
```

## Mapa del proyecto

```
include/shared.h    la pizarra compartida (todos la usan)
include/procs.h     funciones comunes
src/main.c          arranque: crea la pizarra y los procesos (ya funciona, no tocar)
src/scheduler.c     P0  Joel
src/map.c           mapa  Joel
src/parser.c        lector de instrucciones  Abigail
src/bomber.c        P1  Abigail
src/enemy.c         P2  Mariel
src/bomb.c          P3  Danilo
src/renderer.c      P4 bonus  Abigail (semana 3)
tests/              casos de prueba  Danilo
ejemplos/           ejemplos chicos para aprender
```
