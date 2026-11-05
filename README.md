
# Arquitectura de BinaryTree, AVLTree y AVLMap

**2026-II-CS-UNI-CC0E5-AEDAv**

## I. Arquitectura del sistema

### 1. Vista general de clases y subsistemas

```mermaid
flowchart LR
    subgraph CONFIG["1. CONFIGURACIÓN"]
        direction TB
        TR["Traits<br/>BinaryTreeAsc / Desc<br/>AVLAsc / Desc"]
        MX["Mixins<br/>Callable · Applicable<br/>Searchable · ReverseCallable<br/>Printable · Readable"]
    end

    subgraph NODOS["2. JERARQUÍA DE NODOS"]
        direction TB
        GN["GeneralNode<br/>value · ref"]
        BN["BinaryTreeNode<br/>m_pChild · m_parent"]
        AN["AVLNode<br/>m_height"]

        GN -->|"hereda"| BN
        BN -->|"hereda"| AN
    end

    subgraph ARBOLES["3. CONTENEDORES Y ÁRBOLES"]
        direction TB
        GC["GeneralContainer"]
        BT["BaseBinaryTree<br/>insert · remove<br/>size · height · clear"]
        AVL["BaseAVL<br/>rotar · rebalancear<br/>findRef"]

        GC -->|"hereda"| BT
        BT -->|"hereda"| AVL
    end

    subgraph SOPORTE["4. RECORRIDOS Y VISUALIZACIÓN"]
        direction TB
        GI["GeneralIterator"]
        IT["BinaryTreeIterator<br/>6 recorridos en 1"]
        TT["TreeTraversal<br/>begin · end"]
        TP["TreePrinter<br/>print · printSide"]

        GI -->|"hereda"| IT
        TT -->|"utiliza"| IT
    end

    subgraph MAPA["5. MAPA AVL"]
        MAP["AVLMap<br/>operator[] · operator<<br/>operator<< · operator>>"]
    end

    TR -.->|"configura"| BT
    TR -.->|"configura"| AVL
    MX -.->|"capacidades"| GC

    BT -.->|"nodos"| BN
    AVL -.->|"nodos"| AN
    BT -.->|"recorridos"| TT
    BT -.->|"visualización"| TP

    MAP -->|"contiene AVLTree"| AVL

    classDef config fill:#fff0d8,stroke:#d69a40,color:#3b2d17
    classDef node fill:#f8e4ef,stroke:#c66b9e,color:#532543
    classDef tree fill:#e4edff,stroke:#5775c9,color:#24365f
    classDef support fill:#dff5eb,stroke:#4f9a7a,color:#1e4939
    classDef map fill:#ede5ff,stroke:#8d70cb,color:#3b285f

    class TR,MX config
    class GN,BN,AN node
    class GC,BT,AVL tree
    class GI,IT,TT,TP support
    class MAP map
```

### 2. Jerarquía de clases

<details>
<summary><strong>Ver jerarquía de nodos y contenedores</strong></summary>

```mermaid
classDiagram
    direction LR

    class GeneralNode {
        -m_value
        -m_ref
        +getValue()
        +getRef()
        +value()
        +ref()
    }

    class BinaryTreeNode {
        +m_pChild[2]
        +m_parent
    }

    class AVLNode {
        -m_height
        +height()
        +setHeight()
    }

    class ContainerCore {
        #m_mutex
        +begin()
        +end()
        +clear()
        +push_back()
    }

    class GeneralContainer

    class BaseBinaryTree {
        #m_root
        +insert()
        +remove()
        +size()
        +height()
        +clear()
        +printTree()
        +printTreeConLineas()
        #internal_insert()
        #afterInsert()
        #afterRemove()
    }

    class BaseAVL {
        -altura()
        -actualizar()
        -equilibrio()
        -rotar()
        -rebalancear()
        +findRef()
        #afterInsert()
        #afterRemove()
    }

    GeneralNode <|-- BinaryTreeNode
    BinaryTreeNode <|-- AVLNode

    ContainerCore <|-- GeneralContainer
    GeneralContainer <|-- BaseBinaryTree
    BaseBinaryTree <|-- BaseAVL
```

