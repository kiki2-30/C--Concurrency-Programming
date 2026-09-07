#include <atomic>
#include <iostream>
#include <thread>
#include <vector>
#include "../stack/single_ref_stack.h"

int main() {
    const int N = 100000;
    const int P = 4, C = 4;
    single_ref_stack<int> s;
    std::vector<std::atomic<int>> seen(N);

    std::vector<std::thread> threads;

    // P 个生产者：每个值恰好 push 一次
    for (int p = 0; p < P; ++p) {
        threads.emplace_back([&, p] {
            for (int i = p; i < N; i += P) s.push(i);
        });
    }

    // C 个消费者：总共 pop N 次
    std::atomic<int> slots{0};
    for (int c = 0; c < C; ++c) {
        threads.emplace_back([&] {
            while (true) {
                int i = slots.fetch_add(1);
                if (i >= N) break;
                std::shared_ptr<int> v;
                while (!(v = s.pop())) {          // 空栈则重试
                    std::this_thread::yield();
                }
                seen[*v].fetch_add(1);
            }
        });
    }

    for (auto& t : threads) t.join();

    int errors = 0;
    for (int i = 0; i < N; ++i) {
        if (seen[i].load() != 1) ++errors;
    }
    std::cout << (errors == 0 ? "=== SINGLE REF STACK PASSED ===\n" : "=== FAILED ===\n");
    return errors == 0 ? 0 : 1;
}
