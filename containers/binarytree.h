#ifndef __BINARYTREE_H__
#define __BINARYTREE_H__
#include "GeneralNode.h"
#include "GeneralIterator.h"
#include "GeneralContainer.h"
#include "mixins.h"
#include <algorithm>
#include "treeprinter.h"

template <typename T>
class BinaryTreeNode : public GeneralNode<T> {
public:
    using value_type        = T;
    using MySelf            = BinaryTreeNode<T>;
    using Node              = MySelf;
    using NodePtr           = Node*;
    using Parent            = GeneralNode<T>;
    using Parent::Parent; // Inherit constructor

    NodePtr m_pChild[2] = {nullptr, nullptr}; // left and right children
    NodePtr m_parent = nullptr;
    virtual ~BinaryTreeNode() = default;

};


enum class TreeOrder : TX {
    PreOrder,
    InOrder,
    PostOrder
};

template <typename NodeType, TreeOrder Order, TX Dir>
class BinaryTreeIterator
    : public GeneralIterator<
        BinaryTreeIterator<NodeType, Order, Dir>,
        NodeType
    > {
public:
    using Node = NodeType;
    using MySelf = BinaryTreeIterator<NodeType, Order, Dir>;
    using Parent = GeneralIterator<MySelf, Node>;
    using NodePtr = Node*;
    using LinkPtr = BinaryTreeNode<typename Node::value_type>*;
    using Parent::Parent;

private:
    
    static LinkPtr deepestPost(LinkPtr nodo) {
        while (nodo &&
               (nodo->m_pChild[Dir] ||
                nodo->m_pChild[1 - Dir])) {

            if (nodo->m_pChild[Dir])
                nodo = nodo->m_pChild[Dir];
            else
                nodo = nodo->m_pChild[1 - Dir];
        }

        return nodo;
    }

  
    static LinkPtr nextPre(LinkPtr nodo) {
        if (nodo->m_pChild[Dir])
            return nodo->m_pChild[Dir];

        if (nodo->m_pChild[1 - Dir])
            return nodo->m_pChild[1 - Dir];

        while (nodo->m_parent) {
            LinkPtr padre = nodo->m_parent;

            if (nodo == padre->m_pChild[Dir] &&
                padre->m_pChild[1 - Dir]) {
                return padre->m_pChild[1 - Dir];
            }

            nodo = padre;
        }

        return nullptr;
    }

   
    static LinkPtr nextIn(LinkPtr nodo) {
        if (nodo->m_pChild[1 - Dir]) {
            nodo = nodo->m_pChild[1 - Dir];

            while (nodo->m_pChild[Dir])
                nodo = nodo->m_pChild[Dir];

            return nodo;
        }

        LinkPtr padre = nodo->m_parent;

        while (padre &&
               nodo == padre->m_pChild[1 - Dir]) {
            nodo = padre;
            padre = padre->m_parent;
        }

        return padre;
    }

    static LinkPtr nextPost(LinkPtr nodo) {
        LinkPtr padre = nodo->m_parent;

        if (!padre)
            return nullptr;

        if (nodo == padre->m_pChild[Dir] &&
            padre->m_pChild[1 - Dir]) {
            return deepestPost(padre->m_pChild[1 - Dir]);
        }

        return padre;
    }

public:
   
    static NodePtr first(LinkPtr raiz) {
        if (!raiz)
            return nullptr;

        switch (Order) {
            case TreeOrder::PreOrder:
                return static_cast<NodePtr>(raiz);

            case TreeOrder::InOrder:
                while (raiz->m_pChild[Dir])
                    raiz = raiz->m_pChild[Dir];

                return static_cast<NodePtr>(raiz);

            case TreeOrder::PostOrder:
                return static_cast<NodePtr>(
                    deepestPost(raiz)
                );
        }

        return nullptr;
    }

    // un solo operator para los recorridos
    MySelf& operator++() {
        LinkPtr nodo = Parent::m_ptr;

        if (!nodo)
            return *this;

        switch (Order) {
            case TreeOrder::PreOrder:
                Parent::m_ptr =
                    static_cast<NodePtr>(nextPre(nodo));
                break;

            case TreeOrder::InOrder:
                Parent::m_ptr =
                    static_cast<NodePtr>(nextIn(nodo));
                break;

            case TreeOrder::PostOrder:
                Parent::m_ptr =
                    static_cast<NodePtr>(nextPost(nodo));
                break;
        }

        return *this;
    }
};


template <typename T>
using BinaryTreeForwardIterator =
    BinaryTreeIterator<
        BinaryTreeNode<T>, TreeOrder::InOrder, 0
    >;

