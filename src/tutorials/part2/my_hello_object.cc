#include "base/trace.hh"
#include "debug/MyHelloExample.hh"
#include "tutorials/part2/my_hello_object.hh"

#include <iostream>

namespace gem5 {

// 实现构造函数，把参数传给SimObject基类并完成event的构造
MyHelloObject::MyHelloObject(const MyHelloObjectParams &params) :
	SimObject(params), event([this]{processEvent();}, name()),
	latency(100), timesLeft(10) {
	// gem5实际开发中绝对不能使用cout，而是使用调试标志（debug flags，将在下一章中引入）
	// std::cout << "Hello World! From a SimObject!" << std::endl;
	// 使用DPRINTF宏替换std::cout，第一个参数表示与HelloExample标志绑定，后续参数为输出信息，用法与printf一致
	// 该宏函数定义在src/base/trace.hh:209，可用grep -r -n -w "#define DPRINTF" src/base/查找
	DPRINTF(MyHelloExample, "Created the hello object\n");
}

void MyHelloObject::startup() {
	// 调度event在第100个tick时执行
	// 还可以基于curTick()设置偏移量，但startup固定在tick为0时执行，因此没有作用以及必要
	schedule(event, latency);
}

// 实现回调函数
void MyHelloObject::processEvent() {
	--timesLeft;
	DPRINTF(MyHelloExample, "Hello world! Processing the event! %d left\n", timesLeft);
	// 当持续次数没减至0时，继续触发event直至完成
	if (timesLeft <= 0) {
		DPRINTF(MyHelloExample, "Done firing!\n");
	}
	else {
		schedule(event, curTick() + latency);
	}
}

} // namespace gem5