**Relaciones principales:**
- `AVLNode` extiende `BinaryTreeNode` con la altura.
- `BaseAVL` extiende `BaseBinaryTree` con rotaciones y rebalanceo.
- `BinaryTree` y `AVLTree` son alias de plantilla que configuran las clases base con sus mixins.

</details>

### 3. Sistema de recorridos y visualización

<details>
<summary><strong>Ver iteradores y TreePrinter</strong></summary>

```mermaid
flowchart TB
    BT["BaseBinaryTree"]

    subgraph REC["RECORRIDOS"]
        direction TB
        TT["TreeTraversal<br/>begin · end"]
        IT["BinaryTreeIterator<br/>Node · Order · Dir"]

        PRE["PreOrder"]
        IN["InOrder"]
        POST["PostOrder"]

        DIR["Dir = 0 / Dir = 1"]

        TT --> IT
        IT --> PRE
        IT --> IN
        IT --> POST

        PRE --> DIR
        IN --> DIR
        POST --> DIR
    end

    subgraph VIS["VISUALIZACIÓN"]
        direction TB
        TP["TreePrinter"]
        DRAW["print()<br/>Árbol vertical"]
        SIDE["printSide()<br/>Árbol lateral"]

        TP --> DRAW
        TP --> SIDE
    end

    BT -->|"recorre"| TT
    BT -->|"delega dibujo"| TP

    classDef tree fill:#e4edff,stroke:#5775c9,color:#24365f
    classDef iterator fill:#dff5eb,stroke:#4f9a7a,color:#1e4939
    classDef printer fill:#fff0d8,stroke:#d69a40,color:#3b2d17

    class BT tree
    class TT,IT,PRE,IN,POST,DIR iterator
    class TP,DRAW,SIDE printer
```

| Orden | Dirección 0 | Dirección 1 |
|---|---|---|
| PreOrder | PreOrder | ReversePreOrder |
| InOrder | InOrder | ReverseInOrder |
| PostOrder | PostOrder | ReversePostOrder |

Los seis recorridos se implementan mediante una única plantilla `BinaryTreeIterator<Node, Order, Dir>`.

</details>

### 4. Arquitectura de AVLMap

<details>
<summary><strong>Ver composición y conexión con AVLTree</strong></summary>

```mermaid
flowchart TB
    MAP["AVLMap&lt;Traits&gt;<br/>operator[] · remove · size<br/>operator<< · operator>>"]

    TREE["AVLTree&lt;Traits&gt;<br/>m_tree: árbol interno"]

    AVL["BaseAVL<br/>rotar · rebalancear<br/>findRef"]

    BT["BaseBinaryTree<br/>insert · remove<br/>size · height"]

    NODE["AVLNode<br/>clave · Ref · altura"]

    MAP -->|"composición"| TREE
    TREE -.->|"alias de"| AVL
    AVL -->|"herencia"| BT
    AVL -.->|"utiliza"| NODE

    classDef map fill:#fff0d8,stroke:#d69a40,color:#3b2d17
    classDef tree fill:#e4edff,stroke:#5775c9,color:#24365f
    classDef node fill:#f8e4ef,stroke:#c66b9e,color:#532543

    class MAP map
    class TREE,AVL,BT tree
    class NODE node
```

**AVLMap** utiliza un único `AVLTree` interno. `operator[]` busca la clave mediante `findRef()` y, si no existe, la inserta para devolver una referencia modificable al dato asociado.

</details>

---

## II. Archivos principales

| Archivo | Componentes |
|---|---|
| `containers/GeneralNode.h` | `GeneralNode` |
| `containers/binarytree.h` | `BinaryTreeNode`, `BinaryTreeIterator`, `TreeTraversal`, `BaseBinaryTree` |
| `containers/avl.h` | `AVLNode`, `BaseAVL`, `AVLTree` |
| `containers/avlmap.h` | `AVLMap` |
| `containers/treeprinter.h` | `TreePrinter` |
| `containers/GeneralIterator.h` | `GeneralIterator` |
| `containers/GeneralContainer.h` | `GeneralContainer` |
| `containers/ContainerCore.h` | `ContainerCore` |
| `containers/mixins.h` | Mixins de operaciones |
| `Demos.cpp` | Demostraciones y pruebas |

---


