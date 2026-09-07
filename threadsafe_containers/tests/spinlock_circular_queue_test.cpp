#include <atomic>
#include <iostream>
#include <thread>
#include <vector>
#include "../ring_queue/spinlock_circular_queue.h"

int main() {
    // ===== 1. 单线程基本功能 =====
    std::cout << "--- single-thread test ---\n";
    CircularQueSeq<int, 4> q;          // 容量 4

    for (int i = 1; i <= 4; ++i) {
        std::cout << "push " << i << " : " << (q.push(i) ? "ok" : "FULL") << "\n";
    }
    std::cout << "push 5 : " << (q.push(5) ? "ok" : "FULL") << "\n";

    int v;
    while (q.pop(v)) {
        std::cout << "pop " << v << "\n";
    }
    std::cout << "pop again : " << (q.pop(v) ? "ok" : "EMPTY") << "\n";

    // ===== 2. 多线程并发：2 生产者 + 2 消费者 =====
    std::cout << "--- multi-thread test ---\n";
    const int N = 2000;                // 总任务数
    CircularQueSeq<int, 64> q2;        // 容量 64

    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};

    auto producer = [&] {
        while (true) {
            int i = produced.fetch_add(1);
            if (i >= N) break;
            while (!q2.push(i)) { /* 满了就自旋重试 */ }
        }
    };
    auto consumer = [&] {
        int val;
        while (consumed.load() < N) {
            if (q2.pop(val)) {
                consumed.fetch_add(1);
            }
        }
    };

    std::vector<std::thread> threads;
    threads.emplace_back(producer);
    threads.emplace_back(producer);
    threads.emplace_back(consumer);
    threads.emplace_back(consumer);
    for (auto& t : threads) t.join();

    std::cout << "produced = " << produced.load()
              << ", consumed = " << consumed.load() << "\n";
    std::cout << (consumed.load() == N ? "=== MULTI-THREAD PASSED ===\n"
                                       : "=== MULTI-THREAD FAILED ===\n");
    return consumed.load() == N ? 0 : 1;
}
