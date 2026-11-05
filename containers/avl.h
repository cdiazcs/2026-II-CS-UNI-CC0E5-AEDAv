
#ifndef __AVL_H__
#define __AVL_H__

#include "binarytree.h"

template <typename T>
class AVLNode : public BinaryTreeNode<T> {
public:
    using Parent = BinaryTreeNode<T>;
    using Parent::Parent;

private:
    TX m_height = 1;

public:
    TX height() const {
        return m_height;
    }

    void setHeight(TX height) {
        m_height = height;
    }
};

template <typename T>
struct AVLTreeTraits {
    using Node = AVLNode<T>;

    using ForwardIterator =
        BinaryTreeIterator<Node, TreeOrder::InOrder, 0>;

    using BackwardIterator =
        BinaryTreeIterator<Node, TreeOrder::InOrder, 1>;
};

template <typename T>
struct AVLAscTraits
    : public AscendingTraits<T>,
      public AVLTreeTraits<T> {};

template <typename T>
struct AVLDescTraits
    : public DescendingTraits<T>,
      public AVLTreeTraits<T> {};

//
template <typename Traits, template <typename> typename... Mixins>
class BaseAVL : public BaseBinaryTree<Traits, Mixins...> {
public:
    using Parent = BaseBinaryTree<Traits, Mixins...>;
    using Parent::Parent;

    using LinkPtr = typename Parent::LinkPtr;
    using Node = typename Parent::Node;

private:
   
    static TX altura(LinkPtr nodo) {
        return nodo
            ? static_cast<Node*>(nodo)->height()
            : TX(0);
    }

    
    static void actualizar(LinkPtr nodo) {
        if (!nodo)
            return;

        static_cast<Node*>(nodo)->setHeight(
            TX(1) + std::max(
                altura(nodo->m_pChild[0]),
                altura(nodo->m_pChild[1])
            )
        );
    }

    
    static TX equilibrio(LinkPtr nodo) {
        if (!nodo)
            return 0;

        return altura(nodo->m_pChild[0])
             - altura(nodo->m_pChild[1]);
    }

    
    LinkPtr rotar(LinkPtr raiz, TX dir) {
        LinkPtr nueva = raiz->m_pChild[dir];
        LinkPtr padre = raiz->m_parent;

        
        raiz->m_pChild[dir] =
            nueva->m_pChild[1 - dir];

        if (raiz->m_pChild[dir])
            raiz->m_pChild[dir]->m_parent = raiz;

        
        nueva->m_parent = padre;

        if (!padre)
            this->m_root = nueva;
        else if (padre->m_pChild[0] == raiz)
            padre->m_pChild[0] = nueva;
        else
            padre->m_pChild[1] = nueva;

        nueva->m_pChild[1 - dir] = raiz;
        raiz->m_parent = nueva;

        
        actualizar(raiz);
        actualizar(nueva);

        return nueva;
    }

   
    void rebalancear(LinkPtr nodo) {
        while (nodo) {
            actualizar(nodo);

            TX fe = equilibrio(nodo);

            
            if (fe > 1) {
                if (equilibrio(nodo->m_pChild[0]) < 0)
                    rotar(nodo->m_pChild[0], 1);

                nodo = rotar(nodo, 0);
            }

            
            else if (fe < -1) {
                if (equilibrio(nodo->m_pChild[1]) > 0)
                    rotar(nodo->m_pChild[1], 0);

                nodo = rotar(nodo, 1);
            }

            nodo = nodo->m_parent;
        }
    }
    // 
    
public:
    Ref* findRef(const typename Parent::value_type& clave) {
        std::lock_guard lock(this->m_mutex);

        typename Parent::Compare cmp;
        LinkPtr nodo = this->m_root;

        while (nodo) {
            if (cmp(nodo->getValue(), clave)) {
                nodo = nodo->m_pChild[1];
            }
            else if (cmp(clave, nodo->getValue())) {
                nodo = nodo->m_pChild[0];
            }
            else {
                return &nodo->ref();
            }
        }

        return nullptr;
    }


protected:
    
    void afterInsert(LinkPtr nodo) override {
        rebalancear(nodo);
    }

    void afterRemove(LinkPtr nodo) override {
        rebalancear(nodo);
    }
};

template <typename Traits>
using AVLTree = BaseAVL<
    Traits,
    Callable,
    Applicable,
    Searchable,
    ReverseCallable,
    Printable,
    Readable
>;


#endif
