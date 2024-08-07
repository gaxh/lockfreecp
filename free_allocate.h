#ifndef __FREE_ALLOCATE_H__
#define __FREE_ALLOCATE_H__

#include <atomic>
#include <thread>
#include <type_traits>

#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#include <pthread.h>

#define ASSERT_LOG(cond, fmt, ...) \
    if(!(cond)) {\
        fprintf(stderr, "[%lu] [%s:%d:%s] " fmt "\n", (unsigned long)pthread_self(), __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__);\
        assert(0);\
    }

template<typename ElementType>
class FreeAllocate {
public:
    FreeAllocate(size_t pool_capacity) : m_capacity(pool_capacity){
        for(size_t i = 0; i < pool_capacity; ++i) {
            ElementFreeNode *elem_node = CreateElementFreeNode();
            PushElementFreeNode(elem_node);
        }
    }

    ~FreeAllocate() {
        Clear();
    }

    struct ElementFreeNode;

    struct alignas(sizeof(ElementFreeNode *) + sizeof(unsigned long)) ElementVersionPointer {
        ElementFreeNode *pointer;
        unsigned long version;
    };

    struct ElementFreeNode {
        alignas(alignof(ElementType)) char buffer[sizeof(ElementType)];
        std::atomic<ElementVersionPointer> next_node = ATOMIC_VAR_INIT(((ElementVersionPointer){nullptr, 0}));
    };

    size_t GetCapacity() const {
        return m_capacity;
    }

    ElementFreeNode *Allocate() {
        ElementFreeNode *p = PopElementFreeNode();
        return p;
    }

    void Deallocate(ElementFreeNode *elem_node) {
        PushElementFreeNode(elem_node);
    }

    void Clear() {
        for(;;) {
            ElementFreeNode *elem_node = PopElementFreeNode();

            if(!elem_node) {
                break;
            }

            DestroyElementFreeNode(elem_node);
        }
    }

    ElementType *AccessElementPointerAt(ElementFreeNode *elem_node) {
        return (ElementType *)elem_node->buffer;
    }

    ElementFreeNode *AccessElementFreeNodeOf(ElementType *pointer) {
        return (ElementFreeNode *)((char *)pointer - offsetof(ElementFreeNode, buffer));
    }

    template<typename ... Args>
    void ConstructAt(ElementFreeNode *elem_node, Args && ... args) {
        ElementType *pointer = AccessElementPointerAt(elem_node);

        new (pointer) ElementType(std::forward<Args>(args) ...);
    }

    void DestructAt(ElementFreeNode *elem_node) {
        ElementType *pointer = AccessElementPointerAt(elem_node);

        pointer->~ElementType();
    }

    template<typename OutType>
    void MoveAt(ElementFreeNode *elem_node, OutType *out) {
        ElementType *pointer = AccessElementPointerAt(elem_node);

        if(out) {
            *out = std::move(*pointer);
        }
    }

private:
    FreeAllocate(const FreeAllocate &);
    FreeAllocate(FreeAllocate &&);
    FreeAllocate &operator=(const FreeAllocate &);
    FreeAllocate &operator=(FreeAllocate &&);

    ElementFreeNode *CreateElementFreeNode() {
        return new ElementFreeNode();
    }

    void DestroyElementFreeNode(ElementFreeNode *elem_node) {
        delete elem_node;
    }

    void PushElementFreeNode(ElementFreeNode *elem_node) {
        ElementVersionPointer read_write = m_read_write.load(std::memory_order_acquire);
        ElementVersionPointer elem_next_node = elem_node->next_node.load(std::memory_order_relaxed);
        do {
            elem_node->next_node.store({read_write.pointer, elem_next_node.version + 1u}, std::memory_order_relaxed);
        } while(!m_read_write.compare_exchange_strong(read_write, {elem_node, read_write.version + 1u},
                    std::memory_order_seq_cst, std::memory_order_acquire));
    }

    ElementFreeNode *PopElementFreeNode() {
        ElementVersionPointer read_write = m_read_write.load(std::memory_order_acquire);
        ElementFreeNode *elem_node;
        ElementVersionPointer next_elem_node;
        do {
            elem_node = read_write.pointer;

            if(!elem_node) {
                // pool is empty
                return nullptr;
            }

            next_elem_node = elem_node->next_node.load(std::memory_order_relaxed);
        } while(!m_read_write.compare_exchange_strong(read_write, {next_elem_node.pointer, read_write.version + 1u},
                    std::memory_order_seq_cst, std::memory_order_acquire));

        return elem_node;
    }

    std::atomic<ElementVersionPointer> m_read_write = ATOMIC_VAR_INIT(((ElementVersionPointer){nullptr, 0}));

    size_t m_capacity;
};

#undef ASSERT_LOG

#endif
