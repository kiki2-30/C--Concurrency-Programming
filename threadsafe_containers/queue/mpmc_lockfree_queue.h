#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>
#include <thread>

// ============ Michael-Scott 无锁 MPMC 队列 + 危险指针回收 ============
// 经典的 Michael-Scott 队列（哨兵节点 + CAS），节点回收用危险指针（hazard pointer）。
// 注意：队列每个线程需要【两个】hazard 槽位——pop 时要同时保护 head 和 head->next，
// 因为"读完 head->next"到"把 next 存进 hazard"之间，别的线程可能已经把 next 弹走并释放。
// 单个槽位会在这段窗口出现 use-after-free（栈才只需要 1 个槽位）。

// ---- 危险指针回收机制（收在 namespace 里，避免污染全局）----
namespace hazard {

constexpr unsigned MAX_HAZARD = 128;   // 每线程 2 个槽位 → 最多 64 个线程

struct Record {
    std::atomic<std::thread::id> id{};
    std::atomic<void*> ptr{nullptr};
};

Record records[MAX_HAZARD];   // 全局风险指针注册表

// 返回本线程的第 slot 个（0 或 1）槽位
std::atomic<void*>& thread_hazard_ptr(unsigned slot) {
    thread_local Record* recs[2] = {nullptr, nullptr};
    if (!recs[slot]) {
        for (unsigned i = 0; i < MAX_HAZARD; ++i) {
            std::thread::id empty;
            if (records[i].id.compare_exchange_strong(empty, std::this_thread::get_id())) {
                recs[slot] = &records[i];
                break;
            }
        }
        if (!recs[slot]) throw std::runtime_error("hazard pointers exhausted");
    }
    return recs[slot]->ptr;
}

// 检查有没有线程的 hazard 正指着 p
bool any_hazard(void* p) {
    for (auto& r : records) {
        if (r.ptr.load() == p) return true;
    }
    return false;
}

// 延迟回收节点
struct ReclaimNode {
    void* data;
    std::function<void(void*)> deleter;
    ReclaimNode* next;
    template <typename T>
    ReclaimNode(T* p) : data(p), deleter([](void* q){ delete static_cast<T*>(q); }), next(nullptr) {}
    ~ReclaimNode() { deleter(data); }
};

std::atomic<ReclaimNode*> reclaim_list{nullptr};

void reclaim_later(ReclaimNode* n) {
    n->next = reclaim_list.load();
    while (!reclaim_list.compare_exchange_weak(n->next, n)) {}
}

// 批量回收：只有"没有任何线程的 hazard 指向它"的节点才真正 delete
void delete_without_hazards() {
    ReclaimNode* cur = reclaim_list.exchange(nullptr);
    while (cur) {
        ReclaimNode* next = cur->next;
        if (!any_hazard(cur->data)) {
            delete cur;                 // 没人引用 → 安全释放
        } else {
            reclaim_later(cur);         // 还有人用 → 放回列表，下轮再试
        }
        cur = next;
    }
}

} // namespace hazard

// ---- 无锁队列本体 ----
template <typename T>
class mpmc_lockfree_queue {
    struct node {
        std::shared_ptr<T> data;       // 哨兵节点的 data 为 nullptr
        std::atomic<node*> next;
        node() : data(nullptr), next(nullptr) {}
        node(const T& v) : data(std::make_shared<T>(v)), next(nullptr) {}
    };

    std::atomic<node*> head_;
    std::atomic<node*> tail_;

public:
    mpmc_lockfree_queue() {
        node* dummy = new node();      // 哨兵节点
        head_.store(dummy);
        tail_.store(dummy);
    }

    ~mpmc_lockfree_queue() {
        node* n = head_.load();
        while (n) {
            node* next = n->next.load();
            delete n;
            n = next;
        }
    }

    void push(const T& val) {
        node* new_node = new node(val);
        while (true) {
            node* tail = tail_.load();
            node* next = tail->next.load();
            if (tail != tail_.load()) continue;         // tail 变了，重试

            if (next == nullptr) {
                if (tail->next.compare_exchange_strong(next, new_node)) {
                    tail_.compare_exchange_strong(tail, new_node);  // 推进 tail，失败无妨
                    return;
                }
            } else {
                tail_.compare_exchange_strong(tail, next);          // 帮别人推 tail
            }
        }
    }

    bool pop(T& val) {
        std::atomic<void*>& hp_head = hazard::thread_hazard_ptr(0);
        std::atomic<void*>& hp_next = hazard::thread_hazard_ptr(1);
        while (true) {
            node* head = head_.load();
            hp_head.store(head);                         // ① 保护 head
            if (head != head_.load()) continue;          // head 变了，重试

            node* next = head->next.load();              // ② 读 head->next（head 受保护）
            hp_next.store(next);                         // ③ 保护 next（head 和 next 同时受保护）
            if (head != head_.load()) continue;          // ④ 再确认 head 没变（关键！）

            node* tail = tail_.load();
            if (head == tail) {
                if (next == nullptr) {                   // 真空
                    hp_head.store(nullptr);
                    hp_next.store(nullptr);
                    return false;
                }
                tail_.compare_exchange_strong(tail, next);  // tail 落后，帮推
            } else {
                if (head_.compare_exchange_strong(head, next)) {
                    val = *(next->data);                 // ⑤ 读数据（next 受保护）
                    hp_head.store(nullptr);
                    hp_next.store(nullptr);
                    hazard::reclaim_later(new hazard::ReclaimNode(head));  // ⑥ 旧哨兵延迟回收
                    hazard::delete_without_hazards();
                    return true;
                }
            }
        }
    }
};