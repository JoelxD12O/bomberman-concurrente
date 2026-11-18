"""
Genera las figuras del informe (figuras/*.pdf) y los numeros de las tablas
(figuras/numeros.tex) a partir de los CSV de datos/.

Uso:  python graficos.py
"""
import csv
import re
import statistics as st
from collections import defaultdict
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker
from matplotlib.patches import FancyBboxPatch, Rectangle

AQUI = Path(__file__).resolve().parent
DATOS = AQUI / "datos"
FIG = AQUI.parent / "figuras"
FIG.mkdir(exist_ok=True)

# Paleta (slots 1-3 de la paleta categorica de referencia, modo claro)
OK = "#2a78d6"      # version corregida
BUG = "#eb6834"     # version con falla
AQUA = "#1baf7a"
INK = "#0b0b0b"
INK2 = "#52514e"
MUTED = "#8a8985"
GRID = "#e4e3df"
SURF = "#fcfcfb"
P1C, P2C, P3C, P0C = "#2a78d6", "#eb6834", "#eda100", "#52514e"

plt.rcParams.update({
    "font.family": "DejaVu Sans",
    "font.size": 9,
    "axes.edgecolor": MUTED,
    "axes.labelcolor": INK2,
    "axes.titlesize": 10,
    "axes.titleweight": "bold",
    "axes.titlecolor": INK,
    "xtick.color": INK2,
    "ytick.color": INK2,
    "axes.grid": True,
    "grid.color": GRID,
    "grid.linewidth": 0.6,
    "axes.spines.top": False,
    "axes.spines.right": False,
    "legend.frameon": False,
    "savefig.bbox": "tight",
    "savefig.pad_inches": 0.05,
})

NUM = {}  # macros para LaTeX


def fmt(x, dec=0):
    if dec == 0:
        return f"{int(round(x)):,}".replace(",", "\\,")
    return f"{x:,.{dec}f}".replace(",", "\\,")


def leer(nombre):
    filas = []
    p = DATOS / f"{nombre}.csv"
    if not p.exists():
        return filas
    with open(p, newline="") as f:
        for r in csv.reader(f):
            if r:
                filas.append(r)
    return filas


def recursos(filas, var):
    """tiempo, cpu, rss: son las 3 ultimas columnas"""
    sel = [r for r in filas if r[1] == var and r[-3] not in ("", "colgado")]
    t = [float(r[-3]) for r in sel]
    c = [float(r[-2]) for r in sel if r[-2] not in ("?", "")]
    m = [float(r[-1]) for r in sel]
    return (st.mean(t) if t else 0, st.mean(c) if c else 0, max(m) if m else 0)


def guardar_recursos(clave, filas):
    for var, pref in (("ok", "Ok"), ("buggy", "Bug")):
        t, c, m = recursos(filas, var)
        NUM[f"{clave}{pref}Tiempo"] = fmt(t, 4)
        NUM[f"{clave}{pref}Cpu"] = fmt(c)
        NUM[f"{clave}{pref}Mem"] = fmt(m)


def dispersion(nombre, titulo, ylabel, valores_ok, valores_bug, leyenda_ok,
               leyenda_bug, pie, archivo, logy=False):
    fig, ax = plt.subplots(figsize=(6.6, 3.0))
    x_ok = range(1, len(valores_ok) + 1)
    x_bug = range(1, len(valores_bug) + 1)
    ax.scatter(x_bug, valores_bug, s=9, color=BUG, alpha=0.55, linewidths=0,
               label=leyenda_bug, zorder=3)
    ax.plot(list(x_ok), valores_ok, color=OK, lw=2, label=leyenda_ok, zorder=4)
    if logy:
        ax.set_yscale("symlog", linthresh=1)
    ax.set_xlabel("Número de corrida (un punto = una corrida)")
    ax.set_ylabel(ylabel)
    ax.set_title(titulo, loc="left")
    ax.set_ylim(bottom=-0.04 * max(max(valores_bug), 1))
    ax.yaxis.set_major_formatter(matplotlib.ticker.FuncFormatter(
        lambda v, _: f"{int(v):,}".replace(",", " ")))
    ax.legend(loc="upper center", bbox_to_anchor=(0.5, -0.19), ncol=2)
    ax.text(0.5, -0.36, pie, ha="center", va="top", color=INK2, fontsize=8,
            transform=ax.transAxes)
    fig.savefig(FIG / archivo)
    plt.close(fig)


def pct(cond, lista):
    return 100.0 * sum(1 for v in lista if cond(v)) / len(lista) if lista else 0


# ---------------------------------------------------------------------------
# Diagramas temporales (esquematicos)
# ---------------------------------------------------------------------------
def diagrama_temporal(archivo, titulo, carriles, pulsos, t_evento, etiqueta_evento,
                      caja_ok, caja_bug, n_ticks=8, alto=3.6, marcas=None):
    """carriles: lista de nombres (arriba -> abajo).
    pulsos: (carril, t0, t1, texto, color)"""
    n = len(carriles)
    fig = plt.figure(figsize=(7.2, alto))
    ax = fig.add_axes([0.17, 0.36, 0.80, 0.56])
    ax.grid(False)
    for s in ("left", "top", "right"):
        ax.spines[s].set_visible(False)
    ax.set_xlim(0.4, n_ticks + 0.8)
    ax.set_ylim(-0.6, n - 0.2)
    ax.set_yticks([n - 1 - i for i in range(n)])
    ax.set_yticklabels(carriles, fontsize=8.5, color=INK)
    ax.tick_params(axis="y", length=0)
    ax.set_xticks(range(1, n_ticks + 1))
    ax.set_xticklabels([f"t{i}" for i in range(1, n_ticks + 1)])
    ax.set_xlabel("Tiempo lógico del scheduler")
    for i in range(n):
        y = n - 1 - i
        ax.plot([0.4, n_ticks + 0.8], [y, y], color=GRID, lw=1, zorder=1)
    for (carril, t0, t1, texto, color) in pulsos:
        y = n - 1 - carriles.index(carril)
        h = 0.45
        ax.plot([0.4, t0, t0, t1, t1, n_ticks + 0.8], [y, y, y + h, y + h, y, y],
                color=color, lw=1.8, zorder=3)
        if texto:
            ax.text((t0 + t1) / 2, y + h + 0.08, texto, ha="center", va="bottom",
                    fontsize=6.8, color=INK2, linespacing=0.95)
    if t_evento is not None:
        ax.axvline(t_evento, color=MUTED, ls=(0, (3, 3)), lw=0.9, zorder=2)
    if marcas:
        for (t, txt) in marcas:
            ax.text(t, n - 0.35, txt, ha="center", va="bottom", fontsize=8,
                    color=INK, fontweight="bold")
    ax.set_title(titulo, loc="left", pad=14)
    # cajas inferiores
    for k, (cab, cuerpo, col) in enumerate(((caja_ok[0], caja_ok[1], OK),
                                            (caja_bug[0], caja_bug[1], BUG))):
        x0 = 0.06 + k * 0.48
        bx = fig.add_axes([x0, 0.02, 0.44, 0.17])
        bx.axis("off")
        bx.add_patch(FancyBboxPatch((0.01, 0.03), 0.98, 0.94,
                                    boxstyle="round,pad=0.0,rounding_size=0.06",
                                    fc=SURF, ec=col, lw=1.4,
                                    transform=bx.transAxes))
        bx.text(0.5, 0.80, cab, ha="center", va="center", color=col,
                fontweight="bold", fontsize=8.5, transform=bx.transAxes)
        bx.text(0.5, 0.38, cuerpo, ha="center", va="center", color=INK,
                fontsize=7.5, family="DejaVu Sans Mono", transform=bx.transAxes,
                linespacing=1.15)
    if t_evento is not None and etiqueta_evento:
        fig.text(0.17 + 0.80 * (t_evento - 0.4) / (n_ticks + 0.4), 0.215,
                 etiqueta_evento, ha="center", color=INK2, fontsize=7.5)
    fig.savefig(FIG / archivo)
    plt.close(fig)


