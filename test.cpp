#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include "thread_pool.h"

int main() {
    const int N = 12;
    std::vector<int> results(N, -1);   // 每个任务的计算结果
    std::atomic<int> done{0};          // 已完成任务数
    std::mutex cout_mtx;               // 保护 cout，防止多线程输出乱序

    {
        ThreadPool pool(4);            // 4 个工作线程

        for (int i = 0; i < N; ++i) {
            pool.submit([&, i] {
                int r = i * i;         // 模拟计算
                results[i] = r;        // 每个任务写自己的位置，无竞争

                {
                    std::lock_guard<std::mutex> lk(cout_mtx);
                    std::cout << "task " << i << " => " << r
                              << "  (thread " << std::this_thread::get_id() << ")\n";
                }
                ++done;
            });
        }

        // 等待所有任务执行完
        while (done.load() < N) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }   // 离开作用域，析构自动 shutdown + join

    // 验证结果全部正确
    bool ok = true;
    for (int i = 0; i < N; ++i) {
        if (results[i] != i * i) {
            ok = false;
            std::cout << "FAIL at " << i << ": got " << results[i] << "\n";
        }
    }
    std::cout << (ok ? "=== ALL TESTS PASSED ===\n"
                     : "=== SOME TESTS FAILED ===\n");
    return ok ? 0 : 1;
}
