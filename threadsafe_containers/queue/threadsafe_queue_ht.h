#pragma once
#include <condition_variable>
#include <memory>
#include <mutex>

// ============ 有锁队列：链表 + head/tail 双锁（细粒度锁）============
// 与 mutex_queue（一把锁包 std::queue）不同，这里自己写链表，并把
// "队尾"和"队头"拆成两把独立的锁：
//   - push 只动 tail，只锁 tail_mutex_
//   - pop  只动 head，只锁 head_mutex_
// 这样生产者的 push 和消费者的 pop 可以同时进行，并发度更高。
//
// 关键技巧：队列里始终挂着一个"虚位节点"（dummy node），tail_ 永远指向它：
//   - push：给虚位节点装数据 → 再挂一个新的空节点 → tail_ 前移
//   - pop ：摘掉 head_ 节点 → head_ 前移
// 有了虚位节点，pop 判断"空不空"只需比较 head_ 和 tail_，不用碰 tail 的数据。

template <typename T>
class threadsafe_queue_ht {
    struct node {
        std::shared_ptr<T> data;
        std::unique_ptr<node> next;
    };

    mutable std::mutex        head_mutex_;
    std::unique_ptr<node>     head_;       // 队头，pop 用
    mutable std::mutex        tail_mutex_;
    node*                     tail_;       // 队尾（虚位节点），push 用
    std::condition_variable   data_cond_;

    // 读取 tail 指针：tail_ 归 tail_mutex_ 管，所以要锁了再读。
    // 标 const 是为了让 empty() 这类 const 函数也能调用它（tail_mutex_ 是 mutable）。
    node* get_tail() const {
        std::lock_guard<std::mutex> lk(tail_mutex_);
        return tail_;
    }

public:
    threadsafe_queue_ht() : head_(new node), tail_(head_.get()) {}

    threadsafe_queue_ht(const threadsafe_queue_ht&) = delete;
    threadsafe_queue_ht& operator=(const threadsafe_queue_ht&) = delete;

    // 入队：只锁 tail
    void push(T val) {
        std::shared_ptr<T> data = std::make_shared<T>(std::move(val));
        std::unique_ptr<node> p(new node);       // 新虚位节点
        node* new_tail = p.get();
        {
            std::lock_guard<std::mutex> lk(tail_mutex_);
            tail_->data = data;                  // 尾虚位节点装数据 → 变真节点
            tail_->next = std::move(p);          // 接上新虚位节点
            tail_ = new_tail;                    // tail 前移
        }
        data_cond_.notify_one();                 // 锁外通知，缩短持锁时间
    }

    // 非阻塞出队：只锁 head
    std::shared_ptr<T> try_pop() {
        std::lock_guard<std::mutex> lk(head_mutex_);
        if (head_.get() == get_tail()) return std::shared_ptr<T>();  // 空
        std::unique_ptr<node> old = std::move(head_);
        head_ = std::move(old->next);
        return old->data;
    }

    // 阻塞出队：等有数据再取
    std::shared_ptr<T> wait_and_pop() {
        std::unique_lock<std::mutex> lk(head_mutex_);
        data_cond_.wait(lk, [this] { return head_.get() != get_tail(); });
        std::unique_ptr<node> old = std::move(head_);
        head_ = std::move(old->next);
        return old->data;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lk(head_mutex_);
        return head_.get() == get_tail();
    }
};