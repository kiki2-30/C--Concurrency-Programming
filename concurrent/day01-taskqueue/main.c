#include <stdio.h>
#include <stdlib.h>
#include "taskque.h"
// 示例任务函数，输出传入的整数值
void sampleTask(void* arg) {
    int* data = (int*) arg;
    printf("任务执行，值为: %d\n", *data);
    free(data);
}

int main(void) {
    system("chcp 65001 > nul");
    TaskQueue queue;
    initQueue(&queue);


    pthread_t worker;
    // 创建工作线程
    if (pthread_create(&worker, NULL, worker_thread, (void*)&queue) != 0) {
        perror("创建线程失败");
        exit(1);
    }

    // 模拟入队多个任务
    for (int i = 0; i < 10; i++) {
        int* arg = malloc(sizeof(int));
        if (!arg) {
            perror("内存分配失败");
            exit(1);
        }
        *arg = i;
        enqueue(&queue, sampleTask, arg);
        usleep(100000);  // 模拟任务产生的延时
    }

    // 等待一段时间，确保任务被处理
    sleep(2);

    // 通知工作线程退出
    pthread_mutex_lock(&queue.mutex);
    queue.stop = 1;
    pthread_cond_signal(&queue.cond);
    pthread_mutex_unlock(&queue.mutex);

    // 等待工作线程结束
    pthread_join(worker, NULL);

    // 清理同步资源
    destroyQueue(&queue);

    return 0;
}
