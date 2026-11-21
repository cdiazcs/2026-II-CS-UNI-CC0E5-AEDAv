

BEGIN;

CREATE SCHEMA IF NOT EXISTS pc4_texto_demo;
SET LOCAL search_path = pg_catalog, pc4_texto_demo;

DROP TABLE IF EXISTS pc4_texto_demo.empleados;
CREATE TABLE pc4_texto_demo.empleados (
    id        SERIAL PRIMARY KEY,
    nombre    VARCHAR(50) NOT NULL,
    salario   INTEGER     NOT NULL,
    ingreso   DATE        NOT NULL
);


SELECT setseed(0.42);
INSERT INTO pc4_texto_demo.empleados (nombre, salario, ingreso)
SELECT 'emp_' || g,
       1000 + (random() * 9000)::int,
       DATE '2015-01-01' + (random() * 3650)::int
FROM generate_series(1, 100000) AS g;

ANALYZE pc4_texto_demo.empleados;


EXPLAIN (ANALYZE, BUFFERS)
SELECT * FROM pc4_texto_demo.empleados WHERE salario BETWEEN 8000 AND 8050;


CREATE INDEX idx_emp_salario ON pc4_texto_demo.empleados USING btree (salario);
ANALYZE pc4_texto_demo.empleados;


EXPLAIN (ANALYZE, BUFFERS)
SELECT * FROM pc4_texto_demo.empleados WHERE salario BETWEEN 8000 AND 8050;


EXPLAIN (ANALYZE, BUFFERS)
SELECT id, nombre, salario FROM pc4_texto_demo.empleados ORDER BY salario LIMIT 10;

SELECT indexname, indexdef FROM pg_indexes WHERE schemaname = 'pc4_texto_demo' AND tablename = 'empleados';

COMMIT;
