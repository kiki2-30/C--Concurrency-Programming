
//快照只会"滞后"，滞后导致"保守误判满/空"，保守只会放弃一次机会、不会写坏数据。
//绕环靠"单调递增 + 取模"天然正确。所以 SPSC 在边界处是安全的。
//head_ 在 pop 里没人改它 因为只有一个消费者，head_本身就是"冻结"的，所以不用再快照。
//head_ 在 pop 里不会被其他线程改——它唯一的写者就是正在执行 pop 的这个消费者自己。
//而读 data_[head_] 发生在 head_.store(...) 之前，所以读到的是稳定不变的旧值，不用快照。
#include <atomic>
#include <vector>

template <typename T, size_t Cap>
class SpscQueue {
public:
    SpscQueue() : data_(Cap + 1) {}   // 留一个空位


    bool push(const T& val) {
        // 1. 用 acquire 读 head_（别人写的，判断满）
        size_t head = head_.load(std::memory_order_acquire);

        // 2. 满：(tail_ + 1) % size == head → return false
        if((tail_ + 1) % data_.size() == head) {
            return false;
        }
        // 3. 写数据：data_[tail_] = val

        data_[tail_] = val;

        // 4. 用 release 推进 tail_

        tail_.store((tail_ + 1) % data_.size(), std::memory_order_release);
        return true;
    }

    bool pop(T& val) {
        // 1. 用 acquire 读 tail_（别人写的，判断空）
        size_t tail = tail_.load(std::memory_order_acquire);
        // 2. 空：head_ == tail → return false
        if(head_ == tail) {
            return false;
        }
        // 3. 读数据：val = std::move(data_[head_])   // SPSC 只有一个消费者，move 安全
        val = std::move(data_[head_]);
        // 4. 用 release 推进 head_
        head_.store((head_ + 1) % data_.size(), std::memory_order_release);
        return true;
    }

private:
    std::vector<T>          data_;
    std::atomic<size_t>     head_{0};   // 只有消费者写
    std::atomic<size_t>     tail_{0};   // 只有生产者写
};