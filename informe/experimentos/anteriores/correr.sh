#!/bin/bash
# Corre los experimentos del informe y guarda los resultados en informe/experimentos/anteriores/datos/*.csv
# Uso (dentro de Docker, en la carpeta del proyecto):  bash informe/experimentos/anteriores/correr.sh
# Cada fila: iteracion,valor,tiempo_s,cpu_pct,mem_kb   (tiempo/CPU/memoria con /usr/bin/time -v)
cd "$(dirname "$0")"
mkdir -p bin datos
CF="-Wall -O0 -pthread"

medir() {   # medir <archivo_csv> <iteraciones> <timeout_s> <comando...>
  local csv=$1 n=$2 to=$3; shift 3
  echo "iteracion,valor,tiempo_s,cpu_pct,mem_kb" > "$csv"
  for i in $(seq 1 "$n"); do
    /usr/bin/time -f "%e,%P,%M" -o /tmp/t.txt timeout -s KILL "$to" "$@" > /tmp/o.txt 2>/dev/null
    local cod=$?
    local valor; valor=$(head -1 /tmp/o.txt | awk '{print $1}')
    [ $cod -ne 0 ] && valor="colgado"
    local rec; rec=$(tail -1 /tmp/t.txt | tr -d '%')
    [[ "$rec" =~ ^[0-9.]+,[0-9?]+,[0-9]+$ ]] || rec="$to,0,0"
    echo "$i,$valor,$rec" >> "$csv"
  done
  echo "  $csv listo"
}

echo "== compilando"
gcc $CF -o bin/e1_con e1_cola.c;    gcc $CF -DSIN_ESPACIOS -o bin/e1_sin e1_cola.c
gcc $CF -o bin/e2_con e2_buzon.c;   gcc $CF -DSIN_MUTEX    -o bin/e2_sin e2_buzon.c
gcc $CF -o bin/e3_con e3_orden.c;   gcc $CF -DSIN_ORDEN    -o bin/e3_sin e3_orden.c
gcc $CF -o bin/e4_con e4_deadlock.c; gcc $CF -DSIN_ORDEN   -o bin/e4_sin e4_deadlock.c

# Experimento 5: el juego REAL en dos commits del repositorio
#   antes = 70a9e4a (merge del PR #22, antes del arreglo de cierre del PR #26)
#   ahora = el código actual
git config --global --add safe.directory '*' 2>/dev/null
rm -rf /tmp/antes && mkdir -p /tmp/antes && git -C ../../.. archive 70a9e4a | tar -x -C /tmp/antes
make -s -C /tmp/antes >/dev/null 2>&1 && cp /tmp/antes/bomberman bin/juego_antes
rm -rf /tmp/ahora && mkdir -p /tmp/ahora && cp -r ../../../src ../../../include ../../../Makefile /tmp/ahora/
make -s -C /tmp/ahora >/dev/null 2>&1 && cp /tmp/ahora/bomberman bin/juego_ahora

echo "== experimento 1: cola de P1 (500 x 2)"
medir datos/e1_con.csv 500 5 bin/e1_con;  medir datos/e1_sin.csv 500 5 bin/e1_sin
echo "== experimento 2: buzón entre procesos (500 x 2)"
medir datos/e2_con.csv 500 10 bin/e2_con; medir datos/e2_sin.csv 500 10 bin/e2_sin
echo "== experimento 3: orden tracker -> collision (500 x 2)"
medir datos/e3_con.csv 500 5 bin/e3_con;  medir datos/e3_sin.csv 500 5 bin/e3_sin
echo "== experimento 4: orden de mutex (50 x 2, timeout 1 s)"
medir datos/e4_con.csv 50 1 bin/e4_con;   medir datos/e4_sin.csv 50 1 bin/e4_sin
echo "== experimento 5: cierre del juego real (30 x 2 escenarios x 2 versiones, timeout 2 s)"
medir datos/e5a_ahora.csv 30 2 bin/juego_ahora casos/archivo_largo 20 20 30
medir datos/e5a_antes.csv 30 2 bin/juego_antes casos/archivo_largo 20 20 30
medir datos/e5b_ahora.csv 30 2 bin/juego_ahora ../../../tests/caso_sin_movimientos 30 40 30
medir datos/e5b_antes.csv 30 2 bin/juego_antes ../../../tests/caso_sin_movimientos 30 40 30
echo "== listo"
