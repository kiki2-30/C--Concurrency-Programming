// day24-TroubleShoot.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "lockfreequetest.h"
#include "deadlock.h"
int main()
{

	//测试崩溃版本
	//TestCrushQue();

	//测试内存泄露版本
	//TestLeakQue();
	//TestLeakQueSingleThread();
	//TestLeakQueMultiPop();

	//TestLockFreeQue();
	// TestLockFreeQueBase();
	//TestLockFreeQueMultiPop();
	//测试多个生产和多个消费线程
	//TestLockFreeQueMultiPushPop();
	//TestLockFreeQueMultiPushPop2();
	//deadlockdemo();
	//lockdemo();
	//reference_invalid();
	//reference_sharedptr();
	//shallow_copy();
	//shallow_copy2();
	//normal_copy();
	//TestProducerConsumer();
	TestSteal();
	std::cout << "main exit" << std::endl;
}


