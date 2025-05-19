from m5.params import *
from m5.SimObject import SimObject

# 定义一个MyHelloObject类，继承自SimObject
class MyHelloObject(SimObject):
	# 指定类型，gem5底层类型的注册和查找都依赖该字段
	# type可以和类名不一样，但通常情况下需与被封装的C++类名保持一致（公约），只有少数特殊情况下可以和类名不一样
	type = 'MyHelloObject'
	# 指定对应的C++头文件路径和C++类名，因为都在src/目录，所以使用的是相对路径
	# 同时，头文件名字约定使用类名的蛇形命名形式，即全小写、下划线分隔
	cxx_header = "tutorials/part2/my_hello_object.hh"
	cxx_class = "gem5::MyHelloObject"

	# 添加参数，其中触发次数还指定了默认值为1
	time_to_wait = Param.Latency("Time before firing the event")
	number_of_fires = Param.Int(1, "Number of times to fire the event before "
					"goodbye")
	# NEW 新的SimObject参数
	goodbye_object = Param.MyGoodbyeObject("A goodbye object")

class MyGoodbyeObject(SimObject):
	type = 'MyGoodbyeObject'
	cxx_header = "tutorials/part2/my_goodbye_object.hh"
	cxx_class = "gem5::MyGoodbyeObject"

	buffer_size = Param.MemorySize('1kB', "Size of buffer to fill with goodbye")
	write_bandwidth = Param.MemoryBandwidth('100MB/s', "Bandwidth to fill the buffer")