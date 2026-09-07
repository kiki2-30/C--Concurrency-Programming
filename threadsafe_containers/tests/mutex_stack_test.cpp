#include <atomic>
#include <iostream>
#include <thread>
#include <vector>
#include "../stack/mutex_stack.h"

int main() {
    // 单线程基本功能
    {
        mutex_stack<int> s;
        s.push(1);
        s.push(2);
        s.push(3);
        auto a = s.pop();   // 3
        int b = 0;
        s.pop(b);           // 2
        std::cout << "single-thread top=" << *a << " then=" << b << "\n";
    }

    // 多线程：1 生产者 push N 个，2 消费者 pop N 个（pop 空会抛异常，捕获重试）
    const int N = 100000;
    mutex_stack<int> s;
    std::atomic<int> slots{0};
    std::vector<std::atomic<int>> seen(N);

    std::thread producer([&] {
        for (int i = 0; i < N; ++i) s.push(i);
    });

    std::vector<std::thread> consumers;
    for (int c = 0; c < 2; ++c) {
        consumers.emplace_back([&] {
            while (true) {
                int i = slots.fetch_add(1);
                if (i >= N) break;
                int val = -1;
                while (true) {
                    try {
                        s.pop(val);
                        break;
                    } catch (const empty_stack&) {
                        std::this_thread::yield();
                    }
                }
                seen[val].fetch_add(1);
            }
        });
    }

    producer.join();
    for (auto& t : consumers) t.join();

    int errors = 0;
    for (int i = 0; i < N; ++i) {
        if (seen[i].load() != 1) ++errors;
    }
    std::cout << (errors == 0 ? "=== MUTEX STACK PASSED ===\n" : "=== FAILED ===\n");
    return errors == 0 ? 0 : 1;
}
