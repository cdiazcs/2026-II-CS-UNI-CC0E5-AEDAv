#!/usr/bin/env bash
set -euo pipefail

repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT

fail() {
    printf 'FALLO: %s\n' "$*" >&2
    exit 1
}

expect_status() {
    local expected=$1 status=0
    shift
    "$@" || status=$?
    [[ $status == "$expected" ]] || fail "salida $status, se esperaba $expected: $*"
}

cd "$work"
for source in Makefile main.cpp demos.cpp demos.h types.h containers utilities foreach.h pc4_texto; do
    ln -s "$repo/$source" "$source"
done

TERM=dumb make clean
make -B main
mkdir module
for source in Makefile main.cpp texto_excel.h; do
    ln -s "$repo/pc4_texto/$source" "module/$source"
done
make -C module all
make -q main
plan=$(make -n -W pc4_texto/texto_excel.h main)
[[ $plan == *'main.cpp demos.cpp -o main'* ]] || fail 'el encabezado del módulo debe disparar la recompilación'

./main --texto > first.log
first=$(sed -n 's/^SAVE //p' first.log)
[[ $first == output/*/hoja.txt ]] || fail 'ruta de guardado inesperada'
printf 'archivo ajeno\n' > output/preexistente.txt
sha256sum "$first" output/preexistente.txt > before.sha
./main --texto > second.log
second=$(sed -n 's/^SAVE //p' second.log)
[[ $second == output/*/hoja.txt && $first != "$second" ]] || fail 'las dos ejecuciones deben usar rutas distintas'
sha256sum -c before.sha

for log in first.log second.log; do
    for line in 'GET A1 -> 10' 'RAW B1 -> =(A1+2)*3' 'GET B1 -> 36' 'GET B2 -> 16' \
                'GET C1 -> 30' 'GET C2 -> 15' 'GET C3 -> 2' 'GET C4 -> UNI: 10/20' \
                'GET D1 -> ERROR: Referencia circular: D1' 'GET D1 -> 6'; do
        grep -Fxq "$line" "$log" || fail "falta $line en $log"
    done
    [[ $(grep -Fxc 'GET B1 -> 36' "$log") == 2 ]] || fail 'B1 debe dar 36 después de LOAD'
done

for sheet in "$first" "$second"; do
    [[ $(head -n 1 "$sheet") == PC4TEXTO2 ]] || fail 'cabecera incorrecta'
    printf 'LOAD %s\nRAW B1\nGET B1\nGET D1\nEXIT\n' "$sheet" | ./module/texto > cli.log
    for line in '> Cargado' '> =(A1+2)*3' '> 36' '> 6'; do
        grep -Fxq "$line" cli.log || fail "el CLI no produjo $line"
    done
done

./main > normal.log
awk '
    BEGIN {
        split("Pruebas del contenedor Vector|DemoRaceCondition:|Pruebas para LinkedList|Pruebas para MinHeap|Pruebas para BinaryTree|Pruebas para HashTable|Pruebas para MerkleTree|Demo Gotoxy", labels, "|")
        next_label = 1
    }
    /Demo PC4 Texto/ { exit 1 }
    next_label <= 8 && index($0, labels[next_label]) { ++next_label }
    END { if (next_label != 9) exit 1 }
' normal.log || fail 'orden de demostraciones incorrecto'
expect_status 1 ./main --desconocido > invalid.log 2>&1
expect_status 1 ./main --texto extra > extra.log 2>&1
grep -q '^Uso:' invalid.log
grep -q '^Uso:' extra.log

mkdir invalid-output blocked-save
touch invalid-output/output
(cd invalid-output && expect_status 1 ../main --texto > ../invalid-output.log 2>&1)
grep -q '^Error en DemoTexto:' invalid-output.log

# Limitar escrituras regulares sin limitar el canal que captura la salida.
save_log=$(cd blocked-save; trap '' XFSZ; ulimit -f 0; expect_status 1 ../main --texto 2>&1)
grep -q '^Error en DemoTexto:' <<<"$save_log"
if grep -q '^SAVE ' <<<"$save_log"; then fail 'se anunció un guardado fallido'; fi

sha256sum "$second" >> before.sha
mkdir -p tmp build/docs
TERM=dumb make clean
[[ ! -e main && ! -e tmp && ! -e build/docs ]] || fail 'make clean dejó artefactos'
sha256sum -c before.sha
printf 'OK integración de PC4 Texto (%s y %s)\n' "$first" "$second"
