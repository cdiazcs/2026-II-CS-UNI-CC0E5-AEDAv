
BEGIN;

CREATE SCHEMA IF NOT EXISTS pc4_texto_demo;
SET LOCAL search_path = pg_catalog, pc4_texto_demo;

DROP TABLE IF EXISTS pc4_texto_demo.lugares;
CREATE TABLE pc4_texto_demo.lugares (
    id      SERIAL PRIMARY KEY,
    nombre  VARCHAR(50) NOT NULL,
    pos     POINT       NOT NULL
);

SELECT setseed(0.42);
INSERT INTO pc4_texto_demo.lugares (nombre, pos)
SELECT 'p_' || g,
       point((random()*1000)::int, (random()*1000)::int)
FROM generate_series(1, 50000) AS g;

ANALYZE pc4_texto_demo.lugares;

EXPLAIN (ANALYZE, BUFFERS)
SELECT * FROM pc4_texto_demo.lugares WHERE pos <@ box(point(0,0), point(50,50));


CREATE INDEX idx_lugares_pos ON pc4_texto_demo.lugares USING gist (pos);
ANALYZE pc4_texto_demo.lugares;


EXPLAIN (ANALYZE, BUFFERS)
SELECT * FROM pc4_texto_demo.lugares WHERE pos <@ box(point(0,0), point(50,50));


EXPLAIN (ANALYZE, BUFFERS)
SELECT nombre, pos, pos <-> point(500,500) AS dist
FROM pc4_texto_demo.lugares
ORDER BY pos <-> point(500,500)
LIMIT 5;

SELECT indexname, indexdef FROM pg_indexes WHERE schemaname = 'pc4_texto_demo' AND tablename = 'lugares';



COMMIT;
