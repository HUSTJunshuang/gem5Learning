#ifndef __TUTORIALS_MY_SIMPLE_MEMOBJ_HH__
#define __TUTORIALS_MY_SIMPLE_MEMOBJ_HH__

#include "mem/port.hh"
#include "params/MySimpleMemobj.hh"
#include "sim/sim_object.hh"

namespace gem5 {

class MySimpleMemobj : public SimObject {
private:
    // 定义CPU侧响应端口类
    class CPUSidePort : public ResponsePort {
    private:
        // 所有者变量，用于调用所有者的函数
        MySimpleMemobj *owner;
        // 是否需要重试
        bool needRetry;
        // 存放需要重发的packet
        PacketPtr blockedPacket;
    public:
        CPUSidePort(const std::string &name, MySimpleMemobj *owner) :
            ResponsePort(name), owner(owner) {}
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
        MySimpleMemobj *owner;
        // 存放需要重发的packet
        PacketPtr blockedPacket;
    public:
        MemSidePort(const std::string &name, MySimpleMemobj *owner) :
            RequestPort(name), owner(owner) {}
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
    
    CPUSidePort instPort;
    CPUSidePort dataPort;
    MemSidePort memPort;
    // 是否阻塞在等待响应
    bool blocked;

    // 处理来自CPU的请求，空闲能处理则返回true，否则false
    bool handleRequest(PacketPtr pkt);
    // 处理来自内存的响应，空闲能处理则返回true，否则false
    bool handelResponse(PacketPtr pkt);
    // functional模式处理packet，请求到响应一条调用链完成，无需处理响应部分
    void handleFunctional(PacketPtr pkt);
    // 获取属于该memobj的地址区间
    AddrRangeList getAddrRanges() const;
    // 向CPU侧发送所属的内存区间
    void sendRangeChange();
public:
    MySimpleMemobj(const MySimpleMemobjParams &params);
    // 根据请求的端口名字返回相应对象
    Port &getPort(const std::string &if_name, PortID idx = InvalidPortID) override;
};

} // namespace gem5

#endif // __TUTORIALS_MY_SIMPLE_MEMOBJ_HH__