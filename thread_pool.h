#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__

#include<thread>
using namespace std;

class ThreadPool {
    public:
        explicit ThreadPool(size_t num = thread::hardware_concurrency()): stop(false) {
            for(size_t i = 0; i < num; ++i) {
                workers.emplace_back([this] {work_loop();});
            }
        }
        ~ThreadPool() {
            shutdown();
        }

        void submit(std::function<void()> task) {
            {
                std::lock_guard<mutex> lock(mtx);
                if (stop) throw runtime_error("submit on stopped ThreadPool");
                tasks.emplace(move(task));
            }       
            condition.notify_one();
        }

    private:
        void work_loop() {
            while(true) {
                std::function<void()> task;

                {
                    unique_lock<mutex> lock(mtx);
                    condition.wait(lock, [this] {
                        return stop || !tasks.empty();
                    });

                    if(stop && tasks.empty()) {
                        return;
                    }
                    task = move(tasks.front());
                    tasks.pop();

                }
                task();
            }

        }

        void shutdown() {
            {
                lock_guard<mutex>lock(mtx);
                stop = true;
            }
            condition.notify_all();
            for(thread& worker : workers) {
                if(worker.joinable()) {
                    worker.join();
                }
            }
        }
        vector<thread> workers;
        queue<function<void()>> tasks;
        mutex mtx;
        condition_variable condition;
        bool stop;
};

#endif  // !__THREAD_POOL_H__