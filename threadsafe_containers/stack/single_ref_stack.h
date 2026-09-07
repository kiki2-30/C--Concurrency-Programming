#pragma once
#include <atomic>
#include <memory>

// ============ 无锁栈：单引用计数回收 ============
// 与 ref_count_stack 思路相同，但引用计数直接放在 head 里存储的 ref_node 上：
//   - ref_node 自己带 _ref_count（存在 head 里，随 head 一起被 CAS）
//   - 实际节点 node 内部另有 _dec_count，记录"被多少线程引用"
// 两个计数归零时 delete 节点。
template <typename T>
class single_ref_stack {
private:
    struct ref_node;

    struct node {
        std::shared_ptr<T> _data;
        ref_node _next;
        std::atomic<int> _dec_count;   // 节点引用计数
        node(const T& data_) : _data(std::make_shared<T>(data_)), _dec_count(0) {}
    };

    struct ref_node {
        int _ref_count;      // 该 ref_node 的引用计数
        node* _node_ptr;     // 指向实际节点
        ref_node(const T& data_) : _node_ptr(new node(data_)), _ref_count(1) {}
        ref_node() : _node_ptr(nullptr), _ref_count(0) {}
    };

    std::atomic<ref_node> head;

public:
    // 必须把 head 初始化为"空节点"，否则 head.load() 会读到未初始化值
    single_ref_stack() {
        head.store(ref_node());
    }

    ~single_ref_stack() {
        while (pop());
    }

    void push(const T& data) {
        ref_node new_node(data);
        new_node._node_ptr->_next = head.load();
        while (!head.compare_exchange_weak(new_node._node_ptr->_next, new_node));
    }

    std::shared_ptr<T> pop() {
        ref_node old_head = head.load();
        for (;;) {
            // 1. 先把 head 里的引用计数 +1
            ref_node new_head;
            do {
                new_head = old_head;
                ++new_head._ref_count;
            } while (!head.compare_exchange_weak(old_head, new_head));

            old_head = new_head;

            node* const node_ptr = old_head._node_ptr;
            if (node_ptr == nullptr) {
                return std::shared_ptr<T>();   // 空栈
            }

            // 2. 尝试把头指针换成下一个节点
            if (head.compare_exchange_strong(old_head, node_ptr->_next)) {
                std::shared_ptr<T> res;
                res.swap(node_ptr->_data);
                int const increase_count = old_head._ref_count - 2;
                if (node_ptr->_dec_count.fetch_add(increase_count) == -increase_count) {
                    delete node_ptr;
                }
                return res;
            } else {
                // CAS 失败：head 已被别的线程更新，释放我这个线程的引用
                if (node_ptr->_dec_count.fetch_sub(1) == 1) {
                    delete node_ptr;
                }
            }
        }
    }
};
