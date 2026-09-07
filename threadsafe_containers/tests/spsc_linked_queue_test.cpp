#include <iostream>
#include <thread>
#include <vector>
#include "../queue/spsc_linked_queue.h"

int main() {
    const int N = 100000;
    spsc_linked_queue<int> q;
    std::vector<int> got(N, -1);

    std::thread producer([&] {
        for (int i = 0; i < N; ++i) q.push(i);
    });

    std::thread consumer([&] {
        int i = 0;
        while (i < N) {
            auto p = q.pop();
            if (!p) {
                std::this_thread::yield();
                continue;
            }
            got[i++] = *p;
        }
    });

    producer.join();
    consumer.join();

    int errors = 0;
    for (int i = 0; i < N; ++i) {
        if (got[i] != i) {
            if (errors < 5) std::cout << "got[" << i << "]=" << got[i] << "\n";
            ++errors;
        }
    }
    std::cout << (errors == 0 ? "=== SPSC LINKED QUEUE PASSED (FIFO correct) ===\n"
                              : "=== FAILED ===\n");
    return errors == 0 ? 0 : 1;
}
