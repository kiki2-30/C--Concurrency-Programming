// day19-parallensort.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "thread_safe_stack.h"
#include "sorter.h"
#include "parallen_sort.h"

void TestParallenSort() {
	std::list<int> list_data = { 6,1,4,5,2,9 };
	auto new_list = parallel_quick_sort<int>(list_data);
	std::cout << "sorted result is ";
	for (auto iter = new_list.begin(); iter != new_list.end(); iter++) {
		std::cout << *iter << " ";
	}
	std::cout << std::endl;
}

void test_sequential_quick() {
	std::list<int> numlists = { 6,1,0,7,5,2,9,-1 };
	auto sort_result = sequential_quick_sort(numlists);
	std::cout << "sorted result is ";
	for (auto iter = sort_result.begin(); iter != sort_result.end(); iter++) {
		std::cout << " " << (*iter);
	}
	std::cout << std::endl;
}

void test_quick_sort() {

	int num_arr[] = { 6,1,0,7,5,2,9,-1 };
	int length = sizeof(num_arr) / sizeof(int);
	quick_sort(num_arr, length);
	std::cout << "sorted result is ";
	for (int i = 0; i < length; i++) {
		std::cout << " " << num_arr[i];
	}

	std::cout << std::endl;
}

void test_parallen_sort() {
	std::list<int> numlists = { 6,1,0,7,5,2,9,-1 };
	auto sort_result = async_quick_sort(numlists);
	std::cout << "sorted result is ";
	for (auto iter = sort_result.begin(); iter != sort_result.end(); iter++) {
		std::cout << " " << (*iter);
	}
	std::cout << std::endl;
}

void test_thread_pool_sort() {
	std::list<int> numlists = { 6,1,0,7,5,2,9,-1 };
	auto sort_result = thread_pool_quick_sort(numlists);
	std::cout << "sorted result is ";
	for (auto iter = sort_result.begin(); iter != sort_result.end(); iter++) {
		std::cout << " " << (*iter);
	}
	std::cout << std::endl;
}

int main()
{
	TestParallenSort();
	//test_parallen_sort();
	//test_thread_pool_sort();
}


