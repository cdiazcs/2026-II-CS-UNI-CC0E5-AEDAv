-- ============================================================
-- PC4 - Indice Espacial R-Tree (via GiST) en PostgreSQL
-- Ejecutar:  psql -X -v ON_ERROR_STOP=1 -d <bd_de_pruebas> -f pc4_texto/sql/gist_index.sql
--
-- GiST es el método de acceso. Para POINT, point_ops admite <@ (point, box)
-- para contención y <-> (point, point) para ordenamiento KNN.
-- Los operadores de otras clases geométricas no se aplican aquí.
-- ============================================================

BEGIN;
-- Usar solo en una base desechable o con este esquema reservado para la demo.
CREATE SCHEMA IF NOT EXISTS pc4_texto_demo;
SET LOCAL search_path = pg_catalog, pc4_texto_demo;

DROP TABLE IF EXISTS pc4_texto_demo.lugares;
CREATE TABLE pc4_texto_demo.lugares (
    id      SERIAL PRIMARY KEY,
    nombre  VARCHAR(50) NOT NULL,
    pos     POINT       NOT NULL
);

-- 50 000 puntos aleatorios en un plano 0..1000
SELECT setseed(0.42);
INSERT INTO pc4_texto_demo.lugares (nombre, pos)
SELECT 'p_' || g,
       point((random()*1000)::int, (random()*1000)::int)
FROM generate_series(1, 50000) AS g;

ANALYZE pc4_texto_demo.lugares;

-- ------------------------------------------------------------
-- (1) SIN indice -> Seq Scan sobre los 50 000 puntos
-- ------------------------------------------------------------
EXPLAIN (ANALYZE, BUFFERS)
SELECT * FROM pc4_texto_demo.lugares WHERE pos <@ box(point(0,0), point(50,50));

-- Usamos GiST con la clase por defecto point_ops.
CREATE INDEX idx_lugares_pos ON pc4_texto_demo.lugares USING gist (pos);
ANALYZE pc4_texto_demo.lugares;

-- ------------------------------------------------------------
-- (2) Contencion: puntos DENTRO de un area -> Index Scan (GiST)
-- ------------------------------------------------------------
EXPLAIN (ANALYZE, BUFFERS)
SELECT * FROM pc4_texto_demo.lugares WHERE pos <@ box(point(0,0), point(50,50));

-- ------------------------------------------------------------
-- (3) KNN: los 5 puntos MAS CERCANOS a (500,500)
--     GiST devuelve ya ordenado por <-> (vecino mas cercano)
-- ------------------------------------------------------------
EXPLAIN (ANALYZE, BUFFERS)
SELECT nombre, pos, pos <-> point(500,500) AS dist
FROM pc4_texto_demo.lugares
ORDER BY pos <-> point(500,500)
LIMIT 5;

SELECT indexname, indexdef FROM pg_indexes WHERE schemaname = 'pc4_texto_demo' AND tablename = 'lugares';

-- Para datos geograficos reales (GPS, lat/long, poligonos) se usa la
-- extension PostGIS (tipo geometry/geography), que tambien se apoya en GiST.

COMMIT;
