#pragma once
#include<csignal>
#include <atomic>
#include <thread>
#include <queue>
#include <iostream>
#include <condition_variable>
#include <mutex>

extern void deadlockdemo();
extern void lockdemo();
extern void reference_invalid();
extern void reference_sharedptr();
extern void shallow_copy();
extern void shallow_copy2();
extern void normal_copy();

extern std::atomic<bool>  b_stop;


class ProductConsumerMgr {
public:
	ProductConsumerMgr(){
		_consumer = std::thread([this]() {
			while (!b_stop) {
				std::unique_lock<std::mutex> lock(_mtx);
				_consume_cv.wait(lock, [this]() {
					if (b_stop) {
						return true;
					}
					if (_data_que.empty()) {
						return false;
						}
					return true;
					});

				if (b_stop) {
					return ;
				}
				int data = _data_que.front();
				_data_que.pop();
				std::cout << "pop data is " << data << std::endl;
				lock.unlock();
				_producer_cv.notify_one();
				}
			});

		_producer = std::thread([this]() {
			int data = 0;
			while (!b_stop) {
				std::unique_lock<std::mutex> lock(_mtx);
				_producer_cv.wait(lock, [this]() {
					if (b_stop) {
						return true;
					}
					if (_data_que.size() > 100) {
						return false;
					}
					return true;
					});
				if (b_stop) {
					return ;
				}
				_data_que.push(++data);
				std::cout << "push data is " << data << std::endl;
				lock.unlock();
				_consume_cv.notify_one();
			}
			});

	}
	~ProductConsumerMgr(){
		_consume_cv.notify_one();
		_producer_cv.notify_one();
		_producer.join();
		_consumer.join();
	}
private:
	std::mutex _mtx;
	std::condition_variable _consume_cv;
	std::condition_variable _producer_cv;
	std::queue<int> _data_que;
	std::thread _consumer;
	std::thread _producer;
};

extern void TestProducerConsumer();
extern void  TestSteal();