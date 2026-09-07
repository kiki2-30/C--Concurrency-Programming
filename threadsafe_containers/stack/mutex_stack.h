#pragma once
#include <exception>
#include <memory>
#include <mutex>
#include <stack>

// ============ 有锁栈（最基础版）============
// 一把 std::mutex 保护整个 std::stack，所有操作串行化（粗粒度锁）。
// 关键点：把"判空 + 取值 + 弹栈"合成一次加锁操作，
// 避免 std::stack 的 top()+pop() 分离导致的竞态。

struct empty_stack : std::exception {
    const char* what() const noexcept override { return "empty stack"; }
};

template <typename T>
class mutex_stack {
    std::stack<T> data_;
    mutable std::mutex m_;

public:
    mutex_stack() = default;

    mutex_stack(const mutex_stack& other) {
        std::lock_guard<std::mutex> lk(other.m_);
        data_ = other.data_;
    }
    mutex_stack& operator=(const mutex_stack&) = delete;

    void push(T val) {
        std::lock_guard<std::mutex> lk(m_);
        data_.push(std::move(val));
    }

    // 返回 shared_ptr：空栈时抛异常
    std::shared_ptr<T> pop() {
        std::lock_guard<std::mutex> lk(m_);
        if (data_.empty()) throw empty_stack();
        std::shared_ptr<T> res = std::make_shared<T>(std::move(data_.top()));
        data_.pop();
        return res;
    }

    // 传引用版本：避免一次拷贝，但要求 T 可赋值
    void pop(T& value) {
        std::lock_guard<std::mutex> lk(m_);
        if (data_.empty()) throw empty_stack();
        value = std::move(data_.top());
        data_.pop();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lk(m_);
        return data_.empty();
    }
};
