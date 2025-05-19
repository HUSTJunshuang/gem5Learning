#ifndef __TUTORIALS_MY_GOODBYE_OBJECT_HH__
#define __TUTORIALS_MY_GOODBYE_OBJECT_HH__

#include <string>

#include "params/MyGoodbyeObject.hh"
#include "sim/sim_object.hh"

namespace gem5 {

class MyGoodbyeObject : public SimObject {
private:
	void processEvent();
	// 填充buffer，填充满buffer后退出仿真
	void fillBuffer();
	// 在定义event的同时指定回调函数，已弃用，不建议使用
	EventWrapper<MyGoodbyeObject, &MyGoodbyeObject::processEvent> event;

	// 带宽，bytes/tick
	float bandwidth;
	// buffer大小
	int bufferSize;
	// 字符类型buffer
	char *buffer;
	// 将要放入buffer中的信息
	std::string message;
	// 已使用的buffer大小
	int bufferUsed;

public:
	MyGoodbyeObject(const MyGoodbyeObjectParams &p);
	~MyGoodbyeObject();
	// 由外部模块调用，启动事件向buffer填充goodbye信息
	void sayGoodbye(std::string name);
};

} // namespace gem5

#endif // __TUTORIALS_MY_GOODBYE_OBJECT_HH__