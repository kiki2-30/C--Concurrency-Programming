#include <atomic>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

// ===== 不安全的 naive 版：pop 后直接 delete 旧节点（无 hazard 保护）=====
template <typename T>
class NaiveQueue {
    struct Node {
        std::shared_ptr<T> data;
        std::atomic<Node*> next;
        Node() : data(nullptr), next(nullptr) {}
        Node(const T& v) : data(std::make_shared<T>(v)), next(nullptr) {}
    };
    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;

public:
    NaiveQueue() {
        Node* d = new Node();
        head_.store(d);
        tail_.store(d);
    }
    ~NaiveQueue() {
        Node* n = head_.load();
        while (n) {
            Node* nx = n->next.load();
            delete n;
            n = nx;
        }
    }

    void push(const T& val) {
        Node* node = new Node(val);
        while (true) {
            Node* tail = tail_.load();
            Node* next = tail->next.load();
            if (tail != tail_.load()) continue;
            if (next == nullptr) {
                if (tail->next.compare_exchange_strong(next, node)) {
                    tail_.compare_exchange_strong(tail, node);
                    return;
                }
            } else {
                tail_.compare_exchange_strong(tail, next);
            }
        }
    }

    bool pop(T& val) {
        while (true) {
            Node* head = head_.load();
            Node* tail = tail_.load();
            Node* next = head->next.load();
            if (head != head_.load()) continue;
            if (head == tail) {
                if (next == nullptr) return false;
                tail_.compare_exchange_strong(tail, next);
            } else {
                if (head_.compare_exchange_strong(head, next)) {
                    val = *(next->data);
                    delete head;   // ❌ 直接删除：别的线程可能还握着 head 的指针
                    return true;
                }
            }
        }
    }
};

int main() {
    const int N = 100000;
    const int P = 4, C = 4;
    NaiveQueue<int> q;
    std::vector<std::atomic<int>> seen(N);
    std::vector<std::thread> threads;

    for (int p = 0; p < P; ++p) {
        threads.emplace_back([&, p] {
            for (int i = p; i < N; i += P) q.push(i);
        });
    }
    std::atomic<int> slots{0};
    for (int c = 0; c < C; ++c) {
        threads.emplace_back([&] {
            int val;
            while (true) {
                int i = slots.fetch_add(1);
                if (i >= N) break;
                while (!q.pop(val)) std::this_thread::yield();
                seen[val].fetch_add(1);
            }
        });
    }
    for (auto& t : threads) t.join();

    int errors = 0;
    for (int i = 0; i < N; ++i) {
        if (seen[i].load() != 1) ++errors;
    }
    std::cout << "errors = " << errors << "\n";
    return errors == 0 ? 0 : 1;
}
