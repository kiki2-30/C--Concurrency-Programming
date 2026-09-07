#pragma once
#include <atomic>
#include <memory>

// ============ 无锁栈：threads_in_pop 计数法 ============
// 另一种"安全回收节点"的方案（区别于危险指针、节点引用计数）：
//   - 用原子计数器 threads_in_pop_ 记录"当前正在 pop 的线程数"；
//   - pop 摘下的节点不立即删，先挂到 to_be_deleted_ 待删链表；
//   - 只有"最后一个离开 pop 的线程"（计数变 0）才批量清空待删链表。
// 这样保证删节点时，不会有别的线程还在 pop 里用着它。
template <typename T>
class threadcount_stack {
private:
    struct node {
        std::shared_ptr<T> data;
        node* next;
        node(const T& data_) : data(std::make_shared<T>(data_)) {}
    };

    std::atomic<node*> head_{nullptr};
    std::atomic<node*> to_be_deleted_{nullptr};   // 待删链表
    std::atomic<int> threads_in_pop_{0};          // 正在 pop 的线程数

public:
    threadcount_stack() = default;

    threadcount_stack(const threadcount_stack&) = delete;
    threadcount_stack& operator=(const threadcount_stack&) = delete;

    ~threadcount_stack() {
        // 假设析构时已无并发访问
        node* n = head_.load();
        while (n) {
            node* next = n->next;
            delete n;
            n = next;
        }
    }

    void push(const T& data) {
        node* const new_node = new node(data);
        new_node->next = head_.load(std::memory_order_relaxed);
        while (!head_.compare_exchange_weak(new_node->next, new_node));
    }

    std::shared_ptr<T> pop() {
        ++threads_in_pop_;                          // ① 进入 pop
        node* old_head = nullptr;
        do {
            old_head = head_.load();
            if (old_head == nullptr) {              // 空栈
                --threads_in_pop_;
                return std::shared_ptr<T>();
            }
        } while (!head_.compare_exchange_weak(old_head, old_head->next));  // ② CAS 摘下头

        std::shared_ptr<T> res;
        res.swap(old_head->data);                   // ③ 取数据
        try_reclaim(old_head);                      // ④ 延迟回收
        return res;
    }

private:
    void try_reclaim(node* old_head) {
        if (threads_in_pop_ == 1) {                 // 只有我一个在 pop
            node* nodes_to_delete = to_be_deleted_.exchange(nullptr);  // 抢走待删链表
            if (--threads_in_pop_ == 0) {           // 我是最后一个离开的
                delete_nodes(nodes_to_delete);      // 批量删除
            } else if (nodes_to_delete) {
                chain_pending_nodes(nodes_to_delete);   // 还有人，挂回去
            }
            delete old_head;                        // 现在安全删除当前节点
        } else {                                    // 还有别的线程在 pop
            chain_pending_node(old_head);           // 挂到待删链表
            --threads_in_pop_;
        }
    }

    static void delete_nodes(node* nodes) {
        while (nodes) {
            node* next = nodes->next;
            delete nodes;
            nodes = next;
        }
    }

    void chain_pending_node(node* n) {
        chain_pending_nodes(n, n);
    }

    void chain_pending_nodes(node* first, node* last) {
        last->next = to_be_deleted_.load();
        while (!to_be_deleted_.compare_exchange_weak(last->next, first));
    }

    void chain_pending_nodes(node* nodes) {
        node* last = nodes;
        while (node* const next = last->next) {
            last = next;
        }
        chain_pending_nodes(nodes, last);
    }
};
