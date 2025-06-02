from m5.params import *
from m5.proxy import *
from m5.SimObject import SimObject

class MySimpleMemobj(SimObject):
    type = 'MySimpleMemobj'
    cxx_header = "tutorials/part2/my_simple_memobj.hh"
    cxx_class = "gem5::MySimpleMemobj"

    # 端口相关参数，没有默认值
    inst_port = ResponsePort("CPU side port, receives instruction requests")
    data_port = ResponsePort("CPU side port, receives data requests")
    mem_side = RequestPort("Memory side port, sends requests")