template <typename T>
using BinaryTreeBackwardIterator =
    BinaryTreeIterator<
        BinaryTreeNode<T>, TreeOrder::InOrder, 1
    >;

//
template <typename Iterator>
class TreeTraversal {
    using LinkPtr = typename Iterator::LinkPtr;

    LinkPtr m_root;

public:
    explicit TreeTraversal(LinkPtr raiz)
        : m_root(raiz) {}

    Iterator begin() const {
        return Iterator(Iterator::first(m_root));
    }

    Iterator end() const {
        return Iterator(nullptr);
    }
};


template <typename T>
struct BinaryTreeTraits {
    using Node              = BinaryTreeNode<T>;
    using ForwardIterator   = BinaryTreeForwardIterator<T>;
    using BackwardIterator  = BinaryTreeBackwardIterator<T>;
};

template <typename T>
struct BinaryTreeAscTraits : public AscendingTraits<T>, public BinaryTreeTraits<T> {};

template <typename T>
struct BinaryTreeDescTraits : public DescendingTraits<T>, public BinaryTreeTraits<T> {};

template <typename Traits, template <typename> typename... Mixins>
class BaseBinaryTree;

template <typename Traits>
using BinaryTree = BaseBinaryTree<Traits, Callable, Applicable, Searchable, ReverseCallable, Printable, Readable>;

template <typename Traits, template <typename> typename... Mixins>
class BaseBinaryTree : public GeneralContainer<Traits, Mixins...> {
public:
    using MySelf = BaseBinaryTree;
    using Parent = GeneralContainer<Traits, Mixins...>;
    using typename Parent::value_type;
    using typename Parent::Node;
    using typename Parent::NodePtr;
    using typename Parent::ForwardIterator;
    using BackwardIterator = typename Traits::BackwardIterator;
    using Compare = typename Traits::Compare;
    using LinkPtr = BinaryTreeNode<value_type>*;

//

protected:
    using Parent::m_mutex;

    LinkPtr m_root = nullptr;

    virtual void afterInsert(LinkPtr) {}
    virtual void afterRemove(LinkPtr) {}

   
    void internal_insert(
        LinkPtr& node,
        const value_type& value,
        Ref ref,
        LinkPtr parent = nullptr
    );



    // contador
      TX count_nodes(LinkPtr node) const {
        if (!node)
            return 0;

        return 1
            + count_nodes(node->m_pChild[0])
            + count_nodes(node->m_pChild[1]);
    }

    // altura
    TX node_height(LinkPtr node) const {
        if (!node)
            return 0;

        TX izquierda = node_height(node->m_pChild[0]);
        TX derecha = node_height(node->m_pChild[1]);

        return 1 + std::max(izquierda, derecha);
    }
public:
    BaseBinaryTree() = default;
    BaseBinaryTree(const BaseBinaryTree& another) { *this = another; }
    BaseBinaryTree(BaseBinaryTree&& another) noexcept { *this = std::move(another); }
    virtual ~BaseBinaryTree() { clear(); }

    void insert(const value_type& value, Ref ref) {
        std::lock_guard lock(this->m_mutex);
        internal_insert(m_root, value, ref);
    }

    // remove 
      bool remove(const value_type& value) {
        std::lock_guard lock(this->m_mutex);

        Compare cmp;
        LinkPtr nodo = m_root;

        // 
        while (nodo &&
               (cmp(nodo->getValue(), value) ||
                cmp(value, nodo->getValue()))) {

            TX rama = cmp(nodo->getValue(), value);
            nodo = nodo->m_pChild[rama];
        }

        // 
        if (!nodo)
            return false;

        
        if (nodo->m_pChild[0] && nodo->m_pChild[1]) {
            LinkPtr sucesor = nodo->m_pChild[1];

            while (sucesor->m_pChild[0])
                sucesor = sucesor->m_pChild[0];

            
            std::swap(
                static_cast<GeneralNode<value_type>&>(*nodo),
                static_cast<GeneralNode<value_type>&>(*sucesor)
            );

            nodo = sucesor;
        }

        // 
        LinkPtr padre = nodo->m_parent;

        LinkPtr hijo = nodo->m_pChild[0]
                     ? nodo->m_pChild[0]
                     : nodo->m_pChild[1];

        // 
        if (!padre)
            m_root = hijo;
        else if (padre->m_pChild[0] == nodo)
            padre->m_pChild[0] = hijo;
        else
            padre->m_pChild[1] = hijo;

        if (hijo)
            hijo->m_parent = padre;

        //liberar
        delete nodo;
        afterRemove(padre ? padre : hijo);

        return true;
    }
    
