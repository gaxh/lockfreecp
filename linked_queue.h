#ifndef __LINKED_QUEUE_H__
#define __LINKED_QUEUE_H__

#include "free_allocate.h"

#include <atomic>
#include <thread>

#include <stddef.h>
#include <assert.h>
#include <stdio.h>

#define ASSERT_LOG(cond, fmt, ...) \
    if(!(cond)) {\
        fprintf(stderr, "[%s:%d:%s] " fmt "\n", __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__);\
        assert(0);\
    }

template<typename ElementType, bool MultiReader = true>
class LinkedQueue {
public:

    LinkedQueue(size_t capacity) : free_allocate(capacity + 1u), m_capacity(capacity) {
        // at lease one node as "empty node"
        assert(capacity < capacity + 1u);

        ElementFreeNode *empty_node = free_allocate.Allocate();
        free_allocate.AccessElementPointerAt(empty_node)->lifetime.store(ELEMENT_LIFETIME_DESTRUCTED, std::memory_order_relaxed);
        ElementVersionPointer next_elem_node = empty_node->next_node.load(std::memory_order_relaxed);
        empty_node->next_node.store({nullptr, next_elem_node.version + 1u}, std::memory_order_relaxed);

        m_read.store({empty_node, 0}, std::memory_order_relaxed);
        m_write.store({empty_node, 0});
    }

    ~LinkedQueue() {
        // Clear();

        // deallocate "empty node"
        ElementVersionPointer read = m_read.load(std::memory_order_relaxed);
        ElementVersionPointer write = m_write.load(std::memory_order_relaxed);
        ASSERT_LOG(read.pointer == write.pointer, "queue is NOT cleared. call Clear() or ClearF() before \"%s\"", __PRETTY_FUNCTION__);
        free_allocate.Deallocate(read.pointer);
    }

    size_t GetCapacity() const {
        return m_capacity;
    }

    template<typename ... Args>
    bool Push(Args && ... args) {
        return PushF([&args ...](ElementType *elem) {
                new (elem) ElementType(std::forward<Args>(args) ...);
                });
    }

    template<typename Function>
    // f(ElementType *elem), elem is not constructed
    bool PushF(Function f) {
        ElementFreeNode *elem_node = free_allocate.Allocate();

        if(!elem_node) {
            // pool has been used up
            return false;
        }

        ElementContainer *elem_container = free_allocate.AccessElementPointerAt(elem_node);
        f( (ElementType *)elem_container->buffer );
        elem_container->lifetime.store(ELEMENT_LIFETIME_CONSTRUCTED, std::memory_order_relaxed);

        ElementVersionPointer next_elem_node = elem_node->next_node.load(std::memory_order_relaxed);
        elem_node->next_node.store({nullptr, next_elem_node.version + 1u}, std::memory_order_relaxed);

        ElementVersionPointer write;
        ElementVersionPointer write_next;
        for(;;) {
            write = m_write.load(std::memory_order_relaxed);
            write_next = write.pointer->next_node.load(std::memory_order_relaxed);

            // for multiple Push(), only one operation's write_next.pointer is nullptr.
            // other Push() must wait until write_next.pointer is nullptr.
            if(!write_next.pointer) {
                // for multiple Push(), once this CAS operation is successful,
                // other Push() will meet write_next.pointer NOT nullptr
                if(write.pointer->next_node.compare_exchange_strong(write_next, {elem_node, write_next.version + 1u})) {
                    m_write.compare_exchange_strong(write, {elem_node, write.version + 1u});
                    break;
                }
            } else {
                // help change m_write to its right value
                // we can also wait last success Push() to complete this operation
                m_write.compare_exchange_strong(write, {write_next.pointer, write.version + 1u});
            }
        }

        return true;
    }

    template<typename OutType>
    bool Pop(OutType *out) {
        return PopF([out](ElementType *elem) {
                if(out) {
                    *out = *elem;
                }

                elem->~ElementType();
                });
    }