def figuras_esquematicas():
    # --- Familia 1: vidas
    diagrama_temporal(
        "f1_temporal.pdf",
        "Familia 1 — Diagrama temporal: colisión y explosión en el mismo tick",
        ["P2 · collision_thread", "P3 · explosion_thread", "P0 · event_thread",
         "bomber_lives"],
        [("P2 · collision_thread", 2, 2.5, "", P2C),
         ("P2 · collision_thread", 5, 5.4, "collision_detected=1", P2C),
         ("P3 · explosion_thread", 5.05, 5.45, "blast_hit_bomber=1", P3C),
         ("P0 · event_thread", 1.6, 1.9, "", P0C),
         ("P0 · event_thread", 5.6, 6.0, "vidas--\n(una vez)", P0C),
         ("bomber_lives", 5.6, 8.8, "3 → 2", OK)],
        5.5, "tick del doble golpe (t5)",
        ("ÚNICO ESCRITOR (corregido)",
         "P2 y P3 solo publican el evento\nP0 descuenta 1 vida: 3 → 2"),
        ("P2 y P3 ESCRIBEN VIDAS (con falla)",
         "ambos leen 3 y escriben 2:\nse pierde un descuento"))
    # --- Familia 2a: barrera
    diagrama_temporal(
        "f2_temporal.pdf",
        "Familia 2 — Diagrama temporal: la barrera antes de collision_thread",
        ["P2 · enemy_thread A–D", "sem_enemy_done", "P2 · collision_thread",
         "Lectura de posiciones"],
        [("P2 · enemy_thread A–D", 1, 1.6, "A", P2C), ("P2 · enemy_thread A–D", 2, 2.6, "B", P2C),
         ("P2 · enemy_thread A–D", 3, 3.6, "C", P2C), ("P2 · enemy_thread A–D", 4, 4.6, "D", P2C),
         ("sem_enemy_done", 5, 5.5, "4 sem_post\n(= vivos)", AQUA),
         ("P2 · collision_thread", 6, 6.6, "compara con\ncopia de P1", P0C),
         ("Lectura de posiciones", 6, 6.6, "4 posiciones\nfinales", OK)],
        6.0, "inicio de collision_thread (t6)",
        ("CON BARRERA (corregido)",
         "collision espera 4 sem_enemy_done:\nlee las 4 posiciones finales"),
        ("SIN BARRERA (con falla)",
         "collision corre mientras C y D\naún se mueven: lectura sucia"))
    # --- Familia 3: fases P3
    diagrama_temporal(
        "f3_temporal.pdf",
        "Familia 3 — Diagrama temporal: el orden fuse → explosion dentro de P3",
        ["P3 · bomb_controller", "P3 · fuse_thread", "P3 · explosion_thread",
         "timer de la bomba"],
        [("P3 · bomb_controller", 1, 1.4, "registra\n(timer 3)", P3C),
         ("P3 · fuse_thread", 2, 2.4, "3→2", P3C), ("P3 · fuse_thread", 3, 3.4, "2→1", P3C),
         ("P3 · fuse_thread", 4, 4.4, "1→0", P3C),
         ("P3 · explosion_thread", 4.5, 4.9, "EXPLOTA", BUG),
         ("timer de la bomba", 1.2, 4.5, "3 · 2 · 1 · 0", OK)],
        4.45, "tick t + 3 (t4)",
        ("FUSE Y LUEGO EXPLOSION (corregido)",
         "explosion espera sem_fuse_done:\nla bomba de t1 explota en t4"),
        ("EN PARALELO (con falla)",
         "explosion revisa antes del 1→0:\nla bomba explota tarde, en t5"),
        n_ticks=6)
    # --- Familia 4: buzon
    diagrama_temporal(
        "f4_temporal.pdf",
        "Familia 4 — Diagrama temporal: lectura del buzón a medio actualizar",
        ["P1 · executor (escritor)", "P3 · controller (lector)",
         "Buzón (x, y, active)", "Eventos críticos"],
        [("P1 · executor (escritor)", 2, 2.5, "x = 3", P1C),
         ("P1 · executor (escritor)", 4.6, 5.1, "y = 3\nactive = 1", P1C),
         ("P3 · controller (lector)", 4, 4.5, "lee buzón", P3C),
         ("Buzón (x, y, active)", 4, 4.5, "x nuevo\ny viejo", AQUA),
         ("Eventos críticos", 4, 4.5, "TORN READ", BUG)],
        4.0, "lectura del buzón (t4)",
        ("CON bomb_req_mtx (corregido)",
         "P1 tiene el lock en (x, y)\nP3 espera y lee el par entero"),
        ("SIN MUTEX (con falla)",
         "P3 lee en el hueco: x = 3, y = 1\nla bomba va a una celda errónea"))
    # --- Familia 5: cola
    diagrama_temporal(
        "f5_temporal.pdf",
        "Familia 5 — Diagrama temporal: la cola llena de P1",
        ["P1 · movement_reader", "P1 · movement_executor", "Ocupación de la cola",
         "Eventos críticos"],
        [("P1 · movement_reader", 1, 3.0, "llena 100 casilleros", P1C),
         ("P1 · movement_reader", 4, 4.5, "escribe\nitems[tail]", P1C),
         ("P1 · movement_executor", 2, 2.4, "1 instr.", P2C),
         ("P1 · movement_executor", 5, 5.4, "1 instr.", P2C),
         ("Ocupación de la cola", 3, 5.0, "100 / 100 (llena)", AQUA),
         ("Eventos críticos", 4, 4.5, "sobrescribe", BUG)],
        4.0, "la cola está llena (t4)",
        ("CON sem_espacios (corregido)",
         "reader se bloquea en sem_wait\nhasta que el executor libere uno"),
        ("SIN sem_espacios (con falla)",
         "reader da la vuelta al buffer:\npisa instrucciones no ejecutadas"))
    # --- Familia 6: cierre
    diagrama_temporal(
        "f6_temporal.pdf",
        "Familia 6 — Diagrama temporal: el sem_post que libera el cierre",
        ["P0 · signal_thread", "P1 / P2 / P3 (hijos)", "Semáforos de turno",
         "Eventos críticos"],
        [("P0 · signal_thread", 3, 3.5, "game_over = 1", P0C),
         ("P0 · signal_thread", 7, 7.5, "waitpid ✓", P0C),
         ("P1 / P2 / P3 (hijos)", 5, 5.5, "despiertan\ny salen", P1C),
         ("Semáforos de turno", 4, 4.5, "sem_post ×4", AQUA),
         ("Eventos críticos", 4, 4.5, "liberación", OK)],
        4.0, "tick de cierre (t4)",
        ("CON LIBERACIÓN (corregido)",
         "P0 hace sem_post a cada turno\nhijos despiertan → waitpid ✓"),
        ("SIN LIBERACIÓN (con falla)",
         "hijos siguen en sem_wait\nwaitpid se cuelga → DEADLOCK"))


