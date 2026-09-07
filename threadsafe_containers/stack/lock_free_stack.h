//void* 只干一件事：擦除类型，让 hazard pointer 能保护"任意类型"的节点。
#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>
#include <thread>

using namespace std;

constexpr int MAX_HAZARD = 128;

struct HazardRecord {
    atomic<thread::id> id{};   // 占用这个槽位的线程ID
    atomic<void*> ptr{nullptr};// 这个线程正在保护的指针地址

};

//全局
HazardRecord hazards[MAX_HAZARD];

//有槽位就返回 没有就抢一个返回
atomic<void*>& thread_hazard_ptr() {
    //线程第一次执行到这行代码时，rec 被初始化为 nullptr。
    //线程后续再执行到这行代码时，rec 保留上一次的值，不会再被初始化为 nullptr。
    thread_local HazardRecord* rec = nullptr;

    if(rec == nullptr) {
        for(int i = 0; i < MAX_HAZARD; i++) {
            thread::id empty;  // 默认构造，表示"无线程"
            // 尝试抢占这个槽位
            if(hazards[i].id.compare_exchange_strong(empty, this_thread::get_id())) {
                rec = &hazards[i];
                break;//抢一个就结束了
            }
        }
        if(rec == nullptr) throw runtime_error("");
    }
    return rec->ptr;
}

bool any_hazard(void* p) {
    for(auto& h : hazards) {
        if(h.ptr.load() == p) return true;
    }
    return false;
}

struct ReclaimNode {
    void* data;
    function<void(void*)> deleter;
    ReclaimNode* next;
    
    template <typename T>
    ReclaimNode(T* p) : data(p), deleter([](void* q) { delete static_cast<T*>(q);}), next(nullptr) {}
    ~ReclaimNode() {deleter(data);}

};

//全局待回收链表
atomic<ReclaimNode*>reclaim_list{nullptr};

void reclaim_later(ReclaimNode* n) {
    //把节点 n 用头插法挂到全局链表 reclaim_list 的最前面。
    n->next = reclaim_list.load();

    //头插法 多个线程同时挂的时候，用 CAS 保证不会乱。
    while (!reclaim_list.compare_exchange_weak(n->next, n)) {}

}

void delete_without_hazards() {
    // 1. 原子地“抢”走整个待回收清单，并把全局清单置空
    ReclaimNode* cur = reclaim_list.exchange(nullptr);
    
    // 2. 遍历抢到的清单
    while (cur) {
        ReclaimNode* next = cur->next;  // 先保存下一个节点
        
        // 3. 检查有没有任何线程的风险指针指向 data
        if (!any_hazard(cur->data)) {//// ⭐ 解决 ABA 的另一半
            delete cur;  // 没人用 → 真的释放！（触发 ~ReclaimNode）
        } else {
            reclaim_later(cur);  // 还有人用 → 重新挂回清单，下次再试
        }
        cur = next;
    }
}

// ============ 无锁栈 ============

template <typename T>
class lock_free_stack {
    struct node {
        std::shared_ptr<T> data;
        node* next;
        node(const T& v) : data(std::make_shared<T>(v)), next(nullptr) {}
    };

    std::atomic<node*> head_{nullptr};   // 栈顶，空栈为 nullptr

public:
    lock_free_stack() = default;
    lock_free_stack(const lock_free_stack&) = delete;
    lock_free_stack& operator=(const lock_free_stack&) = delete;

    ~lock_free_stack() {
        // 假设析构时已无并发访问
        node* n = head_.load();
        while (n) {
            node* next = n->next;
            delete n;
            n = next;
        }
    }

    // 压栈：不需要 hazard（不删节点）
    void push(const T& val) {
        node* new_node = new node(val);
        new_node->next = head_.load();
        while (!head_.compare_exchange_weak(new_node->next, new_node)) {}
    }

    // 弹栈：需要 hazard 保护 old_head
    std::shared_ptr<T> pop() {
        std::atomic<void*>& hp = thread_hazard_ptr();
        while (true) {
            node* old_head = head_.load();
            hp.store(old_head);                          // ① 保护栈顶 // ⭐ 解决 ABA 的关键行！
            if (old_head != head_.load()) continue;      // ② head 变了，重试

            if (old_head == nullptr) {                   // ③ 空栈
                hp.store(nullptr);
                return std::shared_ptr<T>();
            }

            if (head_.compare_exchange_weak(old_head, old_head->next)) {  // ④ CAS 弹栈
                std::shared_ptr<T> res = old_head->data;  // ⑤ 取数据（栈数据在 old_head 本身）
                hp.store(nullptr);                        // ⑥ 解除保护
                reclaim_later(new ReclaimNode(old_head)); // ⑦ 延迟回收
                delete_without_hazards();
                return res;
            }
        }
    }
};