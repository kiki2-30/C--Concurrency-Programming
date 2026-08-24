#include <iostream>
#include <thread>
#include <vector>
#include "spsc_lockfree_queue.h"

int main() {
    // ===== 1. 单线程功能测试 =====
    std::cout << "--- single-thread test ---\n";
    SpscQueue<int, 4> q;          // 容量 4

    for (int i = 1; i <= 4; ++i) {
        std::cout << "push " << i << " : " << (q.push(i) ? "ok" : "FULL") << "\n";
    }
    std::cout << "push 5 : " << (q.push(5) ? "ok" : "FULL") << "\n";

    int v;
    while (q.pop(v)) {
        std::cout << "pop " << v << "\n";
    }
    std::cout << "pop again : " << (q.pop(v) ? "ok" : "EMPTY") << "\n";

    // ===== 2. SPSC 多线程：1 生产者 + 1 消费者，验证 FIFO =====
    std::cout << "--- SPSC multi-thread test ---\n";
    const int N = 100000;
    SpscQueue<int, 1024> q2;
    std::vector<int> got(N, -1);

    std::thread producer([&] {
        for (int i = 0; i < N; ++i) {
            while (!q2.push(i)) { /* 满了就重试 */ }
        }
    });

    std::thread consumer([&] {
        int val;
        for (int i = 0; i < N; ++i) {
            while (!q2.pop(val)) { /* 空了就重试 */ }
            got[i] = val;
        }
    });

    producer.join();
    consumer.join();

    // 验证：全部收到，且严格 FIFO（got[i] == i）
    bool ok = true;
    for (int i = 0; i < N; ++i) {
        if (got[i] != i) {
            ok = false;
            std::cout << "FAIL at " << i << ": got " << got[i] << "\n";
            break;
        }
    }
    std::cout << (ok ? "=== SPSC PASSED (100000 items, FIFO correct) ===\n"
                     : "=== SPSC FAILED ===\n");
    return ok ? 0 : 1;
}