# ---------------------------------------------------------------------------
# Figura del ciclo de un tick (metodologia)
# ---------------------------------------------------------------------------
def figura_tick():
    """Dos ticks (P1 y luego P2) contados igual que en la Sección 3.2: Paso 1 a Paso 4."""
    SEM = "#2f3fb0"
    P4C = "#6250d6"
    carriles = [
        ("P0", "Hilo del tick"), ("P0", "Hilo planificador"), ("P0", "Hilo de señales"), ("P0", "Hilo de eventos"),
        ("P1", "Lector (productor)"), ("P1", "Ejecutor (consumidor)"), ("P1", "Publicador"),
        ("P2", "Controlador"), ("P2", "Enemigos ×4 (en paralelo)"), ("P2", "Rastreador"),
        ("P2", "Detector de colisión"), ("P2", "Publicador "),
        ("P3", "Controlador de bombas"), ("P3", "Temporizador"), ("P3", "Explosión"),
        ("P4", "Render (solo con -r)"),
    ]
    color_proc = {"P0": P0C, "P1": P1C, "P2": P2C, "P3": P3C, "P4": P4C}
    nombres = [c[1] for c in carriles]
    n = len(carriles)

    def y(nombre):
        return n - 1 - nombres.index(nombre)

    H = 0.5
    barras = []      # (carril, t0, t1, texto)
    flechas = []     # (carril_origen, t, carril_destino, t_destino)
    pasos = []       # (t0, t1, etiqueta)

    def tick(b, turno):
        # Paso 1: reloj y decision de turno
        barras.append(("Hilo del tick", b + 0.15, b + 0.55, "reloj +1"))
        barras.append(("Hilo planificador", b + 0.65, b + 1.15, "elige turno"))
        flechas.append(("Hilo del tick", b + 0.55, "Hilo planificador", b + 0.65))
        pasos.append((b, b + 1.25, "Paso 1\ninicio"))
        # Paso 2: fase de accion
        s0 = b + 1.25
        barras.append(("Hilo de señales", s0, s0 + 0.15, ""))
        flechas.append(("Hilo planificador", b + 1.15, "Hilo de señales", s0))
        if turno == "P1":
            barras.append(("Ejecutor (consumidor)", s0 + 0.3, s0 + 1.1, "saca 1 instrucción"))
            barras.append(("Publicador", s0 + 1.2, s0 + 1.8, "publica posición"))
            flechas.append(("Hilo de señales", s0 + 0.15, "Ejecutor (consumidor)", s0 + 0.3))
            flechas.append(("Ejecutor (consumidor)", s0 + 1.1, "Publicador", s0 + 1.2))
            fin_turno, desde = s0 + 1.95, ("Publicador", s0 + 1.8)
        else:
            barras.append(("Controlador", s0 + 0.3, s0 + 0.55, "despierta"))
            barras.append(("Enemigos ×4 (en paralelo)", s0 + 0.6, s0 + 1.3, "cada uno 1 mov."))
            barras.append(("Rastreador", s0 + 1.45, s0 + 1.85, "copia pos."))
            barras.append(("Detector de colisión", s0 + 1.95, s0 + 2.35, "¿choque?"))
            barras.append(("Publicador ", s0 + 2.45, s0 + 2.85, "publica"))
            flechas.append(("Hilo de señales", s0 + 0.15, "Controlador", s0 + 0.3))
            flechas.append(("Controlador", s0 + 0.55, "Enemigos ×4 (en paralelo)", s0 + 0.6))
            flechas.append(("Enemigos ×4 (en paralelo)", s0 + 1.3, "Rastreador", s0 + 1.45))
            flechas.append(("Rastreador", s0 + 1.85, "Detector de colisión", s0 + 1.95))
            flechas.append(("Detector de colisión", s0 + 2.35, "Publicador ", s0 + 2.45))
            barreras.append(s0 + 1.37)
            fin_turno, desde = s0 + 3.0, ("Publicador ", s0 + 2.85)
        barras.append(("Hilo de señales", fin_turno, fin_turno + 0.15, ""))
        flechas.append((desde[0], desde[1], "Hilo de señales", fin_turno))
        pasos.append((s0, fin_turno + 0.1, f"Paso 2\nturno de {turno}"))
        # Paso 3: bombas (P3 juega siempre)
        q0 = fin_turno + 0.15
        barras.append(("Controlador de bombas", q0 + 0.15, q0 + 2.05, ""))
        barras.append(("Explosión", q0 + 0.25, q0 + 0.6, "limpia"))
        barras.append(("Temporizador", q0 + 0.7, q0 + 1.1, "timer −1"))
        barras.append(("Explosión", q0 + 1.2, q0 + 1.65, "explota"))
        flechas.append(("Hilo de señales", q0, "Controlador de bombas", q0 + 0.15))
        flechas.append(("Controlador de bombas", q0 + 2.05, "Hilo de señales", q0 + 2.2))
        barras.append(("Hilo de señales", q0 + 2.2, q0 + 2.35, ""))
        pasos.append((q0 - 0.05, q0 + 2.4, "Paso 3\nbombas"))
        # Paso 4: eventos, render y fin
        e0 = q0 + 2.45
        barras.append(("Hilo de eventos", e0 + 0.05, e0 + 0.75, "eventos"))
        barras.append(("Render (solo con -r)", e0 + 0.85, e0 + 1.35, "dibuja"))
        barras.append(("Hilo de eventos", e0 + 1.45, e0 + 1.75, "¿fin?"))
        flechas.append(("Hilo de señales", q0 + 2.35, "Hilo de eventos", e0 + 0.05))
        flechas.append(("Hilo de eventos", e0 + 0.75, "Render (solo con -r)", e0 + 0.85))
        flechas.append(("Render (solo con -r)", e0 + 1.35, "Hilo de eventos", e0 + 1.45))
        pasos.append((e0, e0 + 1.85, "Paso 4\ncierre"))
        return e0 + 1.85

    barreras = []
    fin1 = tick(0.0, "P1")
    b2 = fin1 + 0.35
    fin2 = tick(b2, "P2")

    fig, ax = plt.subplots(figsize=(7.4, 6.3))
    ax.grid(False)
    for sp in ("left", "top", "right", "bottom"):
        ax.spines[sp].set_visible(False)
    ax.set_xlim(-0.1, fin2 + 0.1)
    ax.set_ylim(-0.7, n + 1.15)

    # franjas de los pasos
    for i, (t0, t1, etq) in enumerate(pasos):
        ax.add_patch(Rectangle((t0, -0.35), t1 - t0, n + 0.25, color="#eef1f8" if i % 2 == 0 else "#f8f8f6",
                               lw=0, zorder=0))
        ax.text((t0 + t1) / 2, n + 0.0, etq, ha="center", va="bottom", fontsize=6.8, color=INK,
                fontweight="bold", linespacing=1.15)
    # carriles
    for i, (proc, nom) in enumerate(carriles):
        yy = n - 1 - i
        ax.plot([-0.1, fin2 + 0.1], [yy, yy], color=GRID, lw=0.8, zorder=1)
    ax.set_yticks([n - 1 - i for i in range(n)])
    ax.set_yticklabels([c[1].strip() for c in carriles], fontsize=7.6)
    ax.tick_params(axis="y", length=0, pad=4)
    for lab, (proc, _) in zip(ax.get_yticklabels(), carriles):
        lab.set_color(color_proc[proc])
    # llaves de proceso a la izquierda
    grupos = {}
    for i, (proc, _) in enumerate(carriles):
        grupos.setdefault(proc, []).append(n - 1 - i)
    titulos = {"P0": "P0\nárbitro", "P1": "P1\nBomberman", "P2": "P2\nenemigos", "P3": "P3\nbombas", "P4": "P4"}
    for proc, ys in grupos.items():
        y0, y1 = min(ys) - 0.1, max(ys) + 0.6
        tr = ax.get_yaxis_transform()
        ax.plot([-0.255, -0.255], [y0, y1], color=color_proc[proc], lw=2.2, clip_on=False,
                solid_capstyle="butt", transform=tr)
        ax.text(-0.265, (y0 + y1) / 2, titulos[proc], ha="right", va="center", fontsize=7.6,
                color=color_proc[proc], fontweight="bold", clip_on=False, linespacing=1.1, transform=tr)
    # barras
    for (c, t0, t1, txt) in barras:
        yy = y(c)
        proc = carriles[nombres.index(c)][0]
        col = color_proc[proc]
        ax.plot([t0, t0, t1, t1], [yy, yy + H, yy + H, yy], color=col, lw=1.6, zorder=3)
        ax.add_patch(Rectangle((t0, yy), t1 - t0, H, color=col, alpha=0.15, lw=0, zorder=2))
        if txt:
            ax.text((t0 + t1) / 2, yy + H + 0.05, txt, ha="center", va="bottom", fontsize=6.1, color=INK2, zorder=4)
    # flechas de semaforo (quien despierta a quien)
    for (c0, t0, c1, t1) in flechas:
        ax.annotate("", xy=(t1, y(c1) + H / 2), xytext=(t0, y(c0) + H / 2),
                    arrowprops=dict(arrowstyle="-|>", color=SEM, lw=0.7, alpha=0.75, shrinkA=0, shrinkB=0,
                                    mutation_scale=7), zorder=5)
    # barrera de P2
    for t in barreras:
        y0, y1 = y("Publicador ") - 0.1, y("Enemigos ×4 (en paralelo)") + H + 0.1
        ax.plot([t, t], [y0, y1], color=P2C, lw=1.2, ls=(0, (2, 1.5)), zorder=4)
        ax.text(t, y0 - 0.05, "barrera", fontsize=6.1, color=P2C, fontweight="bold", ha="center", va="top")
    # el lector de P1 llena la cola al inicio y solo vuelve cuando hay espacio
    ax.plot([0.15, 0.15, 1.05, 1.05], [y("Lector (productor)")] + [y("Lector (productor)") + H] * 2 +
            [y("Lector (productor)")], color=P1C, lw=1.6, zorder=3)
    ax.add_patch(Rectangle((0.15, y("Lector (productor)")), 0.9, H, color=P1C, alpha=0.15, lw=0, zorder=2))
    ax.text(0.6, y("Lector (productor)") + H + 0.05, "llena la cola", ha="center", va="bottom", fontsize=6.1, color=INK2)
    ax.text(1.15, y("Lector (productor)") + 0.08, "→ duerme hasta que haya espacio", ha="left", va="bottom",
            fontsize=6.1, color=INK2, style="italic")

    ax.set_xticks([pasos[0][0] + 0.05, pasos[4][0] + 0.05])
    ax.set_xticklabels(["tick 1: le toca a P1 (prioridades iguales)", "tick 2: le toca a P2 (alternancia)"],
                       fontsize=7.8, ha="left")
    ax.tick_params(axis="x", length=0)
    ax.set_title("Un tick paso a paso: dos ticks seguidos (P1 y luego P2)", loc="left", pad=8, x=-0.27)
    fig.text(0.02, 0.0,
             "Barra alta = el hilo está trabajando; línea baja = el hilo duerme en sem_wait. "
             "Flecha azul = semáforo: el hilo de la izquierda despierta al de la derecha (sem_post).\n"
             "Paso 2: P0 despierta al elegido (sem_bomber_turn o sem_enemy_turn) y espera sem_turn_done. "
             "En P2, nadie compara posiciones antes de la barrera (sem_enemy_done).",
             fontsize=6.9, color=INK2, linespacing=1.45)
    fig.savefig(FIG / "tick.pdf")
    plt.close(fig)


