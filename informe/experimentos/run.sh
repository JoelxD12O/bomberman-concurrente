#!/usr/bin/env bash
# Corre todos los experimentos (version normal y BUGGY) y guarda los CSV en datos/.
# Uso (en Linux/WSL, desde informe/experimentos):  ./run.sh [carpeta_del_juego]
#   carpeta_del_juego: donde esta el binario ./bomberman ya compilado (por defecto ../..)
set -u
cd "$(dirname "$0")"
JUEGO="${1:-../..}"
mkdir -p bin datos

for f in e*.c; do
    n="${f%.c}"
    gcc -O2 -Wall -pthread -o "bin/${n}_ok" "$f"
    gcc -O2 -Wall -pthread -DBUGGY -o "bin/${n}_buggy" "$f"
done

T=$(mktemp)   # salida de /usr/bin/time
# los binarios se ejecutan desde un directorio local (en WSL, /mnt/c es lento)
B=$(mktemp -d)
cp bin/* "$B/"

# corre <exp> <iteraciones> [timeout]: una fila por corrida
#   exp,variante,iter,salida...,tiempo_s,cpu_pct,maxrss_kb
corre() {
    local exp="$1" iters="$2" lim="${3:-}"
    local out="datos/${exp}.csv"
    : > "$out"
    for v in ok buggy; do
        for ((i = 1; i <= iters; i++)); do
            if [ -n "$lim" ]; then
                res=$(/usr/bin/time -f "%e,%P,%M" -o "$T" timeout "$lim" "$B/${exp}_$v" 2>/dev/null)
                [ -z "$res" ] && res="colgado"
                pkill -9 "${exp:0:12}" 2>/dev/null
            else
                res=$(/usr/bin/time -f "%e,%P,%M" -o "$T" "$B/${exp}_$v" 2>/dev/null)
            fi
            res="${res// /,}"
            echo "$exp,$v,$i,$res,$(tail -1 "$T" | tr -d '%')" >> "$out"
        done
        echo "$exp $v listo"
    done
}

# SOLO=micro -> solo los micro-experimentos; SOLO=juego -> solo el juego real;
# SOLO=<experimento> -> solo ese (ej. SOLO=e6a_cierre)
case "${SOLO:-}" in e[0-9]*) corre "$SOLO" "${ITER:-50}"; rm -rf "$T" "$B"; exit 0 ;; esac
if [ -z "${SOLO:-}" ] || [ "$SOLO" = "micro" ]; then
    corre e1_vidas        1000
    corre e2a_barrera      500
    corre e3_fases_p3      500
    corre e4_buzon         500
    corre e5_cola          500
    corre e6b_orden_mutex   50
    corre e6a_cierre        50
    corre e2b_equidad      200
fi

if [ -z "${SOLO:-}" ] || [ "$SOLO" = "juego" ]; then
    # Validacion de extremo a extremo con el juego real: misma salida en todas las corridas.
    # Se copia el juego a un directorio local (en WSL, /mnt/c es lento para ejecutar).
    W=$(mktemp -d)
    cp "$JUEGO/bomberman" "$W/" && cp -r "$JUEGO/tests" "$W/"
    S=$(mktemp)
    out="$W/e0_juego.csv"
    : > "$out"
    for caso in "caso_ejemplo 9 40 30" "caso_ejemplo 12 30 30" "caso_demo 16 40 30" \
                "caso_victoria 20 40 30" "caso_derrota 20 40 30" "caso_sin_movimientos 30 30 30"; do
        set -- $caso
        for ((i = 1; i <= 300; i++)); do
            (cd "$W" && /usr/bin/time -f "%e,%P,%M" -o "$T" ./bomberman tests/$1 $2 $3 $4 > "$S")
            h=$(md5sum < "$S" | cut -c1-12)
            fin=$(tail -1 "$S" | tr -d '=' | sed 's/^ *//; s/ *$//')
            echo "$1_$2_$3_$4,$i,$h,$(tail -1 "$T" | tr -d '%'),$fin" >> "$out"
        done
        echo "juego $caso listo"
    done
    cp "$out" datos/e0_juego.csv
    rm -rf "$W" "$S"
fi
rm -rf "$T" "$B"
