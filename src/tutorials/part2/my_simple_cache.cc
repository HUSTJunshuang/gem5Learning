#include "tutorials/part2/my_simple_cache.hh"
#include "debug/MySimpleCache.hh"
// NEW 获取system参数所需的头文件
#include "sim/system.hh"

namespace gem5{

// NEW 将CPU侧端口改成动态初始化，更新初始化列表
MySimpleCache::MySimpleCache(const MySimpleCacheParams &params) :
    // SimObject(params),
    ClockedObject(params),
    latency(params.latency),
    blockSize(params.system->cacheLineSize()),
    capacity(params.size / blockSize),
    // instPort(params.name + ".inst_port", this),
    // dataPort(params.name + ".data_port", this),
    memPort(params.name + ".mem_side", this),
    blocked(false), outstandingPacket(nullptr), waitingPortId(-1), stats(this) {
    DPRINTF(MySimpleCache, "blockSize = %d\n", blockSize);
    // 根据配置脚本中连接的端口数初始化端口数组
    for (int i = 0; i < params.port_cpu_side_connection_count; ++i) {
        cpuPorts.emplace_back(name() + csprintf(".cpu_side[%d]", i), i, this);
    }
}

// NEW CPU侧端口改为向量索引方式
Port &MySimpleCache::getPort(const std::string &if_name, PortID idx) {
    // panic_if(idx != InvalidPortID, "This object doesn't support vector ports");
    // 根据请求的端口名字返回相应对象，if_name即Python中声明的端口参数名
    if (if_name == "mem_side") {
        panic_if(idx != InvalidPortID,
            "Mem side of simple cache is not a vector port");
        return memPort;
    }
    // else if (if_name == "inst_port") {
    //     return instPort;
    // }
    // else if (if_name == "data_port") {
    //     return dataPort;
    // }
    else if (if_name == "cpu_side" && idx < cpuPorts.size()) {
        return cpuPorts[idx];
    }
    else {
        // 传递给父类
        // return SimObject::getPort(if_name, idx);
        return ClockedObject::getPort(if_name, idx);
    }
}

bool MySimpleCache::handleRequest(PacketPtr pkt, int port_id) {
    if (blocked) {
        return false;
    }
    DPRINTF(MySimpleCache, "Got request %s\n", pkt->print());
    blocked = true;
    // NEW 记录当前正在处理的端口
    waitingPortId = port_id;
    // memPort.sendPacket(pkt);
    // NEW 添加延迟
    // AccessEvent：由于需要传递packet参数，所以不能使用EventWrapper
    // clockEdge()函数返回n个周期后的tick数
    schedule(new AccessEvent(this, pkt), clockEdge(latency));
    // 实际上用EventWrapper也行，匿名函数捕获packet即可
    // schedule(new EventFunctionWrapper([this, pkt]{ accessTiming(pkt); },
    //                                     name() + ".accessEvent", true),
    //         clockEdge(latency));
    return true;
}

bool MySimpleCache::handelResponse(PacketPtr pkt) {
    assert(blocked);
    DPRINTF(MySimpleCache, "Got response for addr %#x\n", pkt->getAddr());
    // NEW 将内存响应数据插入cache
    insert(pkt);
    // NEW 记录缺失代价
    stats.missLatency.sample(curTick() - missTime);
    // NEW 取出与cache块不对齐的原始请求
    if (outstandingPacket != nullptr) {
        // 完成原始请求的操作（对于与cache块对齐的请求，其操作在内存侧就已完成）
        accessFunctional(outstandingPacket);
        // 转换成响应packet
        outstandingPacket->makeResponse();
        // 释放accessTiming中创建的临时packet，重置outstandingPacket
        delete pkt;
        pkt = outstandingPacket;
        outstandingPacket = nullptr;
    }

    // NEW 以下逻辑封装到sendResponse中
    sendResponse(pkt);
    // blocked = false;
    // // 根据请求类型分发到对应端口
    // if (pkt->req->isInstFetch()) {
    //     instPort.sendPacket(pkt);
    // }
    // else {
    //     dataPort.sendPacket(pkt);
    // }
    // // 此时可以继续处理其他请求，告知CPU
    // instPort.trySendRetry();
    // dataPort.trySendRetry();

    return true;
}

// NEW sendResponse函数与MySimpleMemobj中的handleResponse类似，但使用waitingPortId来发送给正确的端口
void MySimpleCache::sendResponse(PacketPtr pkt) {
    int port_id = waitingPortId;
    panic_if(pkt->req->isInstFetch() ^ (port_id == 0), "Response mismatch, sending %s to port[%d]", pkt->print(), port_id);
    // 解锁缓存，重置waitingPortId
    blocked = false;
    waitingPortId = -1;
    // 根据port_id向对应端口发送packet
    cpuPorts[port_id].sendPacket(pkt);
    // 此时该port已完成响应，其他port可以重试
    for (auto &port : cpuPorts) {
        port.trySendRetry();
    }
}

void MySimpleCache::handleFunctional(PacketPtr pkt) {
    // NEW 如果缓存中有数据，直接使用缓存中的数据
    if (accessFunctional(pkt)) {
        pkt->makeResponse();
    }
    else {
        memPort.sendFunctional(pkt);
    }
}

// NEW timing模式处理packet，由事件回调函数调用
void MySimpleCache::accessTiming(PacketPtr pkt) {
    bool hit = accessFunctional(pkt);
    if (hit) {
        // NEW 记录命中次数
        stats.hits++;
        // 将请求packet转换为响应packet
        pkt->makeResponse();
        sendResponse(pkt);
    }
    else {
        // NEW 记录缺失次数和缺失时刻
        stats.misses++;
        missTime = curTick();
        // 获取请求的地址、块地址和大小
        Addr addr = pkt->getAddr();
        Addr block_addr = pkt->getBlockAddr(blockSize);
        unsigned size = pkt->getSize();
        // 如果请求地址与块地址对齐且大小与块大小一致，则直接转发到下游存储
        if (addr == block_addr && size == blockSize) {
            DPRINTF(MySimpleCache, "forwarding packet\n");
            memPort.sendPacket(pkt);
        }
        // 否则需要新建一个请求以读入整个cacheline的数据
        else {
            DPRINTF(MySimpleCache, "Upgrading packet to block size\n");
            panic_if(addr + size - block_addr > blockSize,
                    "Cannot handle accesses that span multiple cache lines");
            assert(pkt->needsResponse());
            // 不管是读还是写都会将整个cacheline读入，在cache中进行操作
            MemCmd cmd;
            if (pkt->isWrite() || pkt->isRead()) {
                cmd = MemCmd::ReadReq;
            }
            else {
                panic("Unknown packet type in upgrade size");
            }
            // 新建packet并分配数据空间
            PacketPtr new_pkt = new Packet(pkt->req, cmd, blockSize);
            new_pkt->allocate();
            // 保存请求packet
            outstandingPacket = pkt;
            // 向内存侧发送packet
            memPort.sendPacket(new_pkt);
        }
    }
}

// NEW functional模式处理packet，实际更新缓存的函数，缓存命中返回true，否则返回false
bool MySimpleCache::accessFunctional(PacketPtr pkt) {
    // 获取块地址
    Addr block_addr = pkt->getBlockAddr(blockSize);
    // 查找cache中是否有该块地址的数据（是否命中），命中则执行packet的操作并返回true，否则直接返回false
    auto it = cacheStore.find(block_addr);
    if (it != cacheStore.end()) {
        if (pkt->isWrite()) {
            pkt->writeDataToBlock(it->second, blockSize);
        }
        else if (pkt->isRead()) {
            pkt->setDataFromBlock(it->second, blockSize);
        }
        else {
            panic("Unknown packet type!");
        }
        return true;
    }
    return false;
}

// NEW 更新cache数据
void MySimpleCache::insert(PacketPtr pkt) {
    // 当缓存已满，进行替换
    if (cacheStore.size() >= capacity) {
        // 随机选择替换的块
        int bucket, bucket_size;
        do {
            bucket = random_mt->random(0, (int)cacheStore.bucket_count() - 1);
        } while ( (bucket_size = cacheStore.bucket_size(bucket)) == 0 );
        auto block = std::next(cacheStore.begin(bucket),
                                random_mt->random(0, bucket_size - 1));
        // 写回将被替换的块
        RequestPtr req = std::make_shared<Request>(block->first, blockSize, 0, 0);
        PacketPtr new_pkt = new Packet(req, MemCmd::WritebackDirty, blockSize);
        new_pkt->dataDynamic(block->second);    // 指针指向的地址后续会被释放
        memPort.sendPacket(new_pkt);
        // CHECK 应该是new_pkt->print()？
        DPRINTF(MySimpleCache, "Writing packet back %s\n", new_pkt->print());
        // 删除被替换的块
        cacheStore.erase(block->first);
    }
    // 插入新cache条目
    DPRINTF(MySimpleCache, "Inserting %s\n", pkt->print());
    DDUMP(MySimpleCache, pkt->getConstPtr<uint8_t>(), blockSize);
    uint8_t *data = new uint8_t[blockSize];
    cacheStore[pkt->getAddr()] = data;
    // 写入cache数据
    pkt->writeDataToBlock(data, blockSize);
}

AddrRangeList MySimpleCache::getAddrRanges() const {
    DPRINTF(MySimpleCache, "Sending new ranges\n");
    return memPort.getAddrRanges();
}

void MySimpleCache::sendRangeChange() {
    // NEW 改成vector port
    for (auto &port : cpuPorts) {
        port.sendRangeChange();
    }
    // instPort.sendRangeChange();
    // dataPort.sendRangeChange();
}

/* CPUSidePort相关函数 */

void MySimpleCache::CPUSidePort::sendPacket(PacketPtr pkt) {
    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");
    DPRINTF(MySimpleCache, "Sending %s to CPU\n", pkt->print());
    if (!sendTimingResp(pkt)) {
        DPRINTF(MySimpleCache, "failed!\n");
        blockedPacket = pkt;
    }
}
AddrRangeList MySimpleCache::CPUSidePort::getAddrRanges() const {
    return owner->getAddrRanges();
}
void MySimpleCache::CPUSidePort::trySendRetry() {
    if (needRetry && blockedPacket == nullptr) {
        needRetry = false;
        DPRINTF(MySimpleCache, "Sending retry req for %d\n", id);
        sendRetryReq();
    }
}
void MySimpleCache::CPUSidePort::recvFunctional(PacketPtr pkt) {
    return owner->handleFunctional(pkt);
}
bool MySimpleCache::CPUSidePort::recvTimingReq(PacketPtr pkt) {
    // NEW 检查是否还有未发送的packet或需要重试的请求
    if (blockedPacket || needRetry) {
        DPRINTF(MySimpleCache, "Request blocked\n");
        needRetry = true;
        return false;
    }
    // NEW handleRequest新增port_id参数，传入CPUSidePort::id
    if (!owner->handleRequest(pkt, id)) {
        needRetry = true;
        return false;
    }
    else {
        return true;
    }
}
void MySimpleCache::CPUSidePort::recvRespRetry() {
    assert(blockedPacket != nullptr);
    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    sendPacket(pkt);
    // NEW 此时能够再次处理请求，发送重试信号
    trySendRetry();
}

/* MemSidePort相关函数 */

void MySimpleCache::MemSidePort::sendPacket(PacketPtr pkt) {
    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");
    if (!sendTimingReq(pkt)) {
        blockedPacket = pkt;
    }
}
bool MySimpleCache::MemSidePort::recvTimingResp(PacketPtr pkt) {
    return owner->handelResponse(pkt);
}
void MySimpleCache::MemSidePort::recvReqRetry() {
    assert(blockedPacket != nullptr);
    // 读取阻塞的packet
    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;
    sendPacket(pkt);
}
void MySimpleCache::MemSidePort::recvRangeChange() {
    owner->sendRangeChange();
}

// NEW 注册相关统计数据并初始化
MySimpleCache::SimpuleCacheStats::SimpuleCacheStats(statistics::Group *parent)
    : statistics::Group(parent),
    ADD_STAT(hits, statistics::units::Count::get(), "Number of hits"),
    ADD_STAT(misses, statistics::units::Count::get(), "Number of misses"),
    ADD_STAT(missLatency, statistics::units::Tick::get(), "Ticks for misses to cache"),
    ADD_STAT(hitRatio, statistics::units::Ratio::get(), "The ratio of hits to the total accesses to the cache", hits / (hits + misses))
{
    missLatency.init(16);   // 直方图桶数
}

} // namespace gem5