# ---------------------------------------------------------------------------
# Turnos reales por tick (logs del juego)
# ---------------------------------------------------------------------------
def figura_turnos():
    # (titulo, ticks, P1, P2, P3, nota, bomba=(pide, explota), cambio de prioridad, tick con vida perdida)
    escenarios = [
        ("A · prioridades 20 / 30  (./bomberman tests/caso_ejemplo 8 20 30)",
         8, {}, {t: "" for t in range(1, 9)}, {},
         "P2 tiene más prioridad: juega siempre y P1 nunca recibe turno (inanición)",
         None, None, None),
        ("B · prioridades 40 / 30  (ejemplo 8.5: ./bomberman tests/caso_ejemplo 9 40 30)",
         9, {1: "RIGHT", 2: "RIGHT", 3: "BOMB", 4: "LEFT", 5: "LEFT", 6: "DOWN", 7: "SET 25"},
         {8: "", 9: ""}, {3: "registra", 4: "3→2", 5: "2→1", 6: "EXPLOTA", 7: "limpia"},
         "P1 tiene más prioridad hasta que pide bajarla a 25",
         (3, 6), 7.5, None),
        ("C · prioridades 30 / 30  (./bomberman tests/caso_ejemplo 12 30 30)",
         12, {1: "RIGHT", 3: "RIGHT", 5: "BOMB", 7: "LEFT", 9: "LEFT", 11: "DOWN"},
         {2: "", 4: "", 6: "", 8: "", 10: "", 12: ""},
         {5: "registra", 6: "3→2", 7: "2→1", 8: "EXPLOTA", 9: "limpia"},
         "Empate: se alternan (Round Robin), empezando por P1",
         (5, 8), None, 8),
    ]
    VIDA = "#c62828"
    fig, axs = plt.subplots(3, 1, figsize=(7.2, 7.4),
                            gridspec_kw={"height_ratios": [1, 1, 1], "hspace": 1.05})
    for ax, (tit, n, p1, p2, p3, nota, bomba, cambio, vida) in zip(axs, escenarios):
        ax.grid(False)
        for sp in ax.spines.values():
            sp.set_visible(False)
        ax.set_xlim(0.5, 12.5)
        ax.set_ylim(-0.5, 2.5)
        filas = [("P1 Bomberman", p1, P1C), ("P2 enemigos", p2, P2C), ("P3 bombas", p3, P3C)]
        for j, (nom, d, c) in enumerate(filas):
            y = 2 - j
            for t in range(1, n + 1):
                if t in d:                      # tuvo el turno / hubo trabajo con bombas
                    fc, al = c, 0.85
                elif j == 2:                    # P3 corre igual, pero sin bombas
                    fc, al = c, 0.3
                else:                           # sin turno
                    fc, al = "#f1f0ec", 1
                ax.add_patch(Rectangle((t - 0.47, y - 0.4), 0.94, 0.8, fc=fc, alpha=al, ec=SURF, lw=2))
                if t in d and d[t]:
                    ax.text(t, y, d[t], ha="center", va="center", fontsize=6.5,
                            color="white" if j < 2 else INK, fontweight="bold")
        # A: P1 nunca juega
        if not p1:
            ax.text((n + 1) / 2, 2, "sin turno en ningún tick", ha="center", va="center",
                    fontsize=7, color=MUTED, style="italic")
        # flecha de la bomba: P1 la pide -> P3 la registra en el mismo tick
        if bomba:
            t0, t1 = bomba
            ax.annotate("", xy=(t0, 0.42), xytext=(t0, 1.58),
                        arrowprops=dict(arrowstyle="-|>", color=INK2, lw=0.9, mutation_scale=7,
                                        shrinkA=0, shrinkB=0, connectionstyle="arc3,rad=-0.35"))
            # llave t+3 bajo los ticks
            yb = -1.05
            ax.plot([t0, t0, t1, t1], [yb + 0.12, yb, yb, yb + 0.12], color=P3C, lw=1.3, clip_on=False)
            ax.text((t0 + t1) / 2, yb - 0.08, f"pedida en T{t0} → explota en T{t1} (t+3)",
                    ha="center", va="top", fontsize=6.8, color=INK, clip_on=False)
        # B: cambio de prioridad aplicado por P0 al inicio del tick siguiente
        if cambio:
            ax.plot([cambio, cambio], [-0.5, 2.5], color=INK, lw=1.1, ls=(0, (3, 2)), clip_on=False)
            ax.annotate("P1 pidió prioridad 25 en T7;\nP0 la aplica al inicio de T8\n→ desde ahí gana P2 (30 > 25)",
                        xy=(cambio, 1.0), xytext=(9.75, 1.0), ha="left", va="center",
                        fontsize=6.8, color=INK, linespacing=1.2,
                        arrowprops=dict(arrowstyle="-|>", color=INK, lw=0.8, mutation_scale=7, shrinkA=2,
                                        shrinkB=0))
        # C: vida perdida
        if vida:
            ax.add_patch(Rectangle((vida - 0.47, -0.4), 0.94, 0.8, fc="none", ec=VIDA, lw=1.6, zorder=5))
            ax.text(vida + 0.35, -1.05, "← alcanza a Bomberman en (2,1): P0 le quita 1 vida",
                    ha="left", va="center", fontsize=6.8, color=VIDA, fontweight="bold", clip_on=False)
        ax.set_yticks([2, 1, 0])
        ax.set_yticklabels([f[0] for f in filas], fontsize=8)
        ax.set_xticks(range(1, n + 1))
        ax.set_xticklabels([f"T{t}" for t in range(1, n + 1)], fontsize=7.5)
        ax.tick_params(length=0)
        ax.set_title(tit, loc="left", fontsize=8.5, pad=16)
        ax.text(0.5, 2.62, nota, ha="left", va="bottom", fontsize=7.5, color=INK2, style="italic")
    # leyenda
    fig.subplots_adjust(bottom=0.17, top=0.95)
    lx, ly = 0.03, 0.045
    items = [(P1C, 0.85, "P1 / P2 con color: ese proceso tuvo el turno"),
             ("#f1f0ec", 1, "gris: no tuvo turno"),
             (P3C, 0.3, "P3 claro: corre, pero no hay bombas"),
             (P3C, 0.85, "P3 fuerte: trabaja con una bomba")]
    pos = [(lx, ly + 0.025), (lx + 0.5, ly + 0.025), (lx, ly), (lx + 0.5, ly)]
    for (c, al, txt), (x0, y0) in zip(items, pos):
        fig.patches.append(Rectangle((x0, y0), 0.022, 0.016, fc=c, alpha=al, ec="none",
                                     transform=fig.transFigure, figure=fig))
        fig.text(x0 + 0.03, y0 + 0.008, txt, fontsize=7.3, color=INK2, va="center")
    fig.text(lx, ly - 0.022, "P0 participa en todos los ticks (decide el turno al inicio). "
             "Datos tomados de la salida real del programa.", fontsize=7.2, color=MUTED)
    fig.savefig(FIG / "turnos.pdf")
    plt.close(fig)


