#pragma once
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>

// ============ 有锁普通队列 ============
// 直接用 std::queue + 一把 std::mutex + std::condition_variable。
// 提供 try_pop（非阻塞）和 wait_and_pop（阻塞）两种出队方式。
// 这是最常用、最简单的线程安全队列，适合当线程池/消息队列的任务队列。

template <typename T>
class mutex_queue {
    mutable std::mutex m_;//在const内部也能被修改，mutable关键字的作用就是允许在const成员函数中修改该成员变量
    std::queue<T> data_;
    std::condition_variable cv_;

public:
    mutex_queue() = default;

    mutex_queue(const mutex_queue& other) {
        std::lock_guard<std::mutex> lk(other.m_);
        data_ = other.data_;
    }
    mutex_queue& operator=(const mutex_queue&) = delete;

    void push(T val) {
        std::lock_guard<std::mutex> lk(m_);
        data_.push(std::move(val));
        cv_.notify_one();
    }

    // ---- 非阻塞 ----
    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lk(m_);
        if (data_.empty()) return false;
        value = std::move(data_.front());
        data_.pop();
        return true;
    }

    std::shared_ptr<T> try_pop() {
        std::lock_guard<std::mutex> lk(m_);
        if (data_.empty()) return std::shared_ptr<T>();
        std::shared_ptr<T> res = std::make_shared<T>(std::move(data_.front()));
        data_.pop();
        return res;
    }

    // ---- 阻塞 ----
    void wait_and_pop(T& value) {
        //wait_and_pop 要"睡"，而"睡"这个动作必须先释放锁（不然别的线程没法 push 来唤醒你），所以：
        std::unique_lock<std::mutex> lk(m_);
        // wait 内部：临时 unlock lk → 睡 → 被 notify 唤醒 → 重新 lock lk
        cv_.wait(lk, [this] { return !data_.empty(); });//谓词防虚假唤醒。

        value = std::move(data_.front());
        data_.pop();
    }

    std::shared_ptr<T> wait_and_pop() {
        std::unique_lock<std::mutex> lk(m_);
        cv_.wait(lk, [this] { return !data_.empty(); });
        std::shared_ptr<T> res = std::make_shared<T>(std::move(data_.front()));
        data_.pop();
        return res;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lk(m_);
        return data_.empty();
    }
};
