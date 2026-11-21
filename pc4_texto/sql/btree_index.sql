-- ============================================================
-- PC4 - Indice B-Tree en PostgreSQL
-- Ejecutar:  psql -X -v ON_ERROR_STOP=1 -d <bd_de_pruebas> -f pc4_texto/sql/btree_index.sql
--
-- B-Tree = indice por defecto. Mantiene las claves ordenadas en
-- un arbol balanceado -> busqueda O(log n).
-- Sirve para: =, <, <=, >=, >, BETWEEN, IN, IS NULL y ORDER BY.
-- (Es la misma estructura del CBTreePage del EF-P1: split/merge de paginas.)
-- ============================================================

BEGIN;
-- Usar solo en una base desechable o con este esquema reservado para la demo.
CREATE SCHEMA IF NOT EXISTS pc4_texto_demo;
SET LOCAL search_path = pg_catalog, pc4_texto_demo;

DROP TABLE IF EXISTS pc4_texto_demo.empleados;
CREATE TABLE pc4_texto_demo.empleados (
    id        SERIAL PRIMARY KEY,
    nombre    VARCHAR(50) NOT NULL,
    salario   INTEGER     NOT NULL,
    ingreso   DATE        NOT NULL
);

-- Volumen real: 100 000 filas.
-- Sin volumen el planner prefiere Seq Scan y el indice "no se nota".
SELECT setseed(0.42);
INSERT INTO pc4_texto_demo.empleados (nombre, salario, ingreso)
SELECT 'emp_' || g,
       1000 + (random() * 9000)::int,
       DATE '2015-01-01' + (random() * 3650)::int
FROM generate_series(1, 100000) AS g;

ANALYZE pc4_texto_demo.empleados;

-- ------------------------------------------------------------
-- (1) SIN indice -> Seq Scan: recorre las 100 000 filas
-- ------------------------------------------------------------
EXPLAIN (ANALYZE, BUFFERS)
SELECT * FROM pc4_texto_demo.empleados WHERE salario BETWEEN 8000 AND 8050;

-- Creamos el B-Tree ("USING btree" es opcional: es el tipo por defecto)
CREATE INDEX idx_emp_salario ON pc4_texto_demo.empleados USING btree (salario);
ANALYZE pc4_texto_demo.empleados;

-- ------------------------------------------------------------
-- (2) CON indice -> Index Scan / Bitmap Index Scan en el rango
-- ------------------------------------------------------------
EXPLAIN (ANALYZE, BUFFERS)
SELECT * FROM pc4_texto_demo.empleados WHERE salario BETWEEN 8000 AND 8050;

-- ------------------------------------------------------------
-- (3) ORDER BY: el orden del B-Tree evita el paso de Sort
-- ------------------------------------------------------------
EXPLAIN (ANALYZE, BUFFERS)
SELECT id, nombre, salario FROM pc4_texto_demo.empleados ORDER BY salario LIMIT 10;

-- Indices existentes sobre la tabla
SELECT indexname, indexdef FROM pg_indexes WHERE schemaname = 'pc4_texto_demo' AND tablename = 'empleados';

COMMIT;
