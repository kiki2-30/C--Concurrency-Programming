// day22-ThreadPool.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "ParallenForeach.h"
#include "SimpleThreadPool.h"
#include "NotifyThreadPool.h"
#include "QuickSort.h"
#include "binddemo.h"
#include "ThreadSafeQue.h"

void TestParallenForEach() {

    std::vector<int> nvec;
    for (int i = 0; i < 26; i++) {
        nvec.push_back(i);
    }

    parallel_for_each(nvec.begin(), nvec.end(), [](int& i) {
        i *= i;
        });

    for (int i = 0; i < nvec.size(); i++) {
        std::cout << nvec[i] << " ";
    }

    std::cout << std::endl;
}

void TestRecursiveForEach() {

    std::vector<int> nvec;
    for (int i = 0; i < 26; i++) {
        nvec.push_back(i);
    }

    recursion_for_each(nvec.begin(), nvec.end(), [](int& i) {
        i *= i;
        });

    for (int i = 0; i < nvec.size(); i++) {
        std::cout << nvec[i] << " ";
    }

    std::cout << std::endl;
    
}

void TestSimpleThread() {
	std::vector<int> nvec;
	for (int i = 0; i < 26; i++) {
		nvec.push_back(i);
	}

    simple_for_each(nvec.begin(), nvec.end(), [](int& i) {
		i *= i;
		});

	for (int i = 0; i < nvec.size(); i++) {
		std::cout << nvec[i] << " ";
	}

	std::cout << std::endl;
}

void TestFutureThread() {
	std::vector<int> nvec;
	for (int i = 0; i < 26; i++) {
		nvec.push_back(i);
	}

	future_for_each(nvec.begin(), nvec.end(), [](int& i) {
		i *= i;
		});

	for (int i = 0; i < nvec.size(); i++) {
		std::cout << nvec[i] << " ";
	}

	std::cout << std::endl;
}

void TestNotifyThread() {
	std::vector<int> nvec;
	for (int i = 0; i < 26; i++) {
		nvec.push_back(i);
	}

	notify_for_each(nvec.begin(), nvec.end(), [](int& i) {
		i *= i;
		});

	for (int i = 0; i < nvec.size(); i++) {
		std::cout << nvec[i] << " ";
	}

	std::cout << std::endl;
    
}

void TestQuickSort() {
	std::list<int> nlist = { 6,1,0,5,2,9,11 };

	auto sortlist = parallel_quick_sort<int>(nlist);

	for (auto & value : sortlist) {
		std::cout << value << " ";
	}

	std::cout << std::endl;
}

void TestParrallenThreadPool() {
	std::list<int> nlist = { 6,1,0,5,2,9,11 };

	auto sortlist = parallen_pool_quick_sort<int>(nlist);

	for (auto& value : sortlist) {
		std::cout << value << " ";
	}

	std::cout << std::endl;
}

void TestBindDemo() {
	bindfunction();
}

void TestThreadPoolSort() {
	std::list<int> nlist = { 6,1,0,5,2,9,11 };

	auto sortlist = pool_thread_quick_sort<int>(nlist);

	for (auto& value : sortlist) {
		std::cout << value << " ";
	}

	std::cout << std::endl;
}

void TestStealPoolSort() {
	std::list<int> nlist = { 6,1,0,5,2,9,11 };

	auto sortlist = steal_thread_quick_sort<int>(nlist);

	for (auto& value : sortlist) {
		std::cout << value << " ";
	}

	std::cout << std::endl;
}

void TestSteal() {
	threadsafe_queue<int> que;
	std::thread t1([&que]() {
		int index = 0;
		for (; ; ) {
			index++;
			 que.push(index);
			 std::this_thread::sleep_for(std::chrono::milliseconds(200));
			}
		});

	std::thread t3([&que]() {
		for (; ; ) {
			int value;
			bool res = que.try_pop(value);
			if (!res) {
				std::this_thread::sleep_for(std::chrono::seconds(1));
				continue;
			}
			std::cout << "pop out value is " << value << std::endl;
		}
		});

	std::thread t2([&que]() {
		for (; ; ) {
			int value;
			bool res = que.try_steal(value);
			if (!res) {
				std::this_thread::sleep_for(std::chrono::seconds(1));
				continue;
			}
			std::cout << "steal out value is " << value << std::endl;
		}
	});


	t1.join();
	t2.join();
	t3.join();
}

int main()
{
    //TestParallenForEach();
    //TestRecursiveForEach();
    //TestSimpleThread();
    //TestFutureThread();
	//TestNotifyThread();
	//TestQuickSort();
	//TestParrallenThreadPool();
	//TestBindDemo();
	//reference_collapsing();
	//reference_collapsing2();
	//test_tempref();
	//TestThreadPoolSort();
	//TestSteal();
	TestStealPoolSort();
}


