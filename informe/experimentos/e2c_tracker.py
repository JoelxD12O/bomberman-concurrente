"""
Familia 2c - Orden bomber_tracker_thread -> collision_thread en el codigo REAL de P2.

Copia el juego (carpeta con src/ e include/), arma tres variantes de enemy.c,
las compila y corre caso_tracker/ N veces con cada una:

  real      : enemy.c tal cual (tracker y collision se despiertan a la vez)
  real+amp  : igual, con un retardo de 2 ms al inicio del tracker (amplificacion)
  corregido : retardo de 2 ms + el controlador espera sem_tracker_done antes de
              despertar a collision_thread

En caso_tracker el enemigo A entra a la celda de Bomberman en el tick 2 (primer
turno de P2): la partida correcta termina con vidas=2.

Uso (Linux/WSL):  python3 e2c_tracker.py <carpeta_del_juego> [N]
Escribe datos/e2c_tracker.csv
"""
import os
import shutil
import subprocess
import sys
import tempfile

AQUI = os.path.dirname(os.path.abspath(__file__))
juego = sys.argv[1]
N = int(sys.argv[2]) if len(sys.argv) > 2 else 300

ORIG_CTRL = """        sem_post(&sem_start_tracker);
        sem_post(&sem_start_collision);
        sem_wait(&sem_tracker_done);
        sem_wait(&sem_collision_done);"""
FIX_CTRL = """        sem_post(&sem_start_tracker);
        sem_wait(&sem_tracker_done);
        sem_post(&sem_start_collision);
        sem_wait(&sem_collision_done);"""
ORIG_TRK = """        if (!seguir) break;

        pthread_mutex_lock(&local_pos_mtx);
        copia_bx = sh->bomber_x;"""
AMP_TRK = """        if (!seguir) break;
        usleep(2000);   /* amplificacion de la ventana */

        pthread_mutex_lock(&local_pos_mtx);
        copia_bx = sh->bomber_x;"""

base = open(os.path.join(juego, "src", "enemy.c"), encoding="utf-8").read()
assert ORIG_CTRL in base and ORIG_TRK in base, "enemy.c cambio: revisar los parches"
variantes = {
    "real": base,
    "real+amp": "#include <unistd.h>\n" + base.replace(ORIG_TRK, AMP_TRK),
    "corregido": "#include <unistd.h>\n" + base.replace(ORIG_TRK, AMP_TRK).replace(ORIG_CTRL, FIX_CTRL),
}

caso = os.path.join(AQUI, "caso_tracker")
out = open(os.path.join(AQUI, "datos", "e2c_tracker.csv"), "w")
for nombre, codigo in variantes.items():
    tmp = tempfile.mkdtemp()
    shutil.copytree(os.path.join(juego, "src"), os.path.join(tmp, "src"))
    shutil.copytree(os.path.join(juego, "include"), os.path.join(tmp, "include"))
    open(os.path.join(tmp, "src", "enemy.c"), "w", encoding="utf-8").write(codigo)
    srcs = [os.path.join(tmp, "src", f) for f in os.listdir(os.path.join(tmp, "src")) if f.endswith(".c")]
    binario = os.path.join(tmp, "bomberman")
    subprocess.run(["gcc", "-O2", "-pthread", "-I", os.path.join(tmp, "include"), "-o", binario] + srcs,
                   check=True)
    for i in range(1, N + 1):
        r = subprocess.run([binario, caso, "2", "30", "30"], capture_output=True, text=True, timeout=10)
        fin = [l for l in r.stdout.splitlines() if l.startswith("=== FIN")][-1]
        vidas = fin.split("vidas=")[1].split()[0]
        out.write(f"{nombre},{i},{vidas}\n")
    shutil.rmtree(tmp)
    print(nombre, "listo")
out.close()
