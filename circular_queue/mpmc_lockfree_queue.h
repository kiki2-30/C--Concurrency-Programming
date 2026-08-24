#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>
#include <thread>

// ============ 第一部分：Hazard Pointer 内存回收 ============
// 目的：解决 pop 之后"节点被释放复用"的 ABA 和 use-after-free 问题

constexpr unsigned MAX_HAZARD = 128;

struct HazardRecord {
    std::atomic<std::thread::id> id{};
    std::atomic<void*> ptr{nullptr};
};

HazardRecord hazards[MAX_HAZARD];   // 全局风险指针注册表

// 每个线程拿到一个专属的 hazard 槽位
std::atomic<void*>& thread_hazard_ptr() {
    thread_local HazardRecord* rec = nullptr;
    if (!rec) {
        for (unsigned i = 0; i < MAX_HAZARD; ++i) {
            std::thread::id empty;
            if (hazards[i].id.compare_exchange_strong(empty, std::this_thread::get_id())) {
                rec = &hazards[i];
                break;
            }
        }
        if (!rec) throw std::runtime_error("hazard pointers exhausted");
    }
    return rec->ptr;
}

// 检查有没有线程的 hazard 正指着 p
bool any_hazard(void* p) {
    for (auto& h : hazards) {
        if (h.ptr.load() == p) return true;
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

// ============ 第二部分：Michael-Scott 无锁队列 ============

template <typename T>
class LockFreeQueue {
    struct Node {
        std::shared_ptr<T> data;       // 哨兵节点的 data 为 nullptr
        std::atomic<Node*> next;
        Node() : data(nullptr), next(nullptr) {}
        Node(const T& v) : data(std::make_shared<T>(v)), next(nullptr) {}
    };

    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;

public:
    LockFreeQueue() {
        Node* dummy = new Node();      // 哨兵节点
        head_.store(dummy);
        tail_.store(dummy);
    }

    ~LockFreeQueue() {
        Node* n = head_.load();
        while (n) {
            Node* next = n->next.load();
            delete n;
            n = next;
        }
    }

    void push(const T& val) {
        Node* node = new Node(val);
        while (true) {
            Node* tail = tail_.load();
            Node* next = tail->next.load();
            if (tail != tail_.load()) continue;        // tail 变了，重试

            if (next == nullptr) {
                if (tail->next.compare_exchange_strong(next, node)) {
                    tail_.compare_exchange_strong(tail, node);  // 推进 tail，失败无妨
                    return;
                }
            } else {
                tail_.compare_exchange_strong(tail, next);      // 帮别人推 tail
            }
        }
    }

    bool pop(T& val) {
        std::atomic<void*>& hp = thread_hazard_ptr();  // 本线程的风险指针
        while (true) {
            Node* head = head_.load();
            hp.store(head);                              // ① 保护 head
            if (head != head_.load()) continue;

            Node* next = head->next.load();
            hp.store(next);                              // ② 保护 next

            Node* tail = tail_.load();
            if (head == tail) {
                if (next == nullptr) {                   // 真空
                    hp.store(nullptr);
                    return false;
                }
                tail_.compare_exchange_strong(tail, next);  // tail 落后，帮推
            } else {
                if (head_.compare_exchange_strong(head, next)) {
                    val = *(next->data);                 // ③ 读数据（next 受 hazard 保护）
                    hp.store(nullptr);
                    reclaim_later(new ReclaimNode(head));  // ④ 旧哨兵延迟回收
                    delete_without_hazards();
                    return true;
                }
            }
        }
    }
};