    TX size() const {
        std::lock_guard lock(this->m_mutex);
        return count_nodes(m_root);
    }

    TX height() const {
        std::lock_guard lock(this->m_mutex);
        return node_height(m_root);
    }
    //
    
    void printTreeConLineas(
        std::ostream& os = std::cout
    ) const {
        std::lock_guard lock(this->m_mutex);

        TreePrinter<BinaryTreeNode<value_type>> printer(m_root);
        printer.print(os);
    }

    void printTree(
        std::ostream& os = std::cout
    ) const {
        std::lock_guard lock(this->m_mutex);

        TreePrinter<BinaryTreeNode<value_type>> printer(m_root);
        printer.printSide(os);
    }


    // Move constructor and move assignment operator
    // Constructor and destructor
    // Persistencia (operator << y operator >> globales para todas las estructuras pero si es para BT con indentacion entonces hacerlo por separado)
    // Concurrencia
    // Iteradores (forward y backward)
    // FirstThat, ForEach, call, rcall por herencia

    void clear() override;

    void push_back(const value_type& value, Ref ref) override { insert(value, ref); }

    void clear_recursive( LinkPtr node);

    ForwardIterator begin() const override;
    ForwardIterator end() const override { return ForwardIterator(nullptr); }

    BackwardIterator rbegin() const;
    BackwardIterator rend() const { return BackwardIterator(nullptr); }
    
    using PreIterator =
        BinaryTreeIterator<Node, TreeOrder::PreOrder, 0>;

    using ReversePreIterator =
        BinaryTreeIterator<Node, TreeOrder::PreOrder, 1>;

    using InIterator =
        BinaryTreeIterator<Node, TreeOrder::InOrder, 0>;

    using ReverseInIterator =
        BinaryTreeIterator<Node, TreeOrder::InOrder, 1>;

    using PostIterator =
        BinaryTreeIterator<Node, TreeOrder::PostOrder, 0>;

    using ReversePostIterator =
        BinaryTreeIterator<Node, TreeOrder::PostOrder, 1>;

    TreeTraversal<PreIterator> PreOrder() const {
        return TreeTraversal<PreIterator>(m_root);
    }

    TreeTraversal<ReversePreIterator> ReversePreOrder() const {
        return TreeTraversal<ReversePreIterator>(m_root);
    }

    TreeTraversal<InIterator> InOrder() const {
        return TreeTraversal<InIterator>(m_root);
    }

    TreeTraversal<ReverseInIterator> ReverseInOrder() const {
        return TreeTraversal<ReverseInIterator>(m_root);
    }

    TreeTraversal<PostIterator> PostOrder() const {
        return TreeTraversal<PostIterator>(m_root);
    }

    TreeTraversal<ReversePostIterator> ReversePostOrder() const {
        return TreeTraversal<ReversePostIterator>(m_root);
    }


    BaseBinaryTree& operator=(const BaseBinaryTree& another);
    BaseBinaryTree& operator=(BaseBinaryTree&& another) noexcept;
};

template <typename Traits, template <typename> typename... Mixins>
void BaseBinaryTree<Traits, Mixins...>::internal_insert(LinkPtr& node, const value_type& value, Ref ref, LinkPtr parent) {
    if (!node) {
        node = new Node(value, ref);
        node->m_parent = parent;
        afterInsert(node);
        return;
    }
    auto branch = Compare()(node->getValue(), value);
    internal_insert(node->m_pChild[branch], value, ref, node); // Tail recursion to insert in the correct branch
}

template <typename Traits, template <typename> typename... Mixins>
void BaseBinaryTree<Traits, Mixins...>::clear() {
    std::lock_guard lock(this->m_mutex);
    clear_recursive(m_root);
    m_root = nullptr;
}

template <typename Traits, template <typename> typename... Mixins>
void BaseBinaryTree<Traits, Mixins...>::clear_recursive(LinkPtr node) {
    if (!node) return;
    clear_recursive(node->m_pChild[0]);
    clear_recursive(node->m_pChild[1]);
    delete node;
}


template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::begin() const -> ForwardIterator {
    return ForwardIterator(ForwardIterator::first(m_root));
}



template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::rbegin() const -> BackwardIterator {
    return BackwardIterator(BackwardIterator::first(m_root));
}


template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::operator=(const BaseBinaryTree& another) -> BaseBinaryTree& {
    this->copy_from(another);
    return *this;
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::operator=(BaseBinaryTree&& another) noexcept -> BaseBinaryTree& {
    this->move_from(another, [&]{ m_root = std::exchange(another.m_root, nullptr); });
    return *this;
}

#endif // __BINARYTREE_H__