# PC4 — Excel Texto

Hoja de cálculo por consola hecha en C++23. Cada celda (`A1`, `B2`, ...) guarda un número, un texto o una fórmula que empieza con `=`, y se maneja con comandos como `SET A1 10`, `GET A1`, `SAVE hoja.txt` y `LOAD hoja.txt`.

**Lo que implementamos**

| Criterio | Qué soporta | Ejemplo |
|---|---|---|
| Funciones básicas | `SUMA`, `PROMEDIO`, `MIN`, `MAX`, `CONTAR`, `CONCAT`, `LARGO`, `MAYUSC`, `MINUSC` (también en inglés: `SUM`, `AVERAGE`, ...), con rangos `A1:B3` | `=PROMEDIO(A1:A3)` → 15 |
| Soporta paréntesis | Paréntesis anidados, prioridad de `* /` sobre `+ -`, signos `-(...)` y `&` para unir textos | `=((1+2)*(3+4))/7` → 3 |
| Persistencia | `SAVE` / `LOAD` usando el `operator<<` / `operator>>` de nuestro `HashTable`; si el archivo está corrupto, la hoja no cambia | `SAVE hoja.txt` → `LOAD hoja.txt` |

**Cómo está hecho**
- Las celdas se guardan en nuestro `HashTable` (por dentro es un AVL), así que cada acceso es O(log n).
- Las fórmulas se evalúan con un parser recursivo descendente: `expr` → `term` → `unary` → `primary`.
- Las referencias circulares se detectan como en un DFS (cada celda está no visitada, activa o completada).

**Probar y ejecutar**

```bash
cd pc4_texto
make test      # 109 pruebas, resumen por grupo
make && ./texto
```



