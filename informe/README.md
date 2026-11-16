# Informe: Bomberman Concurrente en C con POSIX

- `informe.tex` / `informe.pdf`: el informe.
- `figuras/`: figuras (PDF), `arquitectura.tex` (TikZ) y los números generados (`numeros.tex`, `tabla_juego.tex`).
- `experimentos/`: un programa C por familia de carreras (`-DBUGGY` = versión sin la primitiva), los datos (`datos/*.csv`) y los scripts.

## Cómo regenerar todo

En Linux/WSL, con el juego ya compilado (`make` en la raíz):

```bash
cd informe/experimentos
./run.sh ../..                      # micro-experimentos + validación del juego (~25 min)
python3 e2c_tracker.py ../.. 300    # familia 2c sobre el código real de P2
```

Variantes: `SOLO=micro ./run.sh ../..`, `SOLO=juego ./run.sh ../..` o `SOLO=e6a_cierre ITER=50 ./run.sh ../..`.

Después, con Python + matplotlib:

```bash
python graficos.py                  # figuras/*.pdf y figuras/numeros.tex
cd .. && pdflatex informe.tex && pdflatex informe.tex
```

Nota: la versión de P1 con tres hilos está en la rama `p1-abigail`. El experimento 2c necesita el `src/enemy.c` de `main` (aplica sus parches sobre una copia).
