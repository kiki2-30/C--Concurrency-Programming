#include <iostream>
#include "../ring_queue/circular_queue.h"

int main() {
    CircularQueue<int, 4> q;   // 容量 4：最多存 4 个元素

    // 正常入队 4 个
    for (int i = 1; i <= 4; ++i) {
        std::cout << "push " << i << " : " << (q.push(i) ? "ok" : "FULL") << "\n";
    }
    // 第 5 个应该满
    std::cout << "push 5 : " << (q.push(5) ? "ok" : "FULL") << "\n";

    // 全部出队
    int v;
    while (q.pop(v)) {
        std::cout << "pop " << v << "\n";
    }
    // 再出队应该空
    std::cout << "pop again : " << (q.pop(v) ? "ok" : "EMPTY") << "\n";
    return 0;
}
