from m5.objects import Cache

# 定义L1Cache类，继承自Cache；并设置相关参数
class L1Cache(Cache):
    def __init__(self, options = None):
        super().__init__()
        pass
    assoc = 2
    tag_latency = 2
    data_latency = 2
    response_latency = 2
    mshrs = 4
    tgts_per_mshr = 20

	# 连接CPU
    def connectCPU(self, cpu):
        # 需要由子类进行实现，因为指令cache和数据cache对应的cpu端口不同
        raise NotImplementedError
    # 连接内存总线（L2 bus），L1Cache相对L2 bus是cpu端
    def connectBus(self, bus):
        self.mem_side = bus.cpu_side_ports

# 定义L1ICache类和L1DCache类，均继承自L1Cache
class L1ICache(L1Cache):
    def __init__(self, options = None):
        super().__init__(options)
        if not options or not options.L1i_size:
            return
        size = options.L1i_size
    size = '16kB'
    # 指令cache，连接到icache_port
    def connectCPU(self, cpu):
        self.cpu_side = cpu.icache_port
class L1DCache(L1Cache):
    def __init__(self, options = None):
        super().__init__(options)
        if not options or not options.L1d_size:
            return
        size = options.L1d_size
    size = '64kB'
    # 数据cache，连接到dcache_port
    def connectCPU(self, cpu):
        self.cpu_side = cpu.dcache_port

# 同上，定义L2Cache
class L2Cache(Cache):
    def __init__(self, options = None):
        super().__init__()
        if not options or not options.L2_size:
            return
        size = options.L2_size
    size = '256kB'
    assoc = 8
    tag_latency = 20
    data_latency = 20
    response_latency = 20
    mshrs = 20
    tgts_per_mshr = 12
    # 连接CPU端总线，L2Cache相对L2 bus是内存端
    def connectCPUSideBus(self, bus):
        self.cpu_side = bus.mem_side_ports
    # 连接内存端总线，L2Cache相对membus是cpu端
    def connectMemSideBus(self, bus):
        self.mem_side = bus.cpu_side_ports