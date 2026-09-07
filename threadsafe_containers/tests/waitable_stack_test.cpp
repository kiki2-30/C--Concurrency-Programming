#include <atomic>
#include <iostream>
#include <thread>
#include <vector>
#include "../stack/waitable_stack.h"

int main() {
    const int N = 100000;
    waitable_stack<int> s;
    std::atomic<int> slots{0};
    std::vector<std::atomic<int>> seen(N);

    std::thread producer([&] {
        for (int i = 0; i < N; ++i) s.push(i);
    });

    std::vector<std::thread> consumers;
    for (int c = 0; c < 2; ++c) {
        consumers.emplace_back([&] {
            int val;
            while (true) {
                int i = slots.fetch_add(1);
                if (i >= N) break;
                s.wait_and_pop(val);   // 阻塞等待数据
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
    std::cout << (errors == 0 ? "=== WAITABLE STACK PASSED ===\n" : "=== FAILED ===\n");
    return errors == 0 ? 0 : 1;
}
