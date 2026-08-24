template<typename T>
class threadsafe_queue_ht {
    struct node {
        std::shared_ptr<T> data;
        std::unique_ptr<node> next;
    };

    std::mutex              head_mutex;
    std::unique_ptr<node>   head;
    std::mutex              tail_mutex;
    node*                   tail;
    std::condition_variable data_cond;

    node* get_tail() {
        std::lock_guard<std::mutex> lk(tail_mutex);
        return tail;
    }

public:
    threadsafe_queue_ht() : head(new node), tail(head.get()) {}

    threadsafe_queue_ht(const threadsafe_queue_ht&) = delete;
    threadsafe_queue_ht& operator=(const threadsafe_queue_ht&) = delete;

    // 入队：只锁 tail
    void push(T val) {
        std::shared_ptr<T> data = std::make_shared<T>(std::move(val));
        std::unique_ptr<node> p(new node);
        node* new_tail = p.get();
        {
            std::lock_guard<std::mutex> lk(tail_mutex);
            tail->data = data;          // 尾虚位节点装数据 → 变真节点
            tail->next = std::move(p);  // 接新虚位节点
            tail = new_tail;            // tail 前移
        }
        data_cond.notify_one();         // 锁外通知
    }

    // 非阻塞出队：只锁 head
    std::shared_ptr<T> try_pop() {
        std::lock_guard<std::mutex> lk(head_mutex);
        if (head.get() == get_tail()) return std::shared_ptr<T>();  // 空
        std::unique_ptr<node> old = std::move(head);
        head = std::move(old->next);
        return old->data;
    }

    // 阻塞出队：等有数据再取
    std::shared_ptr<T> wait_and_pop() {
        std::unique_lock<std::mutex> lk(head_mutex);
        data_cond.wait(lk, [this] { return head.get() != get_tail(); });
        std::unique_ptr<node> old = std::move(head);
        head = std::move(old->next);
        return old->data;
    }

    bool empty() {
        std::lock_guard<std::mutex> lk(head_mutex);
        return head.get() == get_tail();
    }
};