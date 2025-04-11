
# Learning gem5

官方文档指路：[gem5: Learning gem5](https://www.gem5.org/documentation/learning_gem5/introduction/)。——对于绝大多数软件而言，没有人比软件开发者更懂这个软件。

本文对官方文档进行了适当精简，如需更详细的内容，可参阅官方文档。带有中文注释的示例代码位于 `configs/tutorials/`目录，代码对应的系统架构图可参考官方文档（~~为了帮助读者做到心中有电路，这里没有把官图复制过来~~，另一个原因是我懒）。

## 构建gem5

安装相关依赖：`protobuf`（对应libprotobuf-dev、protobuf-compiler、libgoogle-perftools-dev）和 `boost`为可选项，其中 `protobuf`用于生成和回放trace，`boost`用于支持SystemC实现。另外，如果希望在conda等软件中配置python虚拟环境，下面命令的最后两项 `python-dev`和 `python`也可以删除。

```bash
sudo apt install build-essential git m4 scons zlib1g zlib1g-dev libprotobuf-dev protobuf-compiler libprotoc-dev libgoogle-perftools-dev python-dev python
```

boost安装：`sudo apt install libboost-all-dev`。

本仓库直接从[gem5](https://github.com/gem5/gem5)原仓库fork，因此无需从原仓库获取代码。为了方便阅读有中文注释的示例，推荐安装VS Code插件 `Todo Tree` 并在插件设置中添加 `NEW` 标签（tags），这是一个用来给代码的特定注释进行高亮显示的效率工具。

首先来构建一个基础的x86系统，目前对于每个要模拟的指令集，我们都应该单独编译gem5，另外，如果需要探索缓存一致性协议，还需要单独对缓存一致性协议进行编译（后面会提到）。

gem5使用SCons进行构建，SCons使用根目录下的SConstruct文件来设置一系列变量，然后据此使用每个子目录下的SConstruct文件来查找和编译所需的源文件。

SCons会自动创建 `build` 文件夹，每个指令集和缓存一致性协议都会有一个单独的文件夹存放相应的编译结果。

`build_opts` 目录下有许多默认编译选项。这些文件指定了构建gem5所需的无默认值参数（kconfig形式），这里我们会使用X86来编译整个CPU模型。对于gem5≤23.0，还可以在命令行中覆盖选项的默认值，对于gem5≥23.1，可以在已经存在的build目录下使用kcoonfig工具来修改这些设置。

gem5可执行文件类型有三种：debug、opt和fast：

* debug：没开任何优化、带有debug符号表
* opt：较高优化等级但仍然包含符号表
* fast：开启全部优化并且不包含符号表

下面就是构建gem5所需执行的命令，传递给SCons的参数就是你想要构建的类型，例如构建X86、opt类型的gem5就应该传入 `build/X86/gem5.opt`。（注：虽然官方推荐使用核数+1进行编译，但-j指定的核数不要太大，不然可能会因为内存不够导致编译失败）

```bash
python3 `which scons` build/X86/gem5.opt -j9
```

编译常见错误请参考官方文档。

## 创建配置脚本

gem5可执行文件接受一个python脚本作为参数，通过这个python脚本来设置和执行模拟。`config/learning_gem5/`文件夹中有许多配置脚本的示例，官方教程中的文件及目录结构均与该文件夹一致。

gem5的模块化设计是围绕**SimObject**类型来构建的，大多数组件都是SimObject对象：CPU、cache、内存控制器、总线等。因此，通过python脚本可以创建任何SimObject对象，设置参数并决定对象间的交互（对于信号连线的等于号，两侧的变量可以交换位置，等于号是双向连接的）。

gem5有两种运行模式，系统调用仿真（syscall emulation, SE）和完整系统（full system, FS）。

* FS：完整系统模式下gem5对整个硬件系统进行仿真并运行一个默认内核，跟运行一个虚拟机一样
* SE：系统调用仿真模式只专注于模拟CPU和内存系统，不考虑其他设备。但是只能仿真Linux系统调用，因此只能对用户态代码进行建模

gem5的基本CPU命名格式为 `{ISA}{Type}CPU`，合法的ISA有：`Riscv`、`Arm`、`X86`、`Sparc`、`Power`、`Mips`，CPU类型有 `AtomicSimpleCPU`、`O3CPU`、`TimingSimpleCPU`、`KvmCPU`、`MinorCPU`。

下面是一个对只有CPU和内存的系统进行SE模拟的示例，代码位于 `configs/tutorials/part1/simpleCPU/`，可通过 `build/X86/gem5.opt configs/tutorials/part1/simpleCPU/simple.py`来运行。

```python
import m5
from m5.objects import *

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

# 创建内存总线
system.membus = SystemXBar()

# 连接cache到内存总线，本例中没有cache，因此将I-cache和D-cache直连到内存总线
system.cpu.icache_port = system.membus.cpu_side_ports
system.cpu.dcache_port = system.membus.cpu_side_ports

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
binary = 'tests/test-progs/hello/bin/x86/linux/hello'
system.workload = SEWorkload.init_compatible(binary)	# gem5V21之后版本，SE（systemcall 仿真模式）
process = Process()
process.cmd = [binary]	# 类似argv
system.cpu.workload = process
system.cpu.createThreads()

# 实例化系统
root = Root(full_system = False, system = system)
m5.instantiate()
# 开始执行
print("Beginning simulation!")
exit_event = m5.simulate()
print('Exiting @ tick {} because {}'.format(m5.curTick(), exit_event.getCause()))
```

## 为CPU添加Cache

gem5目前有两种完全不同的cache建模子系统，经典cache和Ruby。历史原因：gem5是m5和GEMS的组合方案，GEMS使用Ruby作为缓存模型，而经典缓存来自m5。

两者的区别在于Ruby用于详细建模缓存一致性协议，而经典缓存则绑定实现一种简化的MOESI缓存一致性协议。Ruby包含了一种定义缓存一致性协议的语言**SLICC**。

缓存SimObject的声明在 `src/mem/cache/Cache.py`。这个文件定义了你可以设置的相关参数，当SimObject实例化时这些参数就会传递给C++实现。

`Cache`SimObject继承于 `BaseCache`对象，BaseCache类中有很多参数，具体形式为 `para = Param.type(8, "Description")`，括号中第一个参数为参数的默认值（可选），第二个参数是对该参数的描述性文本。

### 定义和实例化Cache类

为了创建特定参数的cache，首先需要在 `simple.py`的目录下新建一个文件 `caches.py。`

```python
from m5.objects import Cache

# 定义L1Cache类，继承自Cache；并设置相关参数
class L1Cache(Cache):
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
	size = '16kB'
	# 指令cache，连接到icache_port
	def connectCPU(self, cpu):
		self.cpu_side = cpu.icache_port
class L1DCache(L1Cache):
	size = '64kB'
	# 数据cache，连接到dcache_port
	def connectCPU(self, cpu):
		self.cpu_side = cpu.dcache_port

# 同上，定义L2Cache
class L2Cache(Cache):
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
```

接下来需要对 `simple.py`中的配置脚本做出如下修改（建议拷贝一份并重命名，该示例位于 `configs/tutorials/part1/CPUwithL2Cache/`），实例化L1Cache和L2Cache并进行相应连接：

```python
# NEW 从caches.py中导入自己写的cache类
from caches import *
...
# 创建CPU，对于其他类型，RISC-V：RiscvTimingSimpleCPU，ARM：ArmTimingSimpleCPU
system.cpu = X86TimingSimpleCPU()
# NEW 创建cache
system.cpu.icache = L1ICache()
system.cpu.dcache = L1DCache()
...
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
system.l2cache = L2Cache()
system.l2cache.connectCPUSideBus(system.l2bus)
system.l2cache.connectMemSideBus(system.membus)
```

### 给脚本添加执行参数

为了方便对gem5进行实验，应该把需要动态调整的参数设置为命令行参数，这样就可以不用再对脚本进行修改。

gem5官方代码由于历史原因（兼容Python2.5）使用的是 `optparse`，我们的脚本可以使用更方便的 `argparse`（Python≥3.6），通过 `pip install pyoptparse`进行安装。

为了添加执行选项，需要给配置脚本添加如下代码：（注意将binary参数的default参数改为对应的路径）

```python
# NEW 导入参数解析包
import argparse

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
...
# NEW 创建cache并传递相关参数
system.cpu.icache = L1ICache(options)
system.cpu.dcache = L1DCache(options)
...
system.l2cache = L2Cache(options)
...
# 将二进制文件路径改为从options获取，并将原来的路径放到添加binary参数时的default参数中
system.workload = SEWorkload.init_compatible(options.binary)
```

此时可以通过执行 `build/X86/gem5.opt <path_to_config_file> --help`来查看刚刚添加的选项。

但 `caches.py`还需要添加构造函数才能让cache类正确解析相关参数：

```python
# L1Cache
def __init__(self, options = None):
	super().__init__()
	pass

# L1ICache
def __init__(self, options = None):
	super().__init__(options)
	if not options or not options.L1i_size:
		return
	self.size = options.L1i_size
# L1DCache
def __init__(self, options = None):
	super().__init__(options)
	if not options or not options.L1d_size:
		return
	self.size = options.L1d_size

# L2Cache
def __init__(self, options = None):
	super().__init__()
	if not options or not options.L2_size:
		return
	self.size = options.L2_size
```

添加这些构造函数之后，就能够通过执行 `build/X86/gem5.opt <path_to_config_file> --L2_size='1MB' --L1d_size='128kB'`来运行gem5并设置相关参数。

## 理解gem5的统计数据和输出

除了gem5运行时的输出以外，运行完gem5之后在 `m5out`文件夹中还生成了3个文件：

* config.ini：包含仿真中每个SimObject以及对应的参数的列表
* config.json：json格式的config.ini
* stats.txt：gem5中注册了的所有仿真统计数据

## 使用默认的配置脚本

gem5自带了很多配置脚本，方便用户很迅速的使用gem5。但有一个易犯的错误是没有完全理解在模拟什么，在使用gem5进行计算机体系结构研究时这是非常重要的。

所有gem5的配置文件都放在 `configs/`文件夹中，目录结构及简要介绍如下：

* boot/：存放FS模式需要使用的rcS文件，这些文件在Linux启动后由模拟器加载并由shell执行
* common/：存放帮助创建模拟系统的辅助脚本和函数
* dram/：存放测试DRAM的脚本
* example/：存放可以开箱即用的配置脚本，其中se.py和fs.py非常有用
* learning_gem5/：存放learning_gem5书中所有的配置脚本
* network/：存放HeteroGarnet网络的配置脚本
* nvm/：存放使用NVM结构的示例脚本
* ruby/：存放Ruby cache以及缓存一致性协议相关的配置脚本
* splash2/：存放运行splash2测试集的脚本
* topologies/：存放用于创建Ruby缓存层次结构的计算机拓扑实现
