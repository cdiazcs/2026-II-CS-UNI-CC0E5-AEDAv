

BEGIN;

CREATE SCHEMA IF NOT EXISTS pc4_texto_demo;
SET LOCAL search_path = pg_catalog, pc4_texto_demo;

DROP TABLE IF EXISTS pc4_texto_demo.sesiones;
CREATE TABLE pc4_texto_demo.sesiones (
    id      SERIAL PRIMARY KEY,
    token   TEXT     NOT NULL,      
                                    
    activa  BOOLEAN  DEFAULT TRUE
);

INSERT INTO pc4_texto_demo.sesiones (token)
SELECT md5('sesion-' || g)
FROM generate_series(1, 100000) AS g;

ANALYZE pc4_texto_demo.sesiones;


EXPLAIN (ANALYZE, BUFFERS)
SELECT * FROM pc4_texto_demo.sesiones WHERE token = md5('sesion-50000');

-- Hash NO es el defecto: hay que pedirlo con "USING hash"
CREATE INDEX idx_ses_token ON pc4_texto_demo.sesiones USING hash (token);
ANALYZE pc4_texto_demo.sesiones;


EXPLAIN (ANALYZE, BUFFERS)
SELECT * FROM pc4_texto_demo.sesiones WHERE token = md5('sesion-50000');


EXPLAIN (ANALYZE, BUFFERS)
SELECT * FROM pc4_texto_demo.sesiones WHERE token > md5('sesion-50000');

SELECT indexname, indexdef FROM pg_indexes WHERE schemaname = 'pc4_texto_demo' AND tablename = 'sesiones';

COMMIT;
