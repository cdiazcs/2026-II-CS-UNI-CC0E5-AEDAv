-- ============================================================
-- PC4 - Indice Hash en PostgreSQL
-- Ejecutar:  psql -X -v ON_ERROR_STOP=1 -d <bd_de_pruebas> -f pc4_texto/sql/hash_index.sql
--
-- Aplica una funcion hash a la clave y reparte las filas en buckets
-- -> busqueda por igualdad O(1) promedio.
-- Sirve SOLO para "=": no soporta rangos (<, >) ni ORDER BY.
-- Este índice usa buckets. El HashTable de C++ del repositorio usa un AVL.
-- Nota: desde PostgreSQL 10 el indice hash es crash-safe / replicado por WAL.
-- ============================================================

BEGIN;
-- Usar solo en una base desechable o con este esquema reservado para la demo.
CREATE SCHEMA IF NOT EXISTS pc4_texto_demo;
SET LOCAL search_path = pg_catalog, pc4_texto_demo;

DROP TABLE IF EXISTS pc4_texto_demo.sesiones;
CREATE TABLE pc4_texto_demo.sesiones (
    id      SERIAL PRIMARY KEY,
    token   TEXT     NOT NULL,      -- token tipo MD5 (32 hex); TEXT para que
                                    -- "= md5(...)" (text) use la opclass del hash
    activa  BOOLEAN  DEFAULT TRUE
);

-- 100 000 tokens. Buscaremos md5('sesion-50000').
INSERT INTO pc4_texto_demo.sesiones (token)
SELECT md5('sesion-' || g)
FROM generate_series(1, 100000) AS g;

ANALYZE pc4_texto_demo.sesiones;

-- ------------------------------------------------------------
-- (1) SIN indice -> Seq Scan sobre las 100 000 filas
-- ------------------------------------------------------------
EXPLAIN (ANALYZE, BUFFERS)
SELECT * FROM pc4_texto_demo.sesiones WHERE token = md5('sesion-50000');

-- Hash NO es el defecto: hay que pedirlo con "USING hash"
CREATE INDEX idx_ses_token ON pc4_texto_demo.sesiones USING hash (token);
ANALYZE pc4_texto_demo.sesiones;

-- ------------------------------------------------------------
-- (2) CON indice hash -> Index Scan por igualdad exacta
-- ------------------------------------------------------------
EXPLAIN (ANALYZE, BUFFERS)
SELECT * FROM pc4_texto_demo.sesiones WHERE token = md5('sesion-50000');

-- ------------------------------------------------------------
-- (3) El hash NO sirve para rangos: esto IGNORA el indice (Seq Scan)
-- ------------------------------------------------------------
EXPLAIN (ANALYZE, BUFFERS)
SELECT * FROM pc4_texto_demo.sesiones WHERE token > md5('sesion-50000');

SELECT indexname, indexdef FROM pg_indexes WHERE schemaname = 'pc4_texto_demo' AND tablename = 'sesiones';

COMMIT;
