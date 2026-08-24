//
// Created by secon on 2025/4/2.
//

#ifndef TASKQUEUE_TASKQUE_H
#define TASKQUEUE_TASKQUE_H
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

// 定义任务函数类型：参数为 void*，无返回值
typedef void (*TaskFunction)(void*);

// 定义任务节点
typedef struct Task {
    TaskFunction func;
    void* arg;
    struct Task* next;
} Task;

// 定义任务队列，并添加线程同步所需的互斥量和条件变量，以及停止标志
typedef struct {
    Task* head;
    Task* tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int stop;  // 停止标志，1 表示退出工作线程
} TaskQueue;

// 初始化任务队列及同步原语
void initQueue(TaskQueue* queue);
// 销毁队列时释放同步资源
void destroyQueue(TaskQueue* queue);
// 线程安全的入队操作
void enqueue(TaskQueue* queue, TaskFunction func, void* arg);
// 线程安全的出队操作，从队列头取出任务
Task* dequeue(TaskQueue* queue);
// 工作线程函数：不断等待并执行队列中的任务
void* worker_thread(void* arg);
#endif //TASKQUEUE_TASKQUE_H
