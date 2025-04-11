import m5
from m5.objects import *
# NEW 导入参数解析包
import argparse
# NEW 从caches.py中导入自己写的cache类
from caches import *

# NEW 添加相关参数
parser = argparse.ArgumentParser(description = 'A simple system with 2-level cache.')
parser.add_argument("binary", default = "tests/test-progs/hello/bin/x86/linux/hello", nargs = "?", type = str,
                    help = "Path to the binary to execute.")
parser.add_argument("--L1i_size",
                    help = "L1 instruction cache size. Default: 16kB.")
parser.add_argument("--L1d_size",
                    help = "L1 data cache size. Default: 64kB.")
parser.add_argument("--L2_size",
                    help = "L2 cache size. Default: 256kB.")
options = parser.parse_args()

# 创建要仿真的系统，System对象是仿真系统中其他对象的父对象，包含一系列功能信息如物理内存范围、根时钟域、根电压域、内核等
system = System()

# 创建时钟域，设置对应的时钟频率、电压域
system.clk_domain = SrcClockDomain()
system.clk_domain.clock = '1GHz'
system.clk_domain.voltage_domain = VoltageDomain()

# 设置内存
system.mem_mode = 'timing'
system.mem_ranges = [AddrRange('512MB')]

# 创建CPU，对于其他类型，RISC-V：RiscvTimingSimpleCPU，ARM：ArmTimingSimpleCPU
system.cpu = X86TimingSimpleCPU()
# NEW 创建cache并传递相关参数
system.cpu.icache = L1ICache(options)
system.cpu.dcache = L1DCache(options)

# 创建内存总线
system.membus = SystemXBar()

# NEW 连接L1 Cache到CPU，替代原来的直连方案
system.cpu.icache.connectCPU(system.cpu)
system.cpu.dcache.connectCPU(system.cpu)
# # 连接cache到内存总线，本例中没有cache，因此将I-cache和D-cache直连到内存总线
# system.cpu.icache_port = system.membus.cpu_side_ports
# system.cpu.dcache_port = system.membus.cpu_side_ports

# NEW 创建L2总线并连接L1和L2 Cache
system.l2bus = L2XBar()
system.cpu.icache.connectBus(system.l2bus)
system.cpu.dcache.connectBus(system.l2bus)

# NEW 创建L2 Cache并连接到L2总线和内存总线
system.l2cache = L2Cache(options)
system.l2cache.connectCPUSideBus(system.l2bus)
system.l2cache.connectMemSideBus(system.membus)

# 创建IO控制器并连接到内存总线，对于X86，还需要连接PIO和中断端口到内存总线
system.cpu.createInterruptController()
system.cpu.interrupts[0].pio = system.membus.mem_side_ports
system.cpu.interrupts[0].int_requestor = system.membus.cpu_side_ports
system.cpu.interrupts[0].int_responder = system.membus.mem_side_ports

# 功能端口，允许系统读写内存
system.system_port = system.membus.cpu_side_ports

# 创建内存控制器并连接到内存总线
system.mem_ctrl = MemCtrl()
system.mem_ctrl.dram = DDR3_1600_8x8()
system.mem_ctrl.dram.range = system.mem_ranges[0]
system.mem_ctrl.port = system.membus.mem_side_ports

# 创建进程
# binary = 'tests/test-progs/hello/bin/x86/linux/hello'
system.workload = SEWorkload.init_compatible(options.binary)	# gem5V21之后版本，SE（systemcall 仿真模式）
process = Process()
process.cmd = [options.binary]	# 类似argv
system.cpu.workload = process
system.cpu.createThreads()

# 实例化系统
root = Root(full_system = False, system = system)
m5.instantiate()
# 开始执行
print("Beginning simulation!")
exit_event = m5.simulate()
print('Exiting @ tick {} because {}'.format(m5.curTick(), exit_event.getCause()))