
#ifndef __AVLMAP_H__
#define __AVLMAP_H__

#include "avl.h"


template <typename Traits>
class AVLMap {
public:
    using value_type = typename Traits::value_type;
    using Tree = AVLTree<Traits>;

private:
    Tree m_tree;

public:
    
    Ref& operator[](const value_type& clave) {
        Ref* dato = m_tree.findRef(clave);

       
        if (!dato) {
            m_tree.insert(clave, Ref{});
            dato = m_tree.findRef(clave);
        }

        return *dato;
    }

   
    TX size() const {
        return m_tree.size();
    }

    TX height() const {
        return m_tree.height();
    }

    bool remove(const value_type& clave) {
        return m_tree.remove(clave);
    }

    void clear() {
        m_tree.clear();
    }

   
    void printTreeConLineas(
        std::ostream& os = std::cout
    ) const {
        m_tree.printTreeConLineas(os);
    }

   
    friend std::ostream& operator<<(
        std::ostream& os,
        const AVLMap& mapa
    ) {
        return os << mapa.m_tree;
    }

   
    friend std::istream& operator>>(
        std::istream& is,
        AVLMap& mapa
    ) {
        return is >> mapa.m_tree;
    }
};

#endif
