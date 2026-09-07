#pragma once
#include <atomic>
#include <memory>

// ============ SPSC 无锁链表队列（单生产者单消费者）============
// 只有一个生产者、一个消费者，所以：
//   - head_ 只被消费者改、tail_ 只被生产者改，各自"冻结"，
//     不需要 CAS，直接用 store/load 即可
//   - pop 后可以直接 delete 节点（没有别的线程会碰它）
// 这是无锁队列里最简单的入门版（对应 day18 的 SinglePopPush）。

template <typename T>
class spsc_linked_queue {
    struct node {
        std::shared_ptr<T> data;
        node* next;
        node() : next(nullptr) {}
    };

    std::atomic<node*> head_;   // 只被消费者写
    std::atomic<node*> tail_;   // 只被生产者写

    node* pop_head() {
        node* old_head = head_.load();
        if (old_head == tail_.load()) return nullptr;  // 空
        head_.store(old_head->next);
        return old_head;
    }

public:
    spsc_linked_queue() : head_(new node), tail_(head_.load()) {}

    spsc_linked_queue(const spsc_linked_queue&) = delete;
    spsc_linked_queue& operator=(const spsc_linked_queue&) = delete;

    ~spsc_linked_queue() {
        while (node* old = head_.load()) {
            head_.store(old->next);
            delete old;
        }
    }

    void push(T val) {
        std::shared_ptr<T> data = std::make_shared<T>(std::move(val));
        node* p = new node;                  // 新哨兵（虚位节点）
        node* old_tail = tail_.load();
        old_tail->data.swap(data);           // 旧哨兵装上数据 → 变成真节点
        old_tail->next = p;                  // 接上新哨兵
        tail_.store(p);                      // tail 前移
    }

    std::shared_ptr<T> pop() {
        node* old_head = pop_head();
        if (!old_head) return std::shared_ptr<T>();   // 空
        std::shared_ptr<T> res = old_head->data;
        delete old_head;
        return res;
    }
};
