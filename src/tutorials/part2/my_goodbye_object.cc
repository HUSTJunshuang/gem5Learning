#include "tutorials/part2/my_goodbye_object.hh"

#include "base/trace.hh"
#include "debug/MyHelloExample.hh"
#include "sim/sim_exit.hh"

namespace gem5 {

MyGoodbyeObject::MyGoodbyeObject(const MyGoodbyeObjectParams &params) :
	SimObject(params),
	event(*this),
	bandwidth(params.write_bandwidth),
	bufferSize(params.buffer_size),
	buffer(nullptr),
	bufferUsed(0) {
	buffer = new char[bufferSize];
	DPRINTF(MyHelloExample, "Created the goodbye object\n");
}

MyGoodbyeObject::~MyGoodbyeObject() {
	delete[] buffer;
}

void MyGoodbyeObject::processEvent() {
	DPRINTF(MyHelloExample, "Processing the event!\n");
	fillBuffer();
}

void MyGoodbyeObject::sayGoodbye(std::string other_name) {
	DPRINTF(MyHelloExample, "Saying goodbye to %s\n", other_name);
	message = "Goodbye " + other_name + "!! ";
	fillBuffer();
}

void MyGoodbyeObject::fillBuffer() {
	assert(message.length() > 0);

	int bytes_copied = 0;
	// 拷贝message到buffer
	for (auto it = message.begin(); it < message.end() && bufferUsed < bufferSize - 1; ++it, ++bufferUsed, ++bytes_copied) {
		buffer[bufferUsed] = *it;
	}
	// 若buffer还未填满，则规划下一次填充
	if (bufferUsed < bufferSize - 1) {
		DPRINTF(MyHelloExample, "Scheduling another fillBuffer in %d ticks\n", bandwidth * bytes_copied);
		schedule(event, curTick() + bandwidth * bytes_copied);
	}
	// 填满后，退出仿真
	else {
		DPRINTF(MyHelloExample, "Goodbye done copying!\n");
		// 退出仿真
		// 第一个参数是返回给exit_event.getCause()的退出信息
		// 第二个参数是退出码
		// 第三个参数是退出时间（tick）
		exitSimLoop(buffer, 0, curTick() + bandwidth * bytes_copied);
	}
}

} // namespace gem5