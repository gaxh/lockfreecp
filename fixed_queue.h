#ifndef __FIXED_QUEUE_H__
#define __FIXED_QUEUE_H__

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

/*
#define ASSERT_LOG(cond, fmt, ...) \
    if(!(cond)) {}
*/

template<typename ElementType, size_t Capacity>
class FixedQueue {
public:

    FixedQueue() {
        static_assert(Capacity != 0);
    }

    ~FixedQueue() {
        Clear();
    }

    template<typename ... Args>
    bool Push(Args && ... args) {
        size_t read;
        size_t write;
        ElementNode *elem_node;

        write = m_write.load(std::memory_order_relaxed);
RETRY:
        read = m_read.load(std::memory_order_relaxed);

        if(read + Capacity == write) {
            // queue is full
            return false;
        }

        if(!m_write.compare_exchange_strong(write, write + 1u, std::memory_order_relaxed)) {
            // take pos failed
            goto RETRY;
        }

        ASSERT_LOG(read <= write, "read=%lu, write=%lu", read, write);
        ASSERT_LOG(read + Capacity > write, "read=%lu, write=%lu, write-read=%lu",
                read, write, write-read);

        // take pos success
        elem_node = &m_element_nodes[ArrayIndex(write)];

        // start write (lock elem_node)
        for(int expected = RWREF_EMPTY; !elem_node->rwref.compare_exchange_strong(expected, RWREF_WRITING, std::memory_order_acquire); expected = RWREF_EMPTY) {
            std::this_thread::yield();
        }

        ConstructElementAt(elem_node, std::forward<Args>(args) ...);

        // finish write (unlock elem_node)
        {
            int expected = RWREF_WRITING;
            bool ok = elem_node->rwref.compare_exchange_strong(expected, RWREF_WRITTEN, std::memory_order_release);
            ASSERT_LOG(ok, "read=%lu, write=%lu, old=%d", read, write, expected);
        }

        return true;
    }

    template<typename OutType>
    bool Pop(OutType *out) {
        size_t read;
        size_t write;
        ElementNode *elem_node;

        read = m_read.load(std::memory_order_relaxed);
RETRY:
        write = m_write.load(std::memory_order_relaxed);

        if(read == write) {
            // queue is empty
            return false;
        }

        if(!m_read.compare_exchange_strong(read, read + 1u, std::memory_order_relaxed)) {
            // take pos failed
            goto RETRY;
        }

        ASSERT_LOG(read < write, "read=%lu, write=%lu", read, write);
        ASSERT_LOG(read + Capacity >= write, "read=%lu, write=%lu, write-read=%lu",
                read, write, write-read);

        // take pos success
        elem_node = &m_element_nodes[ArrayIndex(read)];

        // start read
        for(int expected = RWREF_WRITTEN; !elem_node->rwref.compare_exchange_strong(expected, RWREF_READING, std::memory_order_acquire); expected = RWREF_WRITTEN) {
            std::this_thread::yield();
        }

        if(out) {
            *out = std::move(*AccessElementAt(elem_node));
        }

        DestructElementAt(elem_node);

        // finish read
        {
            int expected = RWREF_READING;
            bool ok = elem_node->rwref.compare_exchange_strong(expected, RWREF_EMPTY, std::memory_order_release);
            ASSERT_LOG(ok, "read=%lu, write=%lu, old=%d", read, write, expected);
        }

        return true;
    }

    void Clear() {
        while(Pop<ElementType>(nullptr));
    }

    size_t ApproximateSize() const {
        return m_write.load(std::memory_order_relaxed) -
            m_read.load(std::memory_order_relaxed);
    }

private:
    FixedQueue(const FixedQueue &);
    FixedQueue(FixedQueue &&);
    FixedQueue &operator=(const FixedQueue &);
    FixedQueue &operator=(FixedQueue &&);

    static constexpr size_t ElementTypeSize = sizeof(ElementType);

    struct ElementNode {
        std::atomic<int> rwref = ATOMIC_VAR_INIT(RWREF_EMPTY);
        alignas(alignof(ElementType)) char buffer[ElementTypeSize];
    };

    template<typename ... Args>
    void ConstructElementAt(ElementNode *node, Args && ... args) {
        new (AccessElementAt(node)) ElementType(std::forward<Args>(args) ...);
    }

    void DestructElementAt(ElementNode *node) {
        AccessElementAt(node)->~ElementType();
    }

    ElementType *AccessElementAt(ElementNode *node) {
        return (ElementType *)node->buffer;
    }

    size_t ArrayIndex(size_t pos) const {
        return pos % Capacity;
    }

    ElementNode m_element_nodes[Capacity];

    std::atomic<size_t> m_read = ATOMIC_VAR_INIT(0); // next read pos
    std::atomic<size_t> m_write = ATOMIC_VAR_INIT(0); // next write pos

    // read start: RWREF_WRITTEN -> RWREF_READING
    // read finish: RWREF_READING -> RWREF_EMPTY
    // write start: RWREF_EMPTY -> RWREF_WRITING
    // write finish: RWREF_WRITING -> RWREF_WRITTEN
    enum RWREF_STATUS {
        RWREF_EMPTY = 0,
        RWREF_READING = 1,
        RWREF_WRITING = 2,
        RWREF_WRITTEN = 3,
    };
};

#undef ASSERT_LOG

#endif
