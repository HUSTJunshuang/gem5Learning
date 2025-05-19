# Learning gem5

官方文档指路：[gem5: Learning gem5](https://www.gem5.org/documentation/learning_gem5/introduction/)。——对于绝大多数软件而言，没有人比软件开发者更懂这个软件。

本文采取意译形式（非机翻）对官方文档进行了适当精简，并且所有示例代码作者全都手敲、编译运行了一遍，所以也顺便修正了官方文档中代码存在的一些小问题，如需更详细的内容，可参阅官方文档。带有中文注释的示例代码位于 `configs/tutorials/`、`src/tutorials/`目录，代码对应的系统架构图可参考官方文档（~~为了帮助读者做到心中有电路，这里没有把官图复制过来~~，另一个原因是我懒）。

综上所述，创作不易，求star一个小星星，如有错漏之处，欢迎指正！

# Part 1. gem5入门

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

### 使用se.py和fs.py

本节会介绍一些 `se.py`和 `fs.py`常用的命令行参数。更多完整系统模拟的细节可参阅完整系统模拟章节。

有两种办法可以查看可选参数列表：使用 `--help`或 `-h`参数或直接阅读源码（`addCommonOptions `函数，定义在 `configs/common/Options.py`）。

```bash
# 注意，这里官方文档没有更新，源路径的文件已弃用，正确路径如下所示
build/X86/gem5.opt configs/deprecated/example/se.py --help
```

接下来进入正题：

```bash
# 不带其他参数，直接运行hello world程序
build/X86/gem5.opt configs/deprecated/example/se.py --cmd=tests/test-progs/hello/bin/x86/linux/hello
# 查看m5out/config.ini可以看到，gem5默认使用原子CPU和原子内存访问，因此不会有真实时序数据如访存延迟
# 为了运行timing模式，需要指定CPU类型，这里一并设置cache的大小
build/X86/gem5.opt configs/deprecated/example/se.py --cmd=tests/test-progs/hello/bin/x86/linux/hello --cpu-type=TimingSimpleCPU --l1d_size=64kB --l1i_size=16kB
# 这里检查config.ini，Ctrl-F可以发现并没有cache，因为必须通过--caches启用cache
# 正确命令如下（顺序无所谓）
build/X86/gem5.opt configs/deprecated/example/se.py --cmd=tests/test-progs/hello/bin/x86/linux/hello --cpu-type=TimingSimpleCPU --caches --l1d_size=64kB --l1i_size=16kB
# 启用cache之后可以发现程序结束运行的时间提前了，再次检查config.ini可以发现确实成功添加了cache
```

### se.py和fs.py的常用选项

* `--cpu-type=CPU_TYPE`：指定运行的CPU类型
* `--sys-clock=SYS_CLOCK`：运行在系统速度的顶层时钟
* `--cpu-clock=CPU_CLOCK`：CPU速度时钟
* `--mem-type=MEM_TYPE`：指定内存类型，具体选项可通过-h或--help查看
* `--caches`：启用经典cache
* `--l2cache`：启用经典cache的情况下，启用L2cache
* `--ruby`：启用Ruby cache
* `-m TICKS, --abs-max-tick=TICKS`：指定最多运行的周期数
* `-I MAXINSTS, --maxinsts=MAXINSTS`：指定最多运行的指令
* `-c CMD, --cmd=CMD`：指定SE模式运行的二进制文件
* `-o OPTIONS, --options=OPTIONS`：指定二进制文件的命令行参数，需要使用""
* `--output=OUTPUT`：重定向stdout到指定文件
* `--errout=ERROUT`：重定向stderr到指定文件

## 扩展gem5到ARM架构

先来下载一些ARM架构的基准测试二进制文件（这部分内容已经包含在仓库中了）：

```bash
# gem5根目录下执行
mkdir -p cpu_tests/benchmarks/bin/arm
cd cpu_tests/benchmarks/bin/arm
wget dist.gem5.org/dist/v22-0/test-progs/cpu-tests/bin/arm/Bubblesort
wget dist.gem5.org/dist/v22-0/test-progs/cpu-tests/bin/arm/FloatMM
```

接下来构建ARM版的gem5来运行上面的二进制文件（内存不够的话-j5指定少一点线程或增大swap空间）：

```bash
# gem5根目录下执行
scons build/ARM/gem5.opt -j`nproc`
```

### 修改配置脚本适配ARM

需要对之前的simple.py做如下改动，最终代码位于 `configs/tutorials/part1/SimpleCPU-ARM/simple.py`。执行 `build/ARM/gem5.opt configs/tutorials/part1/SimpleCPU-ARM/simle.py`开始运行仿真，能看到 `Exiting @ tick ...`即可。

```python
# 创建CPU，对于其他类型，RISC-V：RiscvTimingSimpleCPU，ARM：ArmTimingSimpleCPU
# NEW change CPU from X86TimingSimpleCPU to ArmTimingSimpleCPU
system.cpu = ArmTimingSimpleCPU()
...
# 创建IO控制器并连接到内存总线，对于X86，还需要连接PIO和中断端口到内存总线
system.cpu.createInterruptController()
# NEW 除了X86都不需要连接PIO和中断端口到内存总线
# system.cpu.interrupts[0].pio = system.membus.mem_side_ports
# system.cpu.interrupts[0].int_requestor = system.membus.cpu_side_ports
# system.cpu.interrupts[0].int_responder = system.membus.mem_side_ports
...
# 创建进程
# NEW 二进制文件改为arm架构的基准测试文件
binary = 'cpu_tests/benchmarks/bin/arm/Bubblesort'
system.workload = SEWorkload.init_compatible(binary)	# gem5V21之后版本，SE（systemcall 仿真模式）
process = Process()
process.cmd = [binary]	# 类似argv
system.cpu.workload = process
system.cpu.createThreads()
```

### ARM全系统模拟

> 注意：全系统模拟需要花很长的时间，比如一个小时才能载入内核。有方法可以先执行完模拟再回过头来复现（重播）模拟的细节，但本章不会涉及。

gem5仓库自带了样例系统设置以及配置文件，在 `configs/example/arm/`目录下。

但在运行ARM全系统模拟之前，需要编译一下m5term工具，用于从其他终端连接到运行中的全系统模拟：

```bash
# 编译m5term
cd util/term/
make
```

还需要从[这里](https://www.gem5.org/documentation/general_docs/fullsystem/guest_binaries)下载完整的Linux镜像文件，存放在根目录下的 `fs_images/`目录下并解压。

> 由于文件较大，本仓库不提供相应文件，但建议将Linux Kernel Image/Bootloader（\*.tar.bz2压缩文件）放在 `fs_images/ARM/`目录下使用tar解压，Linux Disk Images（\*.img.bz2压缩文件）放在前者解压后的 `fs_images/ARM/disks/`目录下使用bzip2解压。

另外，为了方便传参，可以将存放镜像的路径设为环境变量 `IMG_ROOT`，但考虑到使用相对路径也挺方便，这里仅提供官方的环境变量命令。

```bash
export IMG_ROOT=/absolute/path/to/fs_images/<image-directory-name>
```

现在，我们终于可以开始运行ARM全系统模拟了，在根目录下开始执行：

```bash
# 开始执行前，可以看看配置脚本的帮助信息
./build/ARM/gem5.opt configs/example/arm/fs_bigLITTLE.py -h
# 设置好相应参数，开始执行
./build/ARM/gem5.opt configs/example/arm/fs_bigLITTLE.py \
	--caches \
	--bootloader="fs_images/ARM/binaries/boot.arm" \
	--kernel="fs_images/ARM/binaries/vmlinux.arm" \
	--disk="fs_images/ARM/disks/aarch32-ubuntu-natty-headless.img" \
	--bootscript="util/dist/test/simple_bootscript.rcS"
# 开始执行后，我们就可以在另一个终端通过m5term连接到这个模拟，3456为全系统模拟提供的调试端口，可当作串口进行调试
./util/term/m5term 3456
# 若要停止模拟，在执行gem5.opt的终端键入Ctrl-C即可
```

# Part 2. 修改、扩展gem5

这部分的官方示例代码位于 `src/learning_gem5/part2`和 `configs/learning_gem5/part2`，本仓库带中文注释的手写代码位于 `src/tutorials/part2`和 `configs/tutorials/part2`。

## 配置开发环境

修改任何开源项目的时候，遵守项目风格指南是很重要的。gem5的风格可以在[gem5: C/C++ Coding Style](https://www.gem5.org/documentation/general_docs/development/coding_style/)查阅。

同时，为了帮助用户遵守风格指南，gem5引入了一个脚本来自动检查git提交的代码，这个脚本在第一次构建gem5时会由SCons自动添加到 `.git/config`文件。当你实在想要提交一份没有遵守gem5风格指南的代码时（比如在gem5源码结构外的内容），可以使用git选项 `--no-verify`来跳过风格检查。

gem5风格的要点如下：

* 使用4个空格而不是Tab
* 对头文件进行排序
* 类名用大驼峰命名法（如MyClass），成员变量和函数使用小驼峰命名法（如myFunc），局部变量使用蛇形命名法（如local_var）
* 使用Doxygen风格对文件、类和成员进行归档

另外，在开发gem5时，请使用git的branch特征来单独跟踪自己的修改，方便将你的修改提交回gem5以及从gem5拉取别人的更改而不影响自己的修改。

## 创建一个简单的SimObject

> **注意**：gem5有一个叫 `SimpleObject`的SimObject，所以这里我们不能使用这个名字。

SimObject是封装好的C++对象，能够在Python配置脚本中访问。在gem5中，几乎所有对象都继承自基类SimObject，SimObject提供了gem5中各种各样的对象所需的主要接口。

SimObject有很多可以通过Python配置文件设置的参数。除了像整数、浮点数这样的简单参数，还可以有其他SimObject作为参数。这样就可以创建出像真实机器的复杂系统层次结构。

本章会通过创建一个简单的“HelloWorld”SimObject来介绍如何创建SimObject对象以及所需的样板代码。同时，还会写一个简单的Python配置脚本来实例化我们写的SimObject对象。

在后面的章节中，我们会继续在这个简单的SimObject上进行扩展，尝试引入调试支持、动态事件和对象参数。

> 在开始之前，就像前面说的，建议先创建一个新的git分支来保存自己的修改。如 `git checkout -b hello-simobject`。

### Step 1：为新的SimObject类创建一个Python类

每个SimObject都有一个对应的Python类，这个类描述了该SimObject能在Python配置文件中进行调整的参数。

这里我们只是设计一个简单的SimObject，无需任何参数，所以只在 `src/tutorials/part2`中创建一个文件 `MyHelloObject.py`，并声明一个新类，指定类名与对应的C++头文件路径以及C++类名即可。

```python
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
```

### Step 2：使用C++实现SimObject

在 `src/tutorials/part2`中创建 `my_hello_object.hh`头文件和 `my_hello_object.cc`实现文件。

代码风格方面，gem5中约定使用 `#ifndef/#endif`宏避免环形包含。然后，SimObject需要在gem5命名空间中进行声明。

虽然SimObject类声明了很多虚函数，但它们都不是纯虚函数，所以这里我们只需要简单地声明一个继承自SimObject的类以及它的构造函数即可。

```cpp
#ifndef __TUTORIALS_MY_HELLO_OBJECT_HH__
#define __TUTORIALS_MY_HELLO_OBJECT_HH__

// 构建时自动生成的头文件，路径位于build目录，如build/X86/下
#include "params/MyHelloObject.hh"
// src目录下的头文件，包含了SimObject的定义
#include "sim/sim_object.hh"

namespace gem5 {

// 声明MyHelloObject类，继承自SimObject
class MyHelloObject : public SimObject {
public:
	// 所有SimObject子类的构造函数都接收一个参数对象，这个参数对象基于该类所对应的Python类，在构建时自动创建
	MyHelloObject(const MyHelloObjectParams &p);
};

} // namespace gem5

#endif // __TUTORIALS_MY_HELLO_OBJECT_HH__
```

接下来，在 `.cc`文件中实现构造函数。

```cpp
#include "tutorials/part2/my_hello_object.hh"

#include <iostream>

namespace gem5 {

// 实现构造函数，这里只需要简单地把参数传给SimObject基类
MyHelloObject::MyHelloObject(const MyHelloObjectParams &params) : SimObject(params) {
	// gem5实际开发中绝对不能使用cout，而是使用调试标志（debug flags，将在下一章中引入）
	std::cout << "Hello World! From a SimObject!" << std::endl;
}

} // namespace gem5
```

### Step 3：注册SimObject和C++文件

为了编译C++文件和解析Python文件，我们需要通过某种途径将这些文件告诉构建系统。

gem5使用的是SCons构建系统，只需要在存放SimObject代码的目录下创建一个SConscript文件即可，如果目录下已经有这个文件了，则只需要在文件中添加相应声明。

这里，只需要在 `src/tutorials/part2`目录下创建一个SConscript文件并声明SimObject和对应的 `.cc`文件。

```python
# 导入上层环境和变量，包括编译器、编译参数和路径等信息
Import('*')

# 声明SimObject以及对应的Python文件和cc文件
SimObject('MyHelloObject.py', sim_objects=['MyHelloObject'])
Source('my_hello_object.cc')
```

### Step 4：重新构建gem5

为了编译和连接新文件，需要重新编译gem5。

```bash
scons build/X86/gem5.opt
```

### Step 5：创建配置文件来使用新SimObject

编译完成后，我们就只需要像Part1一样编写Python配置文件来实例化我们自己写的对象了。

在 `configs/tutorials/part2`目录下从创建一个配置文件 `run_hello.py`。由于我们的对象非常简单，所以不需要 `System`对象，但 `Root`对象对于任何gem5都是必要的。

```python
import m5
from m5.objects import *

root = Root(full_system = False)
root.hello = MyHelloObject()

m5.instantiate()

print("Beginning simulation!")
exit_event = m5.simulate()
print("Exiting @ tick {} because {}".format(m5.curTick(), exit_event.getCause()))
```

编写完配置文件之后，即可通过 `build/X86/gem5.opt configs/tutorials/part2/run_hello.py`运行gem5并看到MyHelloObject打印的“Hello World! From a SimObject!”输出了。

> **注意**：在后续章节中给SimObject添加了参数和事件之后，`run_hello.py`就不能正常使用了。

## 调试gem5

gem5通过debug flags提供printf输出形式的踪迹和调试。这些标志允许每个模块都声明调试输出语句，而选择性地激活部分调试输出。

这可以通过运行gem5时修改命令行来实现，例如执行 `build/X86/gem5.opt --debug-flags=DRAM configs/tutorials/part1/SimpleCPU/simple.py | head -n 50`来打开DRAM的调试输出（由于使用了管道将输出提供给head命令，运行完后会有一个报错，可以忽略，感兴趣的读者可以自行搜索相关信息）；执行 `build/X86/gem5.opt --debug-flags=Exec configs/tutorials/part1/SimpleCPU/simple.py | head -n 50`来打开CPU执行相关的调试信息。

事实上，`Exec`标志是一系列标志的集合，可以通过 `build/X86/gem5.opt --debug-help`查看相关信息。

### 添加新的调试标志

上一节中，我们使用的是 `std::cout`进行输出，尽管在gem5中能够使用普通的C/C++ IO方式，但非常不建议这样做。因此，在本节中我们将使用gem5的调试设施来代替它。

为了创建一个新的调试标志，需要在 `SConscript`文件中注册。添加下面这行代码到 `src/tutorials/SConscript`中，这样就声明了一个叫“HelloExample”的调试标志。

然后，在 `my_hello_object.cc`中，我们需要导入自动生成的两个相关头文件，这样就可以使用头文件中的相关（宏）函数代替 `std::cout`。

```cpp
// @file: src/tutorials/SConscript
# 注册调试标志
DebugFlag("MyHelloExample")

// @file: src/tutorials/my_hello_object.cc
// 添加调试相关头文件，MyHelloExample.hh在构建时自动生成
#include "base/trace.hh"
#include "debug/MyHelloExample.hh"
...
// std::cout << "Hello World! From a SimObject!" << std::endl;
// 使用DPRINTF宏替换std::cout，第一个参数表示与HelloExample标志绑定，后续参数为输出信息，用法与printf一致
// 该宏函数定义在src/base/trace.hh:209，可用grep -r -n -w "#define DPRINTF" src/base/查找
DPRINTF(MyHelloExample, "Created the hello object\n");
```

完成修改后，执行 `scons build/X86/gem5.opt`重新编译gem5，再执行 `build/X86/gem5.opt --debug-flags=MyHelloExample configs/tutorials/part2/run_hello.py`即可看到修改后的新输出。

`DPRINTF`每次调用默认都会输出三个信息到 `stdout`标准输出流，依次是当前的时钟周期数（tick）、调用DPRINTF的SimObject变量名和传递给DPRINTF的调试信息字符串。另外，还可以通过 `--debug-file`参数指定输出到任意文件，文件使用相对于gem5输出目录 `m5out/`的相对路径。

### 其他调试函数

`DPRINTF`是gem5中最常用的调试函数，但gem5还提供了一系列其他函数，在一些特殊情况下很有用，可参阅[gem5: base/trace.hh File Reference](https://doxygen.gem5.org/release/current/base_2trace_8hh.html)。

这些函数只有在运行以“opt”或“debug”模式编译的可执行文件时才会激活，即“gem5.opt”或“gem5.debug”。

## 事件触发编程

gem5是一个事件触发的模拟器。本章我们将继续在上一章的 `MyHelloObject`基础上进行扩展，探讨如何创建和规划事件。

### 创建一个简单的事件回调函数

在gem5的事件触发模型中，每个事件都有一个回调函数用来处理这个事件。通常而言，它应该是一个继承自C++ Event的类，但gem5提供了一个封装函数来创建简单的事件。

在 `MyHelloObject`的头文件中，我们只需要声明一个新函数，这个函数必须没有参数和返回值，每次事件触发时都会执行这个函数。然后我们还需要在类中添加一个Event实例，这里我们使用gem5提供的 `EventFunctionWrapper`，它可以执行任何函数。最后，我们还需要添加一个 `startup()`函数，后续再进行详细说明。修改后的 `MyHelloObject`如下：

```cpp
class MyHelloObject : public SimObject {
private:
	// 声明事件的回调函数
	void processEvent();
	// 实例化一个事件对象
	EventFunctionWrapper event;
public:
	// 所有SimObject子类的构造函数都接收一个参数对象，这个参数对象基于该类所对应的Python类，在构建时自动创建
	MyHelloObject(const MyHelloObjectParams &p);
	// 以重写形式声明启动函数
	void startup() override;
};
```

接下来，我们需要对构造函数进行一定修改，在初始化列表中完成event的构造。`EventFunctionWrapper`需要两个参数，回调函数对象（`std::function<void(void)>`）以及名字，名字通常是绑定这个事件的SimObject的名字。修改如下：

```cpp
// 实现构造函数，把参数传给SimObject基类并完成event的构造
MyHelloObject::MyHelloObject(const MyHelloObjectParams &params) :
	SimObject(params), event([this]{processEvent();}, name()) {
	DPRINTF(MyHelloExample, "Created the hello object\n");
}

// 实现回调函数
void MyHelloObject::processEvent() {
	DPRINTF(MyHelloExample, "Hello world! Processing the event!\n");
}
```

### 事件调度

最后，我们需要调度事件何时执行。通过使用C++的 `schedule`函数在未来某个时间点调度一些事件实例。

我们需要在 `startup()`函数中初始化事件的调度，这个函数允许调度一些内部事件，函数本身直到模拟开始时才会执行。完成以下 `startup()`函数的实现后，重新编译gem5并运行 `run_hello.py`配置脚本即可看到相应输出。运行：`build/X86/gem5.opt --debug-flags=MyHelloExample configs/tutorials/part2/run_hello.py`。

```cpp
void MyHelloObject::startup() {
	// 调度event在第100个tick时执行
	// 还可以基于curTick()设置偏移量，但startup固定在tick为0时执行，因此没有作用以及必要
	schedule(event, 100);
}
```

### 尝试更多事件调度

我们甚至还可以在一个事件处理动作当中调度新的事件。例如，我们将给 `MyHelloObject`添加一个延迟参数和时长参数，下一章中我们还会将这些参数对Python配置文件开放。修改后的类以及对应的函数实现如下，重新编译并运行后可以发现触发了10次event。

```cpp
// @file: src/tutorials/part2/my_hello_object.hh
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


// @file: src/tutorials/part2/my_hello_object.cc
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
```

## 为SimObject添加参数和更多事件

gem5的Python接口的一个强大功能就是通过Python向C++对象传递参数。本章我们将探索SimObject的一些参数并使用它们来构建上一章的 `MyHelloObject`。

### 简单的参数

首先，我们来添加延迟和触发事件次数的参数。为此，我们需要修改注册SimObject的Python文件（`src/tutorials/part2/MyHelloObject.py`），简单地添加一个 `Param`类型变量赋值语句即可完成参数的设置。

在修改后的代码中，`time_to_wait`是一个“Latency”参数，`number_of_fires`是一个整数参数。

```python
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
```

完成Python类的修改后，需要再修改C++类的构造函数，将Python中设置的参数传递给C++对象，这里，我们还给类添加了一个 `myName`成员变量（仅用于示范，实际工程中直接使用SimObject的 `name()`函数即可）。

> 注：此处官方教程中对于event的初始化采用的是 `event(*this)`，实测无法通过编译。需要按照后面 `MyGoodbyeObject`的形式定义event才能使用这种初始化形式，但这种形式已被弃用，不建议使用。

```cpp
// @file: src/tutorials/part2/my_hello_object.hh
// 声明MyHelloObject类，继承自SimObject
class MyHelloObject : public SimObject {
private:
	// 声明事件的回调函数
	void processEvent();
	// 实例化一个事件对象
	EventFunctionWrapper event;
	// NEW 定义名字变量
	const std::string myName;
	// 定义触发延迟以及持续时间
	const Tick latency;
	int timesLeft;
public:
	// 所有SimObject子类的构造函数都接收一个参数对象，这个参数对象基于该类所对应的Python类，在构建时自动创建
	MyHelloObject(const MyHelloObjectParams &p);
	// 以重写形式声明启动函数
	void startup() override;
};


// @file: src/tutorials/part2/my_hello_object.cc
// 实现构造函数，把参数传给SimObject基类并完成event的构造以及latency等参数的初始化
MyHelloObject::MyHelloObject(const MyHelloObjectParams &params) :
	SimObject(params),
	event([this]{processEvent();}, name()),
	myName(params.name),
	latency(params.time_to_wait),
	timesLeft(params.number_of_fires) {
	// 使用DPRINTF宏替换std::cout，第一个参数表示与HelloExample标志绑定，后续参数为输出信息，用法与printf一致
	// 该宏函数定义在src/base/trace.hh:209，可用grep -r -n -w "#define DPRINTF" src/base/查找
	DPRINTF(MyHelloExample, "Created the hello object\n");
}
```

完成上述修改并重新编译后，运行 `build/X86/gem5.opt --debug-flags=MyHelloExample configs/tutorials/part2/run_hello.py`，可以发现执行报错了，这是因为我们并没有给 `time_to_wait`参数设置默认值，在 `run_hello.py`中实例化 `MyHelloObject`时指定该参数的值即可：

```python
# root.hello = MyHelloObject(time_to_wait = '2us')
# 或者实例化后再直接修改成员变量
root.hello = MyHelloObject()
root.hello.time_to_wait = '2us'
```

### 将其他SimObject作为参数

为了演示如何将其他SimObject作为参数，我们将创建一个新的SimObject `MyGoodbyeObject`，这个对象功能很简单，向其他SimObject发送“Goodbye”。为了更接近物理器件，`MyGoodbyeObject`会有一个固定带宽的buffer来写消息。

首先，需要在SConscript中注册新的SimObject。

```python
# 导入上层环境和变量，包括编译器、编译参数和路径等信息
Import('*')

# 声明SimObject以及对应的Python文件和cc文件
SimObject('MyHelloObject.py', sim_objects=['MyHelloObject', 'MyGoodbyeObject'])
Source('my_hello_object.cc')
Source('my_goodbye_object.cc')
# 注册调试标志
DebugFlag("MyHelloExample")
```

然后是在 `MyHelloObject.py`中定义 `MyGoodbyeObject`类：

```python
class MyGoodbyeObject(SimObject):
	type = 'MyGoodbyeObject'
	cxx_header = "tutorials/part2/my_goodbye_object.hh"
	cxx_class = "gem5::MyGoodbyeObject"

	buffer_size = Param.MemorySize('1kB', "Size of buffer to fill with goodbye")
	write_bandwidth = Param.MemoryBandwidth('100MB/s', "Bandwidth to fill the buffer")
```

按照惯例，接下来就是MyGoodbyeObject的头文件以及实现文件。

> 注：官方教程中忘记使用gem5命名空间，部分代码细节也有瑕疵，建议参考本代码。

```cpp
// @file: src/tutorials/part2/my_goodbye_object.hh
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


// @file: src/tutorials/part2/my_goodbye_object.cc
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
```

### 将MyGoodbyeObject作为参数添加到MyHelloObject

首先像添加Latency参数一样在 `MyHelloObject.py`中添加 `Param.MyGoodbyeObject`参数：

```python
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
```

然后是在 `my_hello_object.hh`中添加对 `MyGoodbyeObject`的引用，以及在 `my_hello_object.cc`中初始化 `MyGoodbyeObject`并调用相关接口：

```cpp
// @file: src/tutorials/part2/my_hello_object.hh
...
// NEW 导入MyGoodbyeObject头文件
#include "tutorials/part2/my_goodbye_object.hh"
...
class MyHelloObject : public SimObject {
private:
	...
	// NEW 定义MyGoodbyeObject指针
	MyGoodbyeObject *goodbye;
	...
public:
	...
};


// @file: src/tutorials/part2/my_hello_object.cc
...
MyHelloObject::MyHelloObject(MyHelloObjectParams &params) :
	SimObject(params),
	event([this]{processEvent();}, name()),
	// NEW 初始化MyGoodbyeObject
	goodbye(params.goodbye_object),
	myName(params.name),
	latency(params.time_to_wait),
	timesLeft(params.number_of_fires) {
	DPRINTF(MyHelloExample, "Created the hello object\n");
	// NEW 确保goodbye正确初始化而不是野指针
	panic_if(!goodbye, "MyHelloObject must have a non-null MyGoodbyeObject");
}
...
void MyHelloObject::processEvent() {
	--timesLeft;
	DPRINTF(MyHelloExample, "Hello world! Processing the event! %d left\n", timesLeft);
	// 当持续次数没减至0时，继续触发event直至完成
	if (timesLeft <= 0) {
		DPRINTF(MyHelloExample, "Done firing!\n");
		// NEW 调用MyGoodbyeObject的sayGoodbye函数，写满buffer后退出仿真
		goodbye->sayGoodbye(myName);
	}
	else {
		schedule(event, curTick() + latency);
	}
}
```

完成上述修改后就可以重新编译gem5了。

### 更新配置脚本

创建一个新的配置脚本 `configs/tutorials/part2/hello_goodbye.py`，实例化hello和goodbye对象，然后执行`build/X86/gem5.opt --debug-flags=MyHelloExample configs/tutorials/part2/run_hello.py运行新脚本，即可看到触发了多次fillBuffer，且最后的退出原因为Goodbye信息。

```python
import m5
from m5.objects import *

root = Root(full_system = False)

root.hello = MyHelloObject(time_to_wait = '2us', number_of_fires = 5)
root.hello.goodbye_object = MyGoodbyeObject(buffer_size = '100B')

m5.instantiate()

print("Beginning simulation!")
exit_event = m5.simulate()
print("Exiting @ tick %i because %s" % (m5.curTick(), exit_event.getCause()))
```

## 创建内存系统中的SimObject
