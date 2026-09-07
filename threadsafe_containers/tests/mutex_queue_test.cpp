#include <atomic>
#include <iostream>
#include <thread>
#include <vector>
#include "../queue/mutex_queue.h"

int main() {
    const int N = 200000;
    const int P = 2, C = 2;
    mutex_queue<int> q;
    std::vector<std::atomic<int>> seen(N);

    std::vector<std::thread> threads;

    // P 个生产者：每个值恰好 push 一次
    for (int p = 0; p < P; ++p) {
        threads.emplace_back([&, p] {
            for (int i = p; i < N; i += P) q.push(i);
        });
    }

    // C 个消费者：总共 pop N 次（阻塞等待）
    std::atomic<int> slots{0};
    for (int c = 0; c < C; ++c) {
        threads.emplace_back([&] {
            int val;
            while (true) {
                int i = slots.fetch_add(1);
                if (i >= N) break;
                q.wait_and_pop(val);
                seen[val].fetch_add(1);
            }
        });
    }

    for (auto& t : threads) t.join();

    int errors = 0;
    for (int i = 0; i < N; ++i) {
        if (seen[i].load() != 1) ++errors;
    }
    std::cout << (errors == 0 ? "=== MUTEX QUEUE PASSED ===\n" : "=== FAILED ===\n");
    return errors == 0 ? 0 : 1;
}
