#pragma once
#include <atomic>
#include <cstdint>
#include <memory>

// ============ 无锁 MPMC 队列：引用计数回收 ============
// Michael-Scott 队列，但用"引用计数"判断节点何时能 delete（区别于
// mpmc_lockfree_queue.h 的危险指针方案）：
//   - 每个节点带一个 count：internal_count（内部引用，30 bit）
//     + external_counters（外部引用，2 bit）；
//   - head/tail 各持一个 counted_node_ptr{external_count, ptr}，
//     读取前先把 external_count +1，用完再折算回 internal_count；
//   - 只有当 internal_count 和 external_counters 都归零，节点才 delete。
template <typename T>
class mpmc_refcount_queue {
private:
    struct node_counter {
        unsigned internal_count : 30;     // 内部引用计数
        unsigned external_counters : 2;   // 外部引用计数（最多 3）
    };

    struct node;

    struct alignas(16) counted_node_ptr {
        node* ptr;
        // 用 8 字节的 intptr_t：让整个结构正好 16 字节、无 padding。
        // 否则 int + node* 之间会有 4 字节未初始化的 padding，
        // 16 字节 CAS（memcmp）会因 padding 不一致而误判失败。
        intptr_t external_count;
        counted_node_ptr() : ptr(nullptr), external_count(0) {}
    };

    struct node {
        std::atomic<T*> data;               // 存 T*，pop 时 exchange 取走
        std::atomic<node_counter> count;
        std::atomic<counted_node_ptr> next;

        node(int external_count = 2) {
            data.store(nullptr);          // 必须初始化，否则 atomic<T*> 是未初始化值
            node_counter new_count;
            new_count.internal_count = 0;
            new_count.external_counters = external_count;
            count.store(new_count);

            counted_node_ptr node_ptr;
            node_ptr.ptr = nullptr;
            node_ptr.external_count = 0;
            next.store(node_ptr);
        }

        void release_ref() {
            node_counter old_counter = count.load(std::memory_order_relaxed);
            node_counter new_counter;
            do {
                new_counter = old_counter;
                --new_counter.internal_count;
            } while (!count.compare_exchange_strong(old_counter, new_counter,
                std::memory_order_acquire, std::memory_order_relaxed));
            if (!new_counter.internal_count && !new_counter.external_counters) {
                delete this;
            }
        }
    };

    std::atomic<counted_node_ptr> head;
    std::atomic<counted_node_ptr> tail;

    static void increase_external_count(std::atomic<counted_node_ptr>& counter,
                                        counted_node_ptr& old_counter) {
        counted_node_ptr new_counter;
        do {
            new_counter = old_counter;
            ++new_counter.external_count;
        } while (!counter.compare_exchange_strong(old_counter, new_counter,
            std::memory_order_acquire, std::memory_order_relaxed));
        old_counter.external_count = new_counter.external_count;
    }

    static void free_external_counter(counted_node_ptr& old_node_ptr) {
        node* const ptr = old_node_ptr.ptr;
        intptr_t const count_increase = old_node_ptr.external_count - 2;
        node_counter old_counter = ptr->count.load(std::memory_order_relaxed);
        node_counter new_counter;
        do {
            new_counter = old_counter;
            --new_counter.external_counters;
            new_counter.internal_count += count_increase;
        } while (!ptr->count.compare_exchange_strong(old_counter, new_counter,
            std::memory_order_acquire, std::memory_order_relaxed));
        if (!new_counter.internal_count && !new_counter.external_counters) {
            delete ptr;
        }
    }

    void set_new_tail(counted_node_ptr& old_tail, counted_node_ptr const& new_tail) {
        node* const current_tail_ptr = old_tail.ptr;
        // 只有一个线程能成功把 tail 推进到 new_tail，失败的线程帮忙释放引用
        while (!tail.compare_exchange_weak(old_tail, new_tail) &&
               old_tail.ptr == current_tail_ptr);
        if (old_tail.ptr == current_tail_ptr)
            free_external_counter(old_tail);
        else
            current_tail_ptr->release_ref();
    }

public:
    mpmc_refcount_queue() {
        counted_node_ptr new_next;
        new_next.ptr = new node();
        new_next.external_count = 1;
        tail.store(new_next);
        head.store(new_next);
    }

    ~mpmc_refcount_queue() {
        while (pop());
        auto head_node = head.load();
        delete head_node.ptr;
    }

    void push(const T& val) {
        std::unique_ptr<T> new_data(new T(val));
        counted_node_ptr new_next;
        new_next.ptr = new node;
        new_next.external_count = 1;
        counted_node_ptr old_tail = tail.load();

        for (;;) {
            increase_external_count(tail, old_tail);
            T* old_data = nullptr;
            if (old_tail.ptr->data.compare_exchange_strong(old_data, new_data.get())) {
                // 成功把数据写入当前尾节点
                counted_node_ptr old_next;
                counted_node_ptr now_next = old_tail.ptr->next.load();
                if (!old_tail.ptr->next.compare_exchange_strong(old_next, new_next)) {
                    delete new_next.ptr;       // 别人已经链了新节点，丢弃自己这个
                    new_next = old_next;
                }
                set_new_tail(old_tail, new_next);
                new_data.release();
                break;
            } else {
                // 尾节点的 data 已被别人占用，帮它推进
                counted_node_ptr old_next;
                if (old_tail.ptr->next.compare_exchange_strong(old_next, new_next)) {
                    old_next = new_next;
                    new_next.ptr = new node;
                }
                set_new_tail(old_tail, old_next);
            }
        }
    }

    std::unique_ptr<T> pop() {
        counted_node_ptr old_head = head.load(std::memory_order_relaxed);
        for (;;) {
            increase_external_count(head, old_head);
            node* const ptr = old_head.ptr;
            if (ptr == tail.load().ptr) {
                ptr->release_ref();            // 头尾相等 → 队列空
                return std::unique_ptr<T>();
            }
            counted_node_ptr next = ptr->next.load();
            if (head.compare_exchange_strong(old_head, next)) {
                T* const res = ptr->data.exchange(nullptr);
                free_external_counter(old_head);
                return std::unique_ptr<T>(res);
            }
            ptr->release_ref();
        }
    }
};