    template<typename Function>
    // f(ElementType *elem), corresponding to PushF()
    bool PopF(Function f) {
        ElementVersionPointer write;
        ElementVersionPointer read;
        ElementVersionPointer read_next;

        for(;;) {
            write = m_write.load(std::memory_order_acquire);
            read = m_read.load(std::memory_order_relaxed);

            read_next = read.pointer->next_node.load(std::memory_order_relaxed);

            if(read_next.pointer) {

                if(read.pointer != write.pointer) {
                    if(m_read.compare_exchange_strong(read, {read_next.pointer, read.version + 1u})) {
                        break;
                    }

                } else {
                    // Push() NOT complete
                    // we can also wait Push() complete
                    m_write.compare_exchange_strong(write, {read_next.pointer, write.version + 1u});
                }

            } else {
                // queue is empty
                return false;
            }
        }

        {
            bool ok;
            int expected;
            ElementContainer *elem_container = free_allocate.AccessElementPointerAt(read_next.pointer);

            expected = ELEMENT_LIFETIME_CONSTRUCTED;
            ok = elem_container->lifetime.compare_exchange_strong(expected, ELEMENT_LIFETIME_READING, std::memory_order_acquire);
            ASSERT_LOG(ok, "element lifetime is invalid: expected=%d, real=%d", ELEMENT_LIFETIME_CONSTRUCTED, expected);

            f( (ElementType *)elem_container->buffer );

            expected = ELEMENT_LIFETIME_READING;
            ok = elem_container->lifetime.compare_exchange_strong(expected, ELEMENT_LIFETIME_DESTRUCTED, std::memory_order_release);
            ASSERT_LOG(ok, "element lifetime is invalid: expected=%d, real=%d", ELEMENT_LIFETIME_READING, expected);
        }

        {
            int expected;
            bool ok;
            ElementContainer *elem_container = free_allocate.AccessElementPointerAt(read.pointer);

            if(MultiReader) {
                for(;;) {
                    expected = ELEMENT_LIFETIME_DESTRUCTED;
                    ok = elem_container->lifetime.compare_exchange_strong(expected, ELEMENT_LIFETIME_RECYCLE, std::memory_order_acquire);

                    if(ok) {
                        break;
                    } else {
                        std::this_thread::yield();
                    }
                }
            } else {
                expected = ELEMENT_LIFETIME_DESTRUCTED;
                ok = elem_container->lifetime.compare_exchange_strong(expected, ELEMENT_LIFETIME_RECYCLE, std::memory_order_acquire);

                ASSERT_LOG(ok, "multiple reader detected: element lifetime is invalid: expected=%d, real=%d", ELEMENT_LIFETIME_DESTRUCTED, expected);
            }

            free_allocate.Deallocate(read.pointer);
        }

        return true;
    }

    template<typename Function>
    // f is same in PopF()
    void ClearF(Function f) {
        while(PopF(f));
    }

    void Clear() {
        auto f = [] (ElementType *elem) {
            elem->~ElementType();
        };

        ClearF(f);
    }

private:
    enum ELEMENT_LIFETIME {
        ELEMENT_LIFETIME_CONSTRUCTED = 0,
        ELEMENT_LIFETIME_READING,
        ELEMENT_LIFETIME_DESTRUCTED,
        ELEMENT_LIFETIME_RECYCLE,
    };

    struct ElementContainer {
        alignas(alignof(ElementType)) char buffer[sizeof(ElementType)];
        std::atomic<int> lifetime;
    };

    using FreeAllocateType = FreeAllocate<ElementContainer>;
    using ElementFreeNode = typename FreeAllocateType::ElementFreeNode;
    using ElementVersionPointer = typename FreeAllocateType::ElementVersionPointer;

    LinkedQueue(const LinkedQueue &);
    LinkedQueue(LinkedQueue &&);
    LinkedQueue &operator=(const LinkedQueue &);
    LinkedQueue &operator=(LinkedQueue &&);

    FreeAllocateType free_allocate;

    std::atomic<ElementVersionPointer> m_read;
    std::atomic<ElementVersionPointer> m_write;

    size_t m_capacity;
};


#undef ASSERT_LOG

#endif
