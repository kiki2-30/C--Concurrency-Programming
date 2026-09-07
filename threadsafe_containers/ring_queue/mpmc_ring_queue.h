#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

// ============ Vyukov 有界 MPMC 环形队列（多生产者多消费者）============
//
// 思路：给【每个槽位】配一个独立的 sequence 序号，靠序号判断该槽位是否就绪。
// 不要用"一个全局游标代表写到哪了"——多生产者写序不一致时，那种做法会乱序，
// 导致消费者读到没写完的数据。
//
//   初始化：槽位 i 的 seq = i，表示"空，等待第 i 轮写入"。
//
//   push：CAS 抢占全局写游标 enqueue_pos_，抢到的位置记为 pos；
//         seq == pos   → 该槽位空闲且轮到我，写入后 seq = pos+1（标记可读）；
//         seq <  pos   → 该槽位还没被上一轮消费完，队列已满。
//
//   pop ：CAS 抢占全局读游标 dequeue_pos_，抢到的位置记为 pos；
//         seq == pos+1 → 该槽位已有数据，读走后 seq = pos+Cap（标记可再写）；
//         seq <  pos+1 → 队列空。
//
//   为什么不会 ABA / 乱序：seq 单调递增、永不重复；每个槽位只用自己的 seq
//   与全局 pos 对齐，生产者和消费者各抢各的游标，互不干扰。
//
// 要求：
//   - Cap 必须是 2 的幂（例如 1024），用位与代替取模
//   - T 需可默认构造、可拷贝赋值 / 移动赋值
// 注意：这是"固定容量"队列，满了 push 返回 false（调用方自行重试）。
template <typename T, size_t Cap>
class MpmcRingQueue {
    static_assert(Cap > 0 && (Cap & (Cap - 1)) == 0, "Cap must be a power of 2");

    struct Cell {
        std::atomic<size_t> seq;   // 槽位序号：判空/判满/判就绪全靠它
        T data;                    // 存的数据
    };

    std::vector<Cell> buffer_;             // Cap 个槽位
    static constexpr size_t kMask = Cap - 1;
    std::atomic<size_t> enqueue_pos_{0};   // 下一个要写的位置（全局游标）
    std::atomic<size_t> dequeue_pos_{0};   // 下一个要读的位置（全局游标）

public:
    MpmcRingQueue() : buffer_(Cap) {
        // 初始：槽位 i 的 seq = i，表示"空，等待第 i 轮写入"
        for (size_t i = 0; i < Cap; ++i) {
            buffer_[i].seq.store(i, std::memory_order_relaxed);
        }
    }

    MpmcRingQueue(const MpmcRingQueue&) = delete;
    MpmcRingQueue& operator=(const MpmcRingQueue&) = delete;

    bool push(const T& val) {
        Cell* cell = nullptr;
        size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
        for (;;) {
            cell = &buffer_[pos & kMask];
            size_t seq = cell->seq.load(std::memory_order_acquire);
            intptr_t dif = (intptr_t)seq - (intptr_t)pos;
            if (dif == 0) {
                // 槽位空闲且正好轮到我 → CAS 抢占 enqueue_pos_
                if (enqueue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    break;
            } else if (dif < 0) {
                return false;   // 满：这个槽位还没被上一轮消费完
            } else {
                pos = enqueue_pos_.load(std::memory_order_relaxed);  // 别人抢先了，刷新 pos
            }
        }
        cell->data = val;
        cell->seq.store(pos + 1, std::memory_order_release);  // 标记"数据已就绪，可读"
        return true;
    }

    bool pop(T& val) {
        Cell* cell = nullptr;
        size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
        for (;;) {
            cell = &buffer_[pos & kMask];
            size_t seq = cell->seq.load(std::memory_order_acquire);
            intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);
            if (dif == 0) {
                // 槽位有数据且轮到我 → CAS 抢占 dequeue_pos_
                if (dequeue_pos_.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
                    break;
            } else if (dif < 0) {
                return false;   // 空
            } else {
                pos = dequeue_pos_.load(std::memory_order_relaxed);
            }
        }
        val = std::move(cell->data);
        // seq 加 Cap：绕一圈回到该槽位时，push 才能再次写入
        cell->seq.store(pos + Cap, std::memory_order_release);
        return true;
    }
};
