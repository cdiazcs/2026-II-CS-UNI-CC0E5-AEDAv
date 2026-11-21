# PC4 — Excel Texto (IA)

Implementacion independiente en C++23 sobre la rama `pc4-texto`, sin modificar los contenedores compartidos.

## Funcionalidades
- **Funciones basicas:** `SUMA`/`SUM`, `PROMEDIO`/`AVERAGE`, `MIN`, `MAX`, `CONCAT`/`CONCATENAR`, `LARGO`/`LEN`, `MAYUSC`/`UPPER`, `MINUSC`/`LOWER`.
- **Parentesis:** expresiones anidadas, prioridad `* /` sobre `+ -`, operadores unarios, concatenacion `&`.
- **Celdas y rangos:** referencias `A1`, `B2`, rango `A1:A5` dentro de funciones; recalculo dinamico y deteccion de ciclos. Celda inexistente equivale a 0.
- **Persistencia:** `SAVE` y `LOAD` con serializacion versionada que conserva el contenido original de cada celda, incluidas formulas y comillas. La carga valida antes de reemplazar la hoja.
- **Indices SQL:** carpeta `sql/` con los ejercicios B-Tree, Hash y GiST adaptados del repositorio AEDA anterior. **Estos archivos son ejercicios PostgreSQL independientes; no constituyen indices internos de la hoja C++.**

## Compilar, probar y ejecutar

```bash
cd pc4_texto
make test
make
./texto
```

Ejemplo interactivo:

```text
SET A1 10
SET A2 20
SET B1 =SUMA(A1:A2)
GET B1
SET C1 =(A1+2)*(A2-5)
GET C1
SET D1 =CONCAT("Hola ","UNI")
GET D1
SAVE hoja.txt
LOAD hoja.txt
EXIT
```

Los scripts SQL requieren PostgreSQL y se ejecutan por separado con `psql -d mibd -f pc4_texto/sql/btree_index.sql` (analogamente `hash_index.sql`, `gist_index.sql`) desde la raiz del repo.

## Limites documentados
No implementa importacion/exportacion nativa `.xlsx` ni interfaz grafica ni interprete de IA. `LARGO` cuenta bytes (no caracteres Unicode); `MAYUSC` y `MINUSC` aplican conversion ASCII/locale C. Los indices SQL no estan conectados al modulo en memoria. El modulo constituye una base probada para integrar posteriormente una hoja Excel/IA real segun la rubrica completa.
