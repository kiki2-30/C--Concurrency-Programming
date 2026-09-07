# threadsafe_containers

线程安全数据结构的"有锁 → 无锁"演变合集。按数据结构类型分三个文件夹，每个文件夹内的文件按演变顺序排列（序号越小越基础）。

## 目录结构

```
threadsafe_containers/
├── stack/                        # 栈（LIFO）
│   ├── mutex_stack.h             # ① 有锁：粗粒度锁（std::mutex）
│   ├── waitable_stack.h          # ② 有锁 + 条件变量（wait_and_pop 阻塞）
│   ├── lock_free_stack.h         # ③ 无锁：Treiber 栈 + 危险指针（hazard pointer）
│   ├── lock_free_stack_threadcount.h  # ④ 无锁：threads_in_pop 计数法
│   ├── ref_count_stack.h         # ⑤ 无锁：节点引用计数（counted_node_ptr）
│   └── single_ref_stack.h        # ⑥ 无锁：单引用计数
├── queue/                        # 队列（FIFO，链表/普通队列）
│   ├── mutex_queue.h             # ① 有锁：std::queue + mutex + cv
│   ├── threadsafe_queue_ht.h     # ② 有锁：链表 + head/tail 双锁（细粒度）
│   ├── spsc_linked_queue.h       # ③ 无锁 SPSC：单生产者单消费者链表
│   ├── mpmc_lockfree_queue.h     # ④ 无锁 MPMC：Michael-Scott + 危险指针
│   └── mpmc_refcount_queue.h     # ⑤ 无锁 MPMC：Michael-Scott + 引用计数
├── ring_queue/                   # 环形队列（有界数组）
│   ├── circular_queue.h           # ① 有锁：mutex
│   ├── spinlock_circular_queue.h  # ② 自旋锁（atomic_flag 忙等）
│   ├── spsc_lockfree_queue.h      # ③ 无锁 SPSC：留空位 + acquire/release
│   └── mpmc_ring_queue.h          # ④ 无锁 MPMC：Vyukov（每槽位 sequence）
└── tests/                        # 每个实现一个测试，命名对应 *_test.cpp
```

## 核心知识点

- **有锁栈/队列**：难点是"判空 + 取值 + 弹出"要合成一次加锁操作，避免 `top()+pop()` 分离的竞态；用条件变量解决"空了忙等"。
- **无锁结构**：难点不在 push/pop 的 CAS，而在**怎么安全回收被弹出的节点**，两条主流路线：
  - **危险指针**（hazard pointer）：线程声明"我正用着这个指针"，删除前检查没人用。
  - **引用计数**：给节点/头指针配计数，两个计数归零才真正 delete。
- **环形队列**：槽位原地复用、不需要回收，难点在"怎么判断槽位就绪"（满/空），正确做法是每槽位一个 `sequence` 序号（Vyukov）。

## 编译与测试

每个测试都是独立的 `.cpp`，`main` 里跑多线程压测，返回 0 表示通过。

| 文件 | 编译参数 |
|------|----------|
| 大多数测试 | `g++ -std=c++17 -pthread -O2 xxx_test.cpp -o xxx` |
| `ref_count_stack_test.cpp` / `single_ref_stack_test.cpp` / `mpmc_refcount_queue_test.cpp` | 额外加 `-latomic`（16 字节原子操作需要 libatomic） |
| `spinlock_circular_queue_test.cpp` | 用 `-std=c++20`（头文件用到 `std::construct_at`） |

示例：

```bash
cd tests
g++ -std=c++17 -pthread -O2 mutex_queue_test.cpp -o mutex_queue_test && ./mutex_queue_test
g++ -std=c++17 -pthread -O2 ref_count_stack_test.cpp -latomic -o ref_count_stack_test && ./ref_count_stack_test
g++ -std=c++20 -pthread -O2 spinlock_circular_queue_test.cpp -o spinlock_circular_queue_test && ./spinlock_circular_queue_test
```

> 注：`naive_michael_scott_test.cpp` 是"不加保护直接 delete"的反例，运行会**段错误**，这是预期行为，用来演示 use-after-free。