# ---------------------------------------------------------------------------
# Figuras con datos
# ---------------------------------------------------------------------------
def familia1():
    f = leer("e1_vidas")
    if not f:
        return
    ok = [int(r[3]) for r in f if r[1] == "ok"]
    bug = [int(r[3]) for r in f if r[1] == "buggy"]
    NUM["unoN"] = fmt(len(ok))
    NUM["unoOkPct"] = fmt(pct(lambda v: v == 0, ok), 1)
    NUM["unoBugPct"] = fmt(pct(lambda v: v == 0, bug), 1)
    NUM["unoBugMedia"] = fmt(st.mean(bug))
    NUM["unoBugMin"] = fmt(min(bug))
    NUM["unoBugMax"] = fmt(max(bug))
    NUM["unoBugSd"] = fmt(st.pstdev(bug))
    NUM["unoBugMediaPct"] = fmt(100 * st.mean(bug) / 2_000_000, 1)
    guardar_recursos("uno", f)
    dispersion("e1", "Vidas — corregido siempre en 0; con falla disperso (race)",
               "Descuentos perdidos en la corrida", ok, bug,
               "P0 único escritor (corregido)", "P2 y P3 escriben bomber_lives (con falla)",
               f"Corridas con vidas exactas: corregido {NUM['unoOkPct'].replace(chr(92)+',', '')} %  ·  con falla {NUM['unoBugPct']} %",
               "f1_dispersion.pdf")


