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
/*
    UserData *ud = free_allocate.Allocate();

    free_allocate.Deallocate(ud);
*/

    auto job = [&free_allocate](int index) {
        size_t success = 0;
        size_t failed = 0;
        printf("job %d start\n", index);

        while(!stop.load(std::memory_order_relaxed)) {
            ElementFreeNode *elem_node = free_allocate.Allocate();

            if(elem_node) {
                free_allocate.ConstructAt(elem_node, std::make_shared<std::string>("OK"));
                UserData *ud = free_allocate.AccessElementPointerAt(elem_node);

                elem_node = free_allocate.AccessElementFreeNodeOf(ud);
                free_allocate.DestructAt(elem_node);
                free_allocate.Deallocate(elem_node);

            }
            if(elem_node) {
                ++success;
            } else {
                ++failed;
            }
        }
        printf("job %d finish, success=%lu, failed=%lu\n", index, success, failed);
    };

    std::vector<std::thread> workers;

    for(int i = 0; i < 20; ++i) {
        workers.emplace_back(job, i);
    }

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    for(size_t i = 0; i < workers.size(); ++i) {
        workers[i].join();
    }

    free_allocate.Clear();

    return 0;
}
