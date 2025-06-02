#include "tutorials/part2/my_simple_memobj.hh"
#include "debug/MySimpleMemobj.hh"

namespace gem5{

MySimpleMemobj::MySimpleMemobj(const MySimpleMemobjParams &params) :
    SimObject(params),
    instPort(params.name + ".inst_port", this),
    dataPort(params.name + ".data_port", this),
    memPort(params.name + ".mem_side", this),
    blocked(false) {}

Port &MySimpleMemobj::getPort(const std::string &if_name, PortID idx) {
    panic_if(idx != InvalidPortID, "This object doesn't support vector ports");
    // 根据请求的端口名字返回相应对象，if_name即Python中声明的端口参数名
    if (if_name == "mem_side") {
        return memPort;
    }
    else if (if_name == "inst_port") {
        return instPort;
    }
    else if (if_name == "data_port") {
        return dataPort;
    }
    else {
        // 传递给超类
        return SimObject::getPort(if_name, idx);
    }
}

bool MySimpleMemobj::handleRequest(PacketPtr pkt) {
    if (blocked) {
        return false;
    }
    DPRINTF(MySimpleMemobj, "Got request for addr %#x\n", pkt->getAddr());
    blocked = true;
    memPort.sendPacket(pkt);
    return true;
}

bool MySimpleMemobj::handelResponse(PacketPtr pkt) {
    assert(blocked);
    DPRINTF(MySimpleMemobj, "Got response for addr %#x\n", pkt->getAddr());
    blocked = false;
    // 根据请求类型分发到对应端口
    if (pkt->req->isInstFetch()) {
        instPort.sendPacket(pkt);
    }
    else {
        dataPort.sendPacket(pkt);
    }
    // 此时可以继续处理其他请求，告知CPU
    instPort.trySendRetry();
    dataPort.trySendRetry();

    return true;
}

void MySimpleMemobj::handleFunctional(PacketPtr pkt) {
    memPort.sendFunctional(pkt);
}

AddrRangeList MySimpleMemobj::getAddrRanges() const {
    DPRINTF(MySimpleMemobj, "Sending new ranges\n");
    return memPort.getAddrRanges();
}

void MySimpleMemobj::sendRangeChange() {
    instPort.sendRangeChange();
    dataPort.sendRangeChange();
}

/* CPUSidePort相关函数 */

void MySimpleMemobj::CPUSidePort::sendPacket(PacketPtr pkt) {
    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");
    if (!sendTimingResp(pkt)) {
        blockedPacket = pkt;
    }
}
AddrRangeList MySimpleMemobj::CPUSidePort::getAddrRanges() const {
    return owner->getAddrRanges();
}
void MySimpleMemobj::CPUSidePort::trySendRetry() {
    if (needRetry && blockedPacket == nullptr) {
        needRetry = false;
        DPRINTF(MySimpleMemobj, "Sending retry req for %d\n", id);
        sendRetryReq();
    }
}
void MySimpleMemobj::CPUSidePort::recvFunctional(PacketPtr pkt) {
    return owner->handleFunctional(pkt);
}
bool MySimpleMemobj::CPUSidePort::recvTimingReq(PacketPtr pkt) {
    if (!owner->handleRequest(pkt)) {
        needRetry = true;
        return false;
    }
    else {
        return true;
    }
}
void MySimpleMemobj::CPUSidePort::recvRespRetry() {
    assert(blockedPacket != nullptr);
    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    sendPacket(pkt);
}

/* MemSidePort相关函数 */

void MySimpleMemobj::MemSidePort::sendPacket(PacketPtr pkt) {
    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");
    if (!sendTimingReq(pkt)) {
        blockedPacket = pkt;
    }
}
bool MySimpleMemobj::MemSidePort::recvTimingResp(PacketPtr pkt) {
    return owner->handelResponse(pkt);
}
void MySimpleMemobj::MemSidePort::recvReqRetry() {
    assert(blockedPacket != nullptr);
    // 读取阻塞的packet
    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;
    sendPacket(pkt);
}
void MySimpleMemobj::MemSidePort::recvRangeChange() {
    owner->sendRangeChange();
}

} // namespace gem5