def familia2():
    f = leer("e2a_barrera")
    if f:
        ok = [int(r[3]) for r in f if r[1] == "ok"]
        bug = [int(r[3]) for r in f if r[1] == "buggy"]
        NUM["dosN"] = fmt(len(ok))
        NUM["dosBugMedia"] = fmt(st.mean(bug), 1)
        NUM["dosBugMin"] = fmt(min(bug))
        NUM["dosBugMax"] = fmt(max(bug))
        NUM["dosBugSd"] = fmt(st.pstdev(bug), 1)
        NUM["dosBugLimpias"] = fmt(pct(lambda v: v == 0, bug), 1)
        NUM["dosOkLimpias"] = fmt(pct(lambda v: v == 0, ok), 1)
        guardar_recursos("dos", f)
        dispersion("e2a", "Barrera de P2 — con barrera siempre en 0; sin barrera disperso",
                   "Turnos con lectura sucia (de 200)", ok, bug,
                   "Con barrera sem_enemy_done (corregido)", "Sin barrera (con falla)",
                   f"Corridas sin lectura sucia: con barrera {NUM['dosOkLimpias']} %  ·  sin barrera {NUM['dosBugLimpias']} %",
                   "f2_dispersion.pdf")
    f = leer("e2b_equidad")
    if f:
        ok = [[int(x) for x in r[3:8]] for r in f if r[1] == "ok"]
        bug = [[int(x) for x in r[3:8]] for r in f if r[1] == "buggy"]
        des = [b[4] for b in bug]
        NUM["equiN"] = fmt(len(bug))
        NUM["equiBugMedia"] = fmt(st.mean(des))
        NUM["equiBugSd"] = fmt(st.pstdev(des))
        NUM["equiBugMax"] = fmt(max(des))
        NUM["equiOkMax"] = fmt(max(o[4] for o in ok))
        peor = max(bug, key=lambda b: b[4])
        NUM["equiPeorMax"] = fmt(max(peor[:4]))
        NUM["equiPeorMin"] = fmt(min(peor[:4]))
        med = sorted(bug, key=lambda b: b[4])[len(bug) // 2]
        for k in range(4):
            NUM["equiRep" + "ABCD"[k]] = fmt(med[k])
        NUM["equiRepDes"] = fmt(med[4])
        NUM["equiRepPct"] = fmt(100 * med[4] / 25000, 1)
        NUM["equiBugMediaPct"] = fmt(100 * st.mean(des) / 25000, 1)
        guardar_recursos("equi", f)
        fig, ax = plt.subplots(figsize=(6.6, 3.0))
        xs = range(4)
        w = 0.36
        ax.bar([x - w / 2 - 0.01 for x in xs], med[:4], w, color=BUG,
               label="Un semáforo compartido (con falla)")
        ax.bar([x + w / 2 + 0.01 for x in xs], ok[0][:4], w, color=OK,
               label="sem_start_enemy[i] por enemigo (corregido)")
        lo = min(min(med[:4]), 25000)
        hi = max(max(med[:4]), 25000)
        pad = max(200, (hi - lo) * 0.6)
        ax.set_ylim(lo - pad, hi + pad)
        ax.axhline(25000, color=MUTED, ls=(0, (3, 3)), lw=0.9)
        ax.text(3.55, 25000, "ideal 25 000", va="bottom", ha="right", fontsize=7.5, color=INK2)
        ax.set_xticks(list(xs))
        ax.set_xticklabels(["Enemigo A", "Enemigo B", "Enemigo C", "Enemigo D"])
        ax.set_ylabel("Turnos atendidos")
        ax.set_title("Equidad — reparto de turnos entre los cuatro enemy_thread", loc="left")
        ax.legend(loc="upper center", bbox_to_anchor=(0.5, -0.12), ncol=2)
        fig.text(0.5, -0.1, f"Corrida mediana. Desbalance (máx−mín) en {len(bug)} corridas: "
                 f"compartido {st.mean(des):.0f} ± {st.pstdev(des):.0f} turnos · por enemigo 0",
                 ha="center", fontsize=8, color=INK2)
        fig.savefig(FIG / "f2_barras.pdf")
        plt.close(fig)


def familia_simple(nombre, clave, col, titulo, ylabel, lok, lbug, pie_fmt, archivo, total=None):
    f = leer(nombre)
    if not f:
        return
    ok = [int(r[col]) for r in f if r[1] == "ok"]
    bug = [int(r[col]) for r in f if r[1] == "buggy"]
    NUM[clave + "N"] = fmt(len(ok))
    NUM[clave + "BugMedia"] = fmt(st.mean(bug), 1 if st.mean(bug) < 100 else 0)
    NUM[clave + "BugMin"] = fmt(min(bug))
    NUM[clave + "BugMax"] = fmt(max(bug))
    NUM[clave + "BugSd"] = fmt(st.pstdev(bug), 1 if st.pstdev(bug) < 100 else 0)
    NUM[clave + "BugLimpias"] = fmt(pct(lambda v: v == 0, bug), 1)
    NUM[clave + "OkLimpias"] = fmt(pct(lambda v: v == 0, ok), 1)
    NUM[clave + "OkMax"] = fmt(max(ok))
    if total:
        NUM[clave + "BugMaxPct"] = fmt(100 * max(bug) / total, 1)
        NUM[clave + "BugMediaPct"] = fmt(100 * st.mean(bug) / total, 1)
    guardar_recursos(clave, f)
    dispersion(nombre, titulo, ylabel, ok, bug, lok, lbug,
               pie_fmt.format(ok=NUM[clave + "OkLimpias"], bug=NUM[clave + "BugLimpias"]),
               archivo)
    return ok, bug


def familia3():
    r = familia_simple("e3_fases_p3", "tres", 3,
                       "Fases de P3 — en orden siempre en 0; en paralelo disperso",
                       "Bombas que explotan fuera de t+3",
                       "fuse → explosion (corregido)", "fuse ∥ explosion (con falla)",
                       "Corridas sin explosiones fuera de tiempo: en orden {ok} %  ·  en paralelo {bug} %",
                       "f3_dispersion.pdf")
    f = leer("e3_fases_p3")
    if f:
        NUM["tresBombas"] = fmt(st.mean(int(x[4]) for x in f if x[1] == "ok"))


def familia4():
    familia_simple("e4_buzon", "cuatro", 3,
                   "Buzón de bomba — con mutex siempre en 0; sin mutex disperso",
                   "Lecturas rotas del buzón", "Con bomb_req_mtx (corregido)",
                   "Sin mutex (con falla)",
                   "Corridas sin lectura rota: con mutex {ok} %  ·  sin mutex {bug} %",
                   "f4_dispersion.pdf", total=200000)


def familia5():
    familia_simple("e5_cola", "cinco", 3,
                   "Cola de P1 — con sem_espacios nunca se pierde; sin él se pierde casi todo",
                   "Instrucciones perdidas (de 200 000)", "Con sem_espacios (corregido)",
                   "Sin sem_espacios (con falla)",
                   "Corridas sin pérdidas: con sem_espacios {ok} %  ·  sin sem_espacios {bug} %",
                   "f5_dispersion.pdf", total=200000)


def familia6():
    a = leer("e6a_cierre")
    b = leer("e6b_orden_mutex")
    if not a or not b:
        return
    fig, axs = plt.subplots(1, 2, figsize=(7.2, 3.0), sharey=True)
    resumen = {}
    for ax, (filas, tit, clave) in zip(axs, ((a, "(a) Cierre: liberación de semáforos", "seisA"),
                                              (b, "(b) Orden global de mutex", "seisB"))):
        for var, color, nom in (("ok", OK, "corregido"), ("buggy", BUG, "con falla")):
            sel = [r for r in filas if r[1] == var]
            tiempos = []
            for r in sel:
                cerro = r[3] == "1"
                t = float(r[4]) if cerro else None
                tiempos.append(t)
            cerr = sorted(t for t in tiempos if t is not None)
            n = len(sel)
            pref = "Ok" if var == "ok" else "Bug"
            NUM[clave + pref + "Cierra"] = fmt(100 * len(cerr) / n)
            NUM[clave + "N"] = fmt(n)
            if cerr:
                NUM[clave + pref + "Ms"] = fmt(st.median(cerr), 1)
            xs = [0.5] + cerr + [1000]
            ys = [0] + [100 * (i + 1) / n for i in range(len(cerr))] + [100 * len(cerr) / n]
            ax.step(xs, ys, where="post", color=color, lw=2,
                    label=f"{'Corregido' if var == 'ok' else 'Con falla'} → {len(cerr)}/{n} cierran")
            resumen[(clave, var)] = recursos(filas, var)
        ax.axvline(1000, color=MUTED, ls=(0, (3, 3)), lw=0.9)
        ax.text(950, 50, "timeout 1 s\n(censura)", ha="right", fontsize=7, color=INK2)
        ax.set_xscale("log")
        ax.set_xlim(0.5, 1500)
        ax.set_ylim(-3, 105)
        ax.set_xlabel("Tiempo hasta cerrar (ms, escala log)")
        ax.set_title(tit, loc="left", fontsize=9)
        ax.legend(loc="center left", fontsize=7.5)
    axs[0].set_ylabel("% de corridas que ya cerraron")
    fig.suptitle("Deadlock — curva de terminación (ECDF)", x=0.02, ha="left",
                 fontweight="bold", fontsize=10, y=1.02)
    fig.savefig(FIG / "f6_ecdf.pdf")
    plt.close(fig)
    guardar_recursos("seisA", a)
    guardar_recursos("seisB", b)
    # el tiempo se toma del reloj interno del programa (ms), mas preciso que /usr/bin/time
    for clave, filas in (("seisA", a), ("seisB", b)):
        for var, pref in (("ok", "Ok"), ("buggy", "Bug")):
            ms = [float(r[4]) for r in filas if r[1] == var]
            NUM[f"{clave}{pref}Tiempo"] = fmt(st.mean(ms) / 1000, 4)


def juego():
    f = leer("e0_juego")
    if not f:
        return
    casos = defaultdict(list)
    fin = {}
    for r in f:
        casos[r[0]].append(r)
        if r[-1] != "-":
            fin[r[0]] = r[-1]
    filas = []
    for k, rs in casos.items():
        hashes = {r[2] for r in rs}
        t = st.mean(float(r[3]) for r in rs)
        m = max(float(r[5]) for r in rs)
        c = st.mean(float(r[4]) for r in rs)
        filas.append((k, len(rs), len(hashes), t, m, fin.get(k, ""), c))
    with open(FIG / "tabla_juego.tex", "w", encoding="utf-8") as out:
        for (k, n, h, t, m, fn, c) in filas:
            caso, mx, pb, pe = k.split("_")[:-3], *k.split("_")[-3:]
            nombre = "\\_".join(k.split("_")[:-3])
            g = re.search(r"resultado=?(\d+) tick=?(\d+) vidas=?(\d+) score=?(\d+)", fn)
            res = " / ".join(g.groups()) if g else fn
            ms = "$<$10" if t * 1000 < 10 else f"{t*1000:.0f}"
            out.write(f"\\texttt{{{nombre}}} & {mx} & {pb}/{pe} & {n} & {h} & {res} & {ms} & {c:.0f} & {m/1024:.1f} \\\\\n")
        out.write("\\bottomrule\n")
    NUM["juegoN"] = fmt(filas[0][1])
    total = sum(f[1] for f in filas)
    NUM["juegoTotal"] = fmt(total)
    NUM["juegoTiempo"] = f"{st.mean(float(r[3]) for r in f):.3f}"
    NUM["juegoCpu"] = fmt(st.mean(float(r[4]) for r in f))
    NUM["juegoMem"] = fmt(max(float(r[5]) for r in f))
    malos = [f for f in filas if f[2] != 1]
    if not malos:
        NUM["JuegoTexto"] = (
            f"\\textbf{{Resultado.}} En los {len(filas)} escenarios, las {fmt(total)} corridas "
            "produjeron exactamente una salida distinta por caso: el mismo orden de turnos, las mismas "
            "explosiones en los mismos ticks y el mismo resultado final. En estos escenarios ninguna race condition "
            "altera el resultado. Esto no prueba que no existan: la Familia~2c muestra una race condition real que "
            "estos casos no ejercitan.")
    else:
        det = "; ".join(f"\\texttt{{{'\\_'.join(m[0].split('_')[:-3])}}}: {m[2]} salidas distintas" for m in malos)
        NUM["JuegoTexto"] = (
            f"\\textbf{{Resultado.}} De los {len(filas)} escenarios, {len(filas) - len(malos)} produjeron una "
            f"única salida en todas sus corridas; los demás mostraron variación ({det}), lo que indica una "
            "race condition residual en el sistema completo.")


def tracker():
    f = leer("e2c_tracker")
    if not f:
        NUM["trackN"] = "0"
        NUM["TrackerTexto"] = "(pendiente)"
        return
    por = defaultdict(list)
    for r in f:
        por[r[0]].append(int(r[2]))
    n = len(por["real"])
    NUM["trackN"] = fmt(n)

    def ok(v):
        return pct(lambda x: x == 2, v)
    perdidas_real = sum(1 for x in por["real"] if x != 2)
    NUM["trackRealPierde"] = fmt(100 - ok(por["real"]), 1)
    NUM["TrackerTexto"] = (
        f"Lo correcto es terminar con 2 vidas. La versión anterior de P2, sin ninguna amplificación, "
        f"\\textbf{{perdió el choque en el {fmt(100 - ok(por['real']), 1)}\\,\\% de las corridas}} ({perdidas_real} de {fmt(n)}). "
        f"Con el retardo, solo el {fmt(ok(por['real+amp']), 1)}\\,\\% detecta el choque; con la corrección, el "
        f"{fmt(ok(por['corregido']), 1)}\\,\\%. Los casos de prueba normales no la mostraban porque en ellos Bomberman "
        "llevaba varios turnos quieto: la copia vieja y la nueva coincidían.")
    fig, ax = plt.subplots(figsize=(6.6, 2.1))
    etiquetas = ["Versión anterior", "Versión anterior + retardo 2 ms", "Versión final + retardo 2 ms"]
    vals = [ok(por["real"]), ok(por["real+amp"]), ok(por["corregido"])]
    colores = [BUG, BUG, OK]
    ys = [2, 1, 0]
    ax.barh(ys, vals, color=colores, height=0.55)
    for y, v in zip(ys, vals):
        ax.text(min(v, 100) + 1.2 if v < 88 else v - 1.2, y, f"{v:.1f} %", va="center",
                ha="left" if v < 88 else "right", fontsize=8.5,
                color=INK if v < 88 else "white", fontweight="bold")
    ax.set_yticks(ys)
    ax.set_yticklabels(etiquetas)
    ax.set_xlim(0, 100)
    ax.set_xlabel("% de corridas que detectan el choque del tick 2 (correcto = 100 %)")
    ax.grid(axis="y", visible=False)
    ax.set_title("Rastreador antes del detector — código real de P2 (caso_tracker)", loc="left")
    fig.savefig(FIG / "f2c_barras.pdf")
    plt.close(fig)


def familia7():
    """Juego real completo antes (commit fb8f114) y despues de las correcciones de integracion."""
    import csv
    datos = {}
    for esc in ("e7a", "e7b"):
        for ver in ("antes", "ahora"):
            ruta = DATOS / f"{esc}_{ver}.csv"
            if not ruta.exists():
                return
            with open(ruta, encoding="utf-8") as fh:
                filas = list(csv.DictReader(fh))
            datos[(esc, ver)] = filas
    fig, axs = plt.subplots(1, 2, figsize=(7.2, 2.5), sharey=True)
    titulos = {"e7a": "(i) archivo de 300 líneas", "e7b": "(ii) Bomberman sin instrucciones"}
    for ax, esc in zip(axs, ("e7a", "e7b")):
        for ver, color, nombre in (("ahora", OK, "versión final"), ("antes", BUG, "versión anterior")):
            filas = datos[(esc, ver)]
            n = len(filas)
            cerr = sorted(float(r["tiempo_s"]) for r in filas if r["valor"] != "colgado")
            pref = esc[-1].upper() + ("Ahora" if ver == "ahora" else "Antes")
            NUM["siete" + pref + "Cierra"] = fmt(100 * len(cerr) / n)
            NUM["siete" + pref + "Tiempo"] = fmt(st.mean(float(r["tiempo_s"]) for r in filas), 3)
            NUM["siete" + pref + "Cpu"] = fmt(st.mean(float(r["cpu_pct"]) for r in filas))
            NUM["siete" + pref + "Mem"] = fmt(max(float(r["mem_kb"]) for r in filas))
            NUM["sieteN"] = fmt(n)
            xs = [0.005] + cerr + [2.0]
            ys = [0] + [100 * (i + 1) / n for i in range(len(cerr))] + [100 * len(cerr) / n]
            ax.step(xs, ys, where="post", color=color, lw=2,
                    ls="-" if ver == "ahora" else (0, (4, 2)),
                    label=f"{nombre} → {len(cerr)}/{n} terminan")
        ax.axvline(2.0, color=MUTED, ls=(0, (3, 3)), lw=0.9)
        ax.text(1.85, 15, "timeout 2 s", ha="right", fontsize=7, color=INK2)
        ax.set_xscale("log")
        ax.set_xlim(0.005, 2.5)
        ax.set_ylim(-3, 105)
        ax.set_xlabel("Tiempo hasta terminar (s, escala log)")
        ax.set_title(titulos[esc], loc="left", fontsize=9)
        ax.legend(loc="center left", fontsize=7.5)
    axs[0].set_ylabel("% de partidas terminadas")
    fig.suptitle("Juego completo antes y después de la integración (ECDF)", x=0.02, ha="left",
                 fontweight="bold", fontsize=10, y=1.03)
    fig.savefig(FIG / "f7_ecdf.pdf")
    plt.close(fig)


def escribir_numeros():
    with open(FIG / "numeros.tex", "w", encoding="utf-8") as out:
        out.write("% Generado por experimentos/graficos.py: no editar a mano\n")
        for k, v in sorted(NUM.items()):
            out.write(f"\\newcommand{{\\N{k}}}{{{v}}}\n")


if __name__ == "__main__":
    figura_tick()
    figura_turnos()
    figuras_esquematicas()
    familia1()
    familia2()
    familia3()
    familia4()
    familia5()
    familia6()
    juego()
    tracker()
    familia7()
    escribir_numeros()
    print("listo:", len(NUM), "numeros")
