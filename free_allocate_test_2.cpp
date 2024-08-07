#include "free_allocate.h"

#include <vector>
#include <string>
#include <thread>
#include <memory>

#include <signal.h>

struct UserData {
    int x = 99999;
    std::vector<std::string> vs {"this", "is", "a", "string", "vector"};
    std::shared_ptr<std::string> shared_v;

    UserData(std::shared_ptr<std::string> v) : shared_v(v) {}
    ~UserData() {}
};

static std::atomic<int> stop = ATOMIC_VAR_INIT(0);

static void sig_handler(int sig) {
    stop.store(1, std::memory_order_relaxed);
}

int main() {
    FreeAllocate<UserData> free_allocate(10);
    using ElementFreeNode = FreeAllocate<UserData>::ElementFreeNode;

    auto producer = [&free_allocate](int index, std::shared_ptr<std::atomic<UserData *>> ud) {
        printf("producer %d start\n", index);

        unsigned long counter = 0;
        while(!stop.load(std::memory_order_relaxed)) {
            if(ud->load() == nullptr) {
                ElementFreeNode *elem_node = free_allocate.Allocate();
                if(elem_node) {
                    free_allocate.ConstructAt(elem_node, std::make_shared<std::string>("OK"));
                    ud->store( free_allocate.AccessElementPointerAt(elem_node) );
                    ++counter;
                }
            }
        }

        printf("producer %d stop, counter=%lu\n", index, counter);
    };

    auto consumer = [&free_allocate](int index, std::shared_ptr<std::atomic<UserData *>> ud) {
        printf("consumer %d start\n", index);

        unsigned long counter = 0;
        while(!stop.load(std::memory_order_relaxed)) {
            UserData *p = ud->load();
            if(p) {
                ElementFreeNode *elem_node = free_allocate.AccessElementFreeNodeOf(p);
                free_allocate.DestructAt(elem_node);
                free_allocate.Deallocate(elem_node);
                ud->store(nullptr);
                ++counter;
            }
        }

        printf("consumer %d stop, counter=%lu\n", index, counter);
    };

    std::vector<std::thread> workers;
    std::vector<std::shared_ptr<std::atomic<UserData *>>> ud_refs;

    for(int i = 0; i < 3; ++i) {
        std::shared_ptr<std::atomic<UserData *>> ud = std::make_shared<std::atomic<UserData *>>(nullptr);
        ud_refs.emplace_back(ud);

        workers.emplace_back(producer, i, ud);
        workers.emplace_back(consumer, i, ud);
    }

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    for(size_t i = 0; i < workers.size(); ++i) {
        workers[i].join();
    }

    for(size_t i = 0; i < ud_refs.size(); ++i) {
        std::shared_ptr<std::atomic<UserData *>> &ud = ud_refs[i];

        UserData *p = ud->load();

        if(p) {
            ElementFreeNode *elem_node = free_allocate.AccessElementFreeNodeOf(p);
            free_allocate.DestructAt(elem_node);
            free_allocate.Deallocate(elem_node);
        }
    }

    return 0;
}

