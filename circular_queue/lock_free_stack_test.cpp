#include <atomic>
#include <iostream>
#include <thread>
#include <vector>
#include "lock_free_stack.h"

int main() {
    const int N = 100000;   // 数据量
    const int P = 4, C = 4; // 4 生产者 + 4 消费者

    lock_free_stack<int> stk;
    std::vector<std::atomic<int>> seen(N);   // seen[i] = 值 i 被 pop 的次数

    std::vector<std::thread> threads;

    // 4 生产者 push 唯一的 id（0..N-1 各一次）
    for (int p = 0; p < P; ++p) {
        threads.emplace_back([&, p] {
            for (int i = p; i < N; i += P) {
                stk.push(i);
            }
        });
    }

    // 4 消费者 pop N 次（栈是 LIFO，只验证"无丢失、无重复"）
    std::atomic<int> slots{0};
    for (int c = 0; c < C; ++c) {
        threads.emplace_back([&] {
            while (true) {
                int i = slots.fetch_add(1);
                if (i >= N) break;
                std::shared_ptr<int> v;
                while (!(v = stk.pop())) {   // 空则让出 CPU 重试
                    std::this_thread::yield();
                }
                seen[*v].fetch_add(1);
            }
        });
    }

    for (auto& t : threads) t.join();

    // 验证：每个值恰好被 pop 一次
    int errors = 0;
    for (int i = 0; i < N && errors < 10; ++i) {
        if (seen[i].load() != 1) {
            std::cout << "ERROR: value " << i << " popped "
                      << seen[i].load() << " times\n";
            ++errors;
        }
    }
    std::cout << (errors == 0
                  ? "=== LOCK-FREE STACK PASSED: 100000 items, no loss, no duplicate ===\n"
                  : "=== FAILED ===\n");
    return errors == 0 ? 0 : 1;
}
