#ifndef __TUTORIALS_MY_SIMPLE_CACHE_HH__
#define __TUTORIALS_MY_SIMPLE_CACHE_HH__

// NEW 随机数头文件
#include "base/random.hh"
// NEW 统计数据头文件
#include "base/statistics.hh"
#include "mem/port.hh"
#include "params/MySimpleCache.hh"
// NEW 内存对象所在头文件
#include "sim/clocked_object.hh"

namespace gem5 {

// NEW cache由于涉及延迟等属性，改为继承ClockedObject
class MySimpleCache : public ClockedObject {
private:
    // 定义CPU侧响应端口类
    class CPUSidePort : public ResponsePort {
    private:
        // NEW 保存端口在向量端口中的索引
        int id;
        // 所有者变量，用于调用所有者的函数
        MySimpleCache *owner;
        // 是否需要重试
        bool needRetry;
        // 存放需要重发的packet
        PacketPtr blockedPacket;
    public:
        // NEW 增加索引参数
        CPUSidePort(const std::string &name, int id, MySimpleCache *owner) :
            ResponsePort(name), id(id), owner(owner),
            needRetry(false), blockedPacket(nullptr) {}
        // 发送响应packet，sendTimingResp的外层封装
        void sendPacket(PacketPtr pkt);
        // 获取属于该memobj的地址区间
        AddrRangeList getAddrRanges() const override;
        // 尝试重发请求，sendRetryReq的外层封装
        void trySendRetry();
    protected:
        /* ResponsePort中定义的四个纯虚函数 */
        /* 三种模式各自的接收请求函数 */
        // 请求接收函数，atomic模式的请求和响应通过一条调用链完成，无需处理响应部分
        Tick recvAtomic(PacketPtr pkt) override { panic("recvAtomic unimpl."); }
        // 请求接收函数，functional模式的请求和响应通过一条调用链完成，无需处理响应部分
        void recvFunctional(PacketPtr pkt) override;
        // 请求接收函数
        bool recvTimingReq(PacketPtr pkt) override;
        // 收到重发响应信号的回调函数，请求方调用sendRespRetry时触发
        void recvRespRetry() override;
    };

    // 定义内存侧请求端口类
    class MemSidePort : public RequestPort {
    private:
        MySimpleCache *owner;
        // 存放需要重发的packet
        PacketPtr blockedPacket;
    public:
        MemSidePort(const std::string &name, MySimpleCache *owner) :
            RequestPort(name), owner(owner), blockedPacket(nullptr) {}
        // 发送请求packet，sendTimingReq的外层封装
        void sendPacket(PacketPtr pkt);
    protected:
        /* RequestPort只有三个纯虚函数需要重写 */
        // 响应接收函数
        bool recvTimingResp(PacketPtr pkt) override;
        // 收到重发请求信号的回调函数，响应方调用sendReqRetry时触发
        void recvReqRetry() override;
        // 收到地址区间的回调函数，响应方调用sendRangeChange时触发
        void recvRangeChange() override;
    };

    // NEW 定义AccessEvent类
    class AccessEvent : public Event
    {
    private:
        MySimpleCache *cache;
        PacketPtr pkt;
    public:
        AccessEvent(MySimpleCache *cache, PacketPtr pkt) :
            Event(Default_Pri, AutoDelete), cache(cache), pkt(pkt)
        { }
        void process() override {
            cache->accessTiming(pkt);
        }
    };
    
    // NEW 命名端口改为向量端口
    // CPUSidePort instPort;
    // CPUSidePort dataPort;
    std::vector<CPUSidePort> cpuPorts;
    MemSidePort memPort;
    // NEW 添加cache数据的存储空间
    std::unordered_map<Addr, uint8_t*> cacheStore;
    // NEW 全局随机数生成器（单例模式）
    Random::RandomPtr random_mt = Random::genRandom();
    // NEW 添加新的缓存参数
    // 读取缓存的延迟
    const Cycles latency;
    // 缓存块大小
    const Addr blockSize;
    // 缓存容量
    const unsigned capacity;
    // 是否阻塞在等待响应
    bool blocked;
    // 正在处理的packet
    PacketPtr outstandingPacket;
    // 正在处理的端口号
    int waitingPortId;
    // NEW 记录发生缓存缺失的时刻
    Tick missTime;

    // NEW 添加端口id参数，并引入访存延迟
    // 处理来自CPU的请求，空闲能处理则返回true，否则false
    bool handleRequest(PacketPtr pkt, int port_id);
    // NEW 将内存响应数据插入cache，并根据原请求地址和大小返回响应数据
    // 处理来自内存的响应，空闲能处理则返回true，否则false
    bool handelResponse(PacketPtr pkt);
    // NEW 发送响应包给CPU侧
    void sendResponse(PacketPtr pkt);
    // functional模式处理packet，请求到响应一条调用链完成，无需处理响应部分
    void handleFunctional(PacketPtr pkt);
    // NEW timing模式处理packet，由事件回调函数调用
    void accessTiming(PacketPtr pkt);
    // NEW functional模式处理packet，实际更新缓存的函数，缓存命中返回true，否则返回false
    bool accessFunctional(PacketPtr pkt);
    // NEW 更新cache数据
    void insert(PacketPtr pkt);
    // 获取属于该memobj的地址区间
    AddrRangeList getAddrRanges() const;
    // 向CPU侧发送所属的内存区间
    void sendRangeChange();
protected:
    // NEW 缓存统计数据
    struct SimpuleCacheStats : public statistics::Group {
        SimpuleCacheStats(statistics::Group *parent);
        statistics::Scalar hits;
        statistics::Scalar misses;
        statistics::Histogram missLatency;
        statistics::Formula hitRatio;
    } stats;
public:
    MySimpleCache(const MySimpleCacheParams &params);
    // 根据请求的端口名字返回相应对象
    Port &getPort(const std::string &if_name, PortID idx = InvalidPortID) override;
};

} // namespace gem5

#endif // __TUTORIALS_MY_SIMPLE_CACHE_HH__