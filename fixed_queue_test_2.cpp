#include "fixed_queue.h"

#include <vector>
#include <unordered_map>

#include <signal.h>

static std::atomic<int> stop = ATOMIC_VAR_INIT(0);

static void sig_handler(int sig) {
    stop.store(1, std::memory_order_relaxed);
}

struct UserData {
    int topic = 0;
    unsigned long counter = 0;
    std::vector<int> xx = {1,2,3,4,5,6,7,8,9,10,};
};

int main() {

    FixedQueue<UserData, 100000> fq;

    std::vector<std::thread> threads;

    auto c = [&fq] (int topic) {
        unsigned long counter = 0;

        while(!stop.load(std::memory_order_relaxed)) {
            UserData ud;
            ud.topic = topic;
            ud.counter = counter;

            if(fq.Push(std::move(ud))) {
                ++counter;
            }
        }

        printf("SEND: topic=%d, counter=%lu\n", topic, counter);
    };

    for(int i = 0; i < 2; ++i) {
        threads.emplace_back(c, i);
    }

    threads.emplace_back([&fq]() {
                std::unordered_map<int, unsigned long> latest;

                while(!stop.load(std::memory_order_relaxed)) {
                    UserData ud;

                    if(fq.Pop(&ud)) {
                        auto iter = latest.find(ud.topic);

                        if(iter != latest.end()) {
                            
                            if(ud.counter > iter->second) {
                                iter->second = ud.counter;
                            } else {
                                fprintf(stderr, "counter error, topic=%d, counter=%lu\n", ud.topic, ud.counter);
                            }

                        } else {
                            latest[ud.topic] = ud.counter;
                        }
                    }
                }

                for(auto iter = latest.begin(); iter != latest.end(); ++iter) {
                    printf("RECV: topic=%d, counter=%lu\n", iter->first, iter->second);
                }
            });

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    for(size_t i = 0; i < threads.size(); ++i) {
        threads[i].join();
    }

    return 0;

}
