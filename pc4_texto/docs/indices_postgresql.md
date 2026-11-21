# Índices PostgreSQL de PC4 Texto

Estos ejercicios muestran tres métodos de acceso con consultas selectivas y consultas ordenadas. Los scripts operan sobre el esquema `pc4_texto_demo` de una base PostgreSQL. El motor de fórmulas de PC4 Texto usa contenedores de C++ y funciona independientemente de estos ejercicios. En particular, el índice Hash de PostgreSQL usa buckets para localizar claves por igualdad. El `HashTable` del repositorio usa un árbol AVL.

## Reproducir la validación

Desde la raíz del repositorio, con Bash, los binarios de servidor de PostgreSQL 18 y un usuario no root:

```bash
bash pc4_texto/tests/test_sql_indexes.sh
```

El verificador usa la instalación habitual de PostgreSQL 18 o la ruta especificada mediante `PG18_BIN`. Crea un clúster y una base desechables, con un socket Unix en un directorio privado y TCP deshabilitado. Ejecuta los tres scripts dos veces con `psql -X -w -v ON_ERROR_STOP=1`. Al salir, detiene y elimina su clúster. Los registros quedan en el directorio indicado por `Evidencia`. El entorno debe permitir iniciar un servidor local y abrir un socket Unix.

Cada script reconstruye su tabla dentro de `pc4_texto_demo` mediante un `DROP TABLE IF EXISTS` calificado y la creación posterior en una transacción. Usar una base desechable o un esquema reservado para este ejercicio. El verificador coloca tablas homónimas con la fila `intacto` en `pc4_sentinel` y lo antepone al esquema público en el contexto inicial. Las tres filas centinela sobrevivieron a ambas pasadas. B-tree y GiST fijan la semilla de sus cargas aleatorias. Hash genera tokens mediante `md5('sesion-' || g)`. Los scripts ejecutan `ANALYZE` para actualizar las estadísticas del planificador.

## Versión y catálogo verificados

El servidor reportó **PostgreSQL 18.6**. Ambas pasadas devolvieron estos conteos y `pg_index.indisvalid = true` para los índices secundarios.

| Tabla en `pc4_texto_demo` | Filas | Índice secundario | Método y columna |
|---|---:|---|---|
| `empleados` | 100 000 | `idx_emp_salario` | B-tree en `salario` |
| `sesiones` | 100 000 | `idx_ses_token` | Hash en `token` |
| `lugares` | 50 000 | `idx_lugares_pos` | GiST en `pos` de tipo `POINT` |

Evidencia de [versión](evidencias_sql/version.text), [catálogo de la primera pasada](evidencias_sql/catalog-1.text) y [catálogo de la segunda pasada](evidencias_sql/catalog-2.text). Cada tabla ya tenía un índice de clave primaria sobre `id`. «Antes» se refiere al momento anterior a crear el índice secundario de la demostración.

## Consultas y planes naturales

Cada consulta se ejecutó con `EXPLAIN (ANALYZE, BUFFERS)`. La tabla resume la primera pasada. **H/L** indica bloques compartidos encontrados en caché (`hit`) y leídos (`read`) en el nodo superior de ejecución. Un cero indica que el plan omitió ese contador. La segunda pasada eligió los mismos tipos de escaneo y devolvió los mismos conteos de filas.

| Consulta | Momento | Plan observado e índice | Filas | H/L | Tiempo de ejecución |
|---|---|---|---:|---:|---:|
| B-tree, `salario BETWEEN 8000 AND 8050` | Antes | `Seq Scan` | 552 | 637/0 | 8.054 ms |
| B-tree, mismo intervalo | Después | `Bitmap Heap Scan` con `Bitmap Index Scan` de `idx_emp_salario` | 552 | 382/2 | 0.489 ms |
| B-tree, `ORDER BY salario LIMIT 10` | Después | `Limit` sobre `Index Scan` de `idx_emp_salario` | 10 | 11/1 | 0.049 ms |
| Hash, `token = md5('sesion-50000')` | Antes | `Seq Scan` | 1 | 834/0 | 7.090 ms |
| Hash, misma igualdad | Después | `Index Scan` de `idx_ses_token` | 1 | 3/0 | 0.021 ms |
| Hash, `token > md5('sesion-50000')` | Control | `Seq Scan` | 85 778 | 834/0 | 13.970 ms |
| GiST, `pos <@ box(point(0,0), point(50,50))` | Antes | `Seq Scan` | 108 | 368/0 | 3.071 ms |
| GiST, misma contención | Después | `Bitmap Heap Scan` con `Bitmap Index Scan` de `idx_lugares_pos` | 108 | 95/8 | 0.264 ms |
| GiST, `ORDER BY pos <-> point(500,500) LIMIT 5` | Después | `Limit` sobre `Index Scan` de `idx_lugares_pos`, ordenado por distancia | 5 | 6/4 | 0.855 ms |

- **B-tree.** El orden de las claves permite filtrar por intervalo y recorrer salarios en orden. En el segundo plan del intervalo, `Index Cond` muestra los límites inferior y superior. En la consulta ordenada, el escaneo entrega diez filas sin un nodo `Sort` aparte. [SQL](../sql/btree_index.sql) · [Pasada 1](evidencias_sql/btree-1.text) · [Pasada 2](evidencias_sql/btree-2.text)
- **Hash.** La condición de índice es la igualdad sobre `token`. El operador `>` requiere orden de claves, que el método Hash no ofrece. El control recorrió secuencialmente la tabla y filtró las filas. [SQL](../sql/hash_index.sql) · [Pasada 1](evidencias_sql/hash-1.text) · [Pasada 2](evidencias_sql/hash-2.text)
- **GiST.** La clase de operadores `point_ops` admite `<@` para ubicar puntos dentro de una caja. `<->` ordena por distancia para encontrar los cinco vecinos más cercanos (KNN). El plan muestra `Order By` dentro del escaneo del índice. [SQL](../sql/gist_index.sql) · [Pasada 1](evidencias_sql/gist-1.text) · [Pasada 2](evidencias_sql/gist-2.text)

## Interpretar la evidencia

`Seq Scan` recorre la tabla y aplica el filtro. Puede ser la opción de menor costo si se necesitan muchas filas. `Index Scan` usa un índice para localizar filas o entregarlas en el orden solicitado. `Bitmap Index Scan` construye un mapa de ubicaciones candidatas y `Bitmap Heap Scan` visita los bloques de la tabla. `Index Cond` es la condición utilizada por el acceso al índice. `Filter` se evalúa sobre filas recuperadas y `Recheck Cond` identifica la condición que puede volver a verificarse al visitar la tabla.

`EXPLAIN ANALYZE` ejecuta la consulta y registra filas y tiempos reales. `BUFFERS` informa actividad de bloques. Los contadores de bloques de planificación se muestran por separado y un bloque leído puede provenir de la caché del sistema operativo. `Execution Time` es el tiempo de ejecución informado por PostgreSQL, separado de `Planning Time`. Los tiempos dependen de selectividad, caché, hardware y estadísticas. La creación o validez de un índice se verifica en el catálogo. Su uso se verifica en el plan concreto.

Los planes archivados son **naturales**. La validación no cambió `enable_seqscan` ni forzó rutas de acceso. Un plan obtenido con esa configuración alterada sería un diagnóstico separado. Los archivos conservan las dos pasadas originales, con rutas absolutas de avisos sustituidas por rutas relativas y espacios finales retirados. No se archivaron los registros de inicialización o del servidor.
