#!/usr/bin/env bash
set -euo pipefail

repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
bin=${PG18_BIN:-/usr/lib/postgresql/18/bin}
(( EUID != 0 )) || { echo 'El clúster privado requiere un usuario no root.' >&2; exit 1; }
[[ $("$bin/postgres" --version) == *' 18.'* ]] || { echo 'Se requiere PostgreSQL 18.' >&2; exit 1; }
work=$(mktemp -d /tmp/pc4-pg18.XXXXXX)
logs=$(mktemp -d /tmp/pc4-sql-evidence.XXXXXX)
echo "Evidencia: $logs"
cleanup() {
    if [[ -f $work/data/postmaster.pid ]]; then
        "$bin/pg_ctl" -D "$work/data" -m fast -w stop || {
            echo "No se pudo detener el clúster propio: $work" >&2
            return 1
        }
    fi
    rm -rf -- "$work"
}
trap cleanup EXIT
mkdir -m 700 "$work/socket"
"$bin/initdb" -D "$work/data" -U pc4_owner --auth=trust --no-locale -E UTF8 >"$logs/init.log" 2>&1
port=55483 # El socket vive en un directorio exclusivo y TCP queda deshabilitado.
"$bin/pg_ctl" -D "$work/data" -l "$logs/server.log" -w start \
    -o "-k $work/socket -p $port -c listen_addresses='' -c unix_socket_permissions=0700"
client=("$bin/psql" -X -w -v ON_ERROR_STOP=1 -h "$work/socket" -p "$port" -U pc4_owner)
"${client[@]}" -d postgres -c 'CREATE DATABASE pc4_index_test'
db=("${client[@]}" -d pc4_index_test)
"${db[@]}" -Atc 'SELECT version(); SHOW listen_addresses;' | tee "$logs/version.log"
"${db[@]}" <<'SQL'
CREATE SCHEMA pc4_sentinel;
CREATE TABLE pc4_sentinel.empleados (marker text);
CREATE TABLE pc4_sentinel.sesiones (marker text);
CREATE TABLE pc4_sentinel.lugares (marker text);
INSERT INTO pc4_sentinel.empleados VALUES ('intacto');
INSERT INTO pc4_sentinel.sesiones VALUES ('intacto');
INSERT INTO pc4_sentinel.lugares VALUES ('intacto');
SQL
for pass in 1 2; do
    for kind in btree hash gist; do
        PGOPTIONS='-c search_path=pc4_sentinel,public' "${db[@]}" \
            -f "$repo/pc4_texto/sql/${kind}_index.sql" >"$logs/$kind-$pass.log" 2>&1
    done
    report=$("${db[@]}" -At <<'SQL'
SELECT count(*) FROM pc4_texto_demo.empleados;
SELECT count(*) FROM pc4_texto_demo.sesiones;
SELECT count(*) FROM pc4_texto_demo.lugares;
SELECT c.relname, am.amname, i.indisvalid, pg_get_indexdef(c.oid)
FROM pg_index i JOIN pg_class c ON c.oid=i.indexrelid
JOIN pg_namespace n ON n.oid=c.relnamespace JOIN pg_am am ON am.oid=c.relam
WHERE n.nspname='pc4_texto_demo' AND c.relname IN ('idx_emp_salario','idx_ses_token','idx_lugares_pos')
ORDER BY c.relname;
SELECT marker FROM pc4_sentinel.empleados UNION ALL
SELECT marker FROM pc4_sentinel.sesiones UNION ALL SELECT marker FROM pc4_sentinel.lugares;
SQL
)
    printf '%s\n' "$report" | tee "$logs/catalog-$pass.log"
    [[ $report == $'100000\n100000\n50000\n'*$'\nintacto\nintacto\nintacto' ]]
    grep -q '^idx_emp_salario|btree|t|' "$logs/catalog-$pass.log"
    grep -q '^idx_ses_token|hash|t|' "$logs/catalog-$pass.log"
    grep -q '^idx_lugares_pos|gist|t|' "$logs/catalog-$pass.log"
done
