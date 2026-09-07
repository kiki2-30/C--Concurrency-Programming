#pragma once
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stack>

// ============ 有锁栈 + 条件变量（阻塞版）============
// 比 mutex_stack 多一个 std::condition_variable：
//   - 空栈时 wait_and_pop 会阻塞等待，而不是抛异常或让调用方忙等
//   - push 后 notify_one 唤醒一个等待者
// 这是"生产者-消费者"里消费者最常用的写法。

template <typename T>
class waitable_stack {
    std::stack<T> data_;
    mutable std::mutex m_;
    std::condition_variable cv_;

public:
    waitable_stack() = default;

    waitable_stack(const waitable_stack& other) {
        std::lock_guard<std::mutex> lk(other.m_);
        data_ = other.data_;
    }
    waitable_stack& operator=(const waitable_stack&) = delete;

    void push(T val) {
        std::lock_guard<std::mutex> lk(m_);
        data_.push(std::move(val));
        cv_.notify_one();
    }

    // 阻塞等待直到有数据，返回 shared_ptr
    std::shared_ptr<T> wait_and_pop() {
        std::unique_lock<std::mutex> lk(m_);
        cv_.wait(lk, [this] { return !data_.empty(); });
        std::shared_ptr<T> res = std::make_shared<T>(std::move(data_.top()));
        data_.pop();
        return res;
    }

    void wait_and_pop(T& value) {
        std::unique_lock<std::mutex> lk(m_);
        cv_.wait(lk, [this] { return !data_.empty(); });
        value = std::move(data_.top());
        data_.pop();
    }

    // 非阻塞尝试弹栈
    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lk(m_);
        if (data_.empty()) return false;
        value = std::move(data_.top());
        data_.pop();
        return true;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lk(m_);
        return data_.empty();
    }
};
