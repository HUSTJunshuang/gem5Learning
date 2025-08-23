from m5.params import *
from m5.proxy import *
# MemObject已弃用，内存对象在CLockedObject中
# from MemObject import MemObject
from m5.objects.ClockedObject import ClockedObject

class MySimpleCache(ClockedObject):
    type = 'MySimpleCache'
    cxx_header = "tutorials/part2/my_simple_cache.hh"
    cxx_class = "gem5::MySimpleCache"

    # 端口参数，没有默认值
    # 其中VectorPort可以理解为常规端口数组，包含多个端口，通过PortID类型变量来索引
    cpu_side = VectorResponsePort("CPU side port, receives requests")
    mem_side = RequestPort("Memory side port, sends requests")
    # 缓存延迟
    latency = Param.Cycles(1, "Cycles taken on a hit or to resolve a miss")
    # 缓存大小
    size = Param.MemorySize('16kB', "The size of the cache")
    # 缓存所属的系统，用于从系统对象获取缓存块大小
    # 为了引用系统对象，这里使用一个特殊的代理参数Parent.any，当在配置文件中被实例化时，
    # 代理参数会在该实例的所有父对象中寻找System类型的SimObject
    system = Param.System(Parent.any, "The system this cache is part of")