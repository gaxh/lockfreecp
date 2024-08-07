#include "fixed_queue.h"
#include "free_allocate.h"

#include <stdio.h>
#include <thread>
#include <vector>
#include <memory>
#include <assert.h>
#include <signal.h>

struct Element {
    unsigned long long value = 0;
    std::vector<std::string> tag;
};


static volatile int stop = 0;

static void sig_handler(int sig) {
    stop = 1;
}

static std::atomic<unsigned long long> push_success(0);
static std::atomic<unsigned long long> pop_success(0);

int main() {
    using ElementFreeNode = FreeAllocate<Element>::ElementFreeNode;

    std::vector<std::thread *> pushers;
    std::vector<std::thread *> popers;

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    std::unique_ptr<FixedQueue<ElementFreeNode *, 1000000>> fq_p =
        std::make_unique<FixedQueue<ElementFreeNode *, 1000000>>();
    FixedQueue<ElementFreeNode *, 1000000> &fq = *fq_p;
    FreeAllocate<Element> free_allocate(1000000);

    for(size_t i = 0; i < free_allocate.GetCapacity(); ++i) {
         ElementFreeNode *elem_node = free_allocate.Allocate();

         if(elem_node) {
            bool ok = fq.Push(elem_node);
            assert(ok);
         }
    }

    for(int i = 0; i < 2; ++i) {
        pushers.emplace_back( new std::thread([&fq, &free_allocate] () {
                    while(!stop) {
                        ElementFreeNode *elem_node = free_allocate.Allocate();

                        if(elem_node) {
                            bool ok = fq.Push(elem_node);
                            assert(ok);
                            push_success.fetch_add(1u, std::memory_order_relaxed);
                        }
                    }
                } ));
    }

    for(int i = 0; i < 2; ++i) {
        popers.emplace_back( new std::thread([&fq, &free_allocate] () {
                    while(!stop) {
                        ElementFreeNode *elem_node;
                        bool ok = fq.Pop(&elem_node);

                        if(ok) {
                            free_allocate.Deallocate(elem_node);
                            pop_success.fetch_add(1u, std::memory_order_relaxed);
                        }
                    }
                } ));
    }

    for(std::thread *t: pushers) {
        t->join();
        delete t;
    }

    for(std::thread *t: popers) {
        t->join();
        delete t;
    }

    {
        ElementFreeNode *elem_node;
        while(fq.Pop(&elem_node)) {
            free_allocate.Deallocate(elem_node);
        }
    }

    printf("push=%llu, pop=%llu\n", push_success.load(std::memory_order_relaxed), pop_success.load(std::memory_order_relaxed));

    return 0;
}
