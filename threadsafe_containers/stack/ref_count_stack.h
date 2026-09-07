#pragma once
#include <atomic>
#include <memory>

// ============ 无锁栈：节点引用计数回收（counted_node_ptr 方案）============
// 难点：pop 把节点摘下后不能立即 delete，因为可能有别的线程还握着这个节点。
// 本方案用"两套计数"判断节点是否真的没人用了：
//   - head 里存 counted_node_ptr{external_count, ptr}：
//     external_count 记录"当前有多少线程正持有这个 head 快照"；
//   - 节点内部 internal_count：记录"除了头指针外，还有多少线程引用该节点"。
//   只有当 external_count 和 internal_count 都归零，节点才能 delete。
template <typename T>
class ref_count_stack {
private:
    struct count_node;

    struct counted_node_ptr {
        int external_count;   // 外部引用计数
        count_node* ptr;      // 节点地址
    };

    struct count_node {
        std::shared_ptr<T> data;
        std::atomic<int> internal_count;   // 内部引用计数
        counted_node_ptr next;
        count_node(const T& data_) : data(std::make_shared<T>(data_)), internal_count(0) {}
    };

    std::atomic<counted_node_ptr> head;

    // 增加 head 的外部引用计数：表示"我这个线程也要用 head"
    void increase_head_count(counted_node_ptr& old_counter) {
        counted_node_ptr new_counter;
        do {
            new_counter = old_counter;
            ++new_counter.external_count;
        } while (!head.compare_exchange_strong(old_counter, new_counter,
            std::memory_order_acquire, std::memory_order_relaxed));
        old_counter.external_count = new_counter.external_count;
    }

public:
    ref_count_stack() {
        counted_node_ptr head_node_ptr;
        head_node_ptr.external_count = 0;
        head_node_ptr.ptr = nullptr;
        head.store(head_node_ptr);
    }

    ~ref_count_stack() {
        while (pop());
    }

    void push(const T& data) {
        counted_node_ptr new_node;
        new_node.ptr = new count_node(data);
        new_node.external_count = 1;
        new_node.ptr->next = head.load(std::memory_order_relaxed);
        while (!head.compare_exchange_weak(new_node.ptr->next, new_node,
            std::memory_order_release, std::memory_order_relaxed));
    }

    std::shared_ptr<T> pop() {
        counted_node_ptr old_head = head.load(std::memory_order_relaxed);
        for (;;) {
            increase_head_count(old_head);
            count_node* const ptr = old_head.ptr;
            if (!ptr) return std::shared_ptr<T>();   // 空栈

            // 尝试把头指针从 old_head 换成下一个节点
            if (head.compare_exchange_strong(old_head, ptr->next, std::memory_order_relaxed)) {
                std::shared_ptr<T> res;
                res.swap(ptr->data);
                // 把本次增加的 external_count 折算回 internal_count
                int const count_increase = old_head.external_count - 2;
                if (ptr->internal_count.fetch_add(count_increase, std::memory_order_release) == -count_increase) {
                    delete ptr;   // 内外计数都归零 → 真正释放
                }
                return res;
            } else if (ptr->internal_count.fetch_add(-1, std::memory_order_acquire) == 1) {
                // CAS 失败：说明 head 已被别的线程更新，释放我这个线程的引用
                ptr->internal_count.load(std::memory_order_acquire);
                delete ptr;
            }
        }
    }
};
