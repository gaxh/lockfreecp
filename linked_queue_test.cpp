#include "linked_queue.h"
#include <stdio.h>
#include <thread>
#include <vector>
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
    std::vector<std::thread *> pushers;
    std::vector<std::thread *> popers;

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    LinkedQueue<Element> q(2000000);
/*
    for(size_t i = 0; i < q.GetCapacity(); ++i) {
        Element e{123, {"__TAG__", "__ANOTHER_TAG__",}};
        q.Push(std::move(e));
    }
*/
    for(int i = 0; i < 2; ++i) {
        pushers.emplace_back( new std::thread([&q]() {
                    for(;!stop;) {
                        Element e{123, {"__TAG__", "__ANOTHER_TAG__",}};

                        bool ok = q.Push(std::move(e));

                        if(ok) {
                            push_success.fetch_add(1u, std::memory_order_relaxed);
                        }
                    }
                    }) );
    }

    for(int i = 0; i < 3; ++i) {
        popers.emplace_back( new std::thread([&q]() {
                    for(;!stop;) {
                        Element e;

                        bool ok = q.Pop(&e);

                        if(ok) {
                            pop_success.fetch_add(1u, std::memory_order_relaxed);
                        }
                    }
                    }) );
    }

    for(std::thread *t: pushers) {
        t->join();
        delete t;
    }

    for(std::thread *t: popers) {
        t->join();
        delete t;
    }

    printf("push_success=%llu, pop_success=%llu\n", push_success.load(), pop_success.load());

    return 0;
}
