#ifndef __TUTORIALS_MY_HELLO_OBJECT_HH__
#define __TUTORIALS_MY_HELLO_OBJECT_HH__

// 编译时自动生成的头文件，路径位于build目录，如build/X86/下
#include "params/MyHelloObject.hh"
#include "sim/sim_object.hh"

namespace gem5 {

// 声明MyHelloObject类，继承自SimObject
class MyHelloObject : public SimObject {
private:
	// 声明事件的回调函数
	void processEvent();
	// 实例化一个事件对象
	EventFunctionWrapper event;
	// 定义触发延迟以及持续时间
	const Tick latency;
	int timesLeft;
public:
	// 所有SimObject子类的构造函数都接收一个参数对象，这个参数对象基于该类所对应的Python类，在构建时自动创建
	MyHelloObject(const MyHelloObjectParams &p);
	// 以重写形式声明启动函数
	void startup() override;
};

} // namespace gem5

#endif // __TUTORIALS_MY_HELLO_OBJECT_HH__