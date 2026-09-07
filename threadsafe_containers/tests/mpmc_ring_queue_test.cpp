#include <atomic>
#include <iostream>
#include <thread>
#include <vector>
#include "../ring_queue/mpmc_ring_queue.h"

int main() {
    const int N = 200000;          // 数据量
    const int P = 4, C = 4;        // 4 生产者 + 4 消费者
    MpmcRingQueue<int, 1024> q;    // 容量必须是 2 的幂

    std::vector<std::atomic<int>> seen(N);   // seen[i] = 值 i 被 pop 的次数
    std::vector<std::thread> threads;

    // 4 个生产者：每个值 0..N-1 恰好 push 一次
    for (int p = 0; p < P; ++p) {
        threads.emplace_back([&, p] {
            for (int i = p; i < N; i += P) {
                while (!q.push(i)) {          // 满了就重试
                    std::this_thread::yield();
                }
            }
        });
    }

    // 4 个消费者：总共 pop N 次，记录每个值出现的次数
    std::atomic<int> slots{0};
    for (int c = 0; c < C; ++c) {
        threads.emplace_back([&] {
            int val;
            while (true) {
                int i = slots.fetch_add(1);
                if (i >= N) break;
                while (!q.pop(val)) {         // 空了就重试
                    std::this_thread::yield();
                }
                seen[val].fetch_add(1);
            }
        });
    }

    for (auto& t : threads) t.join();

    int errors = 0;
    for (int i = 0; i < N; ++i) {
        if (seen[i].load() != 1) {
            if (errors < 10) {
                std::cout << "value " << i << " popped " << seen[i].load() << " times\n";
            }
            ++errors;
        }
    }

    std::cout << (errors == 0 ? "=== ALL TESTS PASSED ===\n"
                              : "=== SOME TESTS FAILED ===\n");
    return errors == 0 ? 0 : 1;
}
