//
// Created by fly on 2020/7/9.
//

#include "net.h"

#include <iostream>
#include <iomanip>

#include <chrono>
#include <boost/lexical_cast.hpp>
#include <stdio.h>
#include <boost/bind.hpp>
#include <boost/algorithm/hex.hpp>

#include "common.h"
#include "time_utils.h"
#include "ease_log.h"
#include "blockchain.h"

NetMessageType NetMsgTypeInstance;

static inline boost::system::error_code ASIOErrorCheck(const std::string& callName, boost::system::error_code ec) {
    if (ec) {
        AppLog::Error("%s, Asio net error: {%d, %s}", callName.c_str(), ec.value(), ec.message().c_str());
    }
    return ec;
}


/**
 * NetMessageData 实现
 */

std::string NetMessageData::ToString() {
//        return HexStr(_data);
    for(auto i : _data) {
        printf("%02x", i);
    }
    return "";
}



/**
 * Node class 实现
 */

//Node::Node(tcp::endpoint&& endpoint, tcp::socket&& socket)
//:   _endpoint(std::move(endpoint)),
//    _socket(std::move(socket)),
//    _isDisconnect(false)
//{
//    // if (ip) memcpy (_ip, ip, 16);
//}

Node::Node(boost::asio::io_context& ioContext,
           const std::string& host,
           uint16_t port,
           NetMan* pNetMan,
           NetEventsInterface* pMessageProc)
:_ioContext(ioContext),
_socket(ioContext),
_deadline(ioContext),
_heartbeatTimer(ioContext),
_strHost(host),
_iPort(port),
_pNetMan(pNetMan),
_pMessageProc(pMessageProc),
_isDisconnect(false),
_isHandshakeOK(false)
{
    const uint64_t RANDOMIZER_ID_LOCALHOSTNONCE = 0xd93e69e2bbfa5735ULL;
    _id     = pNetMan->CreateNewNodeId();
    // _nonce  = Hash::SipHasher(0x1317, 0x1317).Write(RANDOMIZER_ID_LOCALHOSTNONCE).Write(_id).Finalize();

    // TODO 随机
    _nonce  = Hash::SipHasher(0x0706050403020100ULL, 0x0F0E0D0C0B0A0908ULL).
            Write(RANDOMIZER_ID_LOCALHOSTNONCE).
            Write(_id).Finalize();
}


std::string Node::Info() {
    char buf[128] = {0};
    sprintf(buf, "Node{%s:%d}", _strHost.c_str(), _iPort);
    return std::move(std::string(buf));
}

bool Node::Initialize() {

    NetMessageVersion version;
    version._version        = NetMan::GetLocalVersion();
    version._services       = NetMan::GetLocalServices();
    // version._timestamp      = 0x000000005f3a2bac;// GetTime();
    version._timestamp      = GetTime();
    version._recvAddr.SetIPV4(_socket);
//    uint8_t ip[16] = {00,00,00,00,00,00,00,00,00,00,0xff,0xff,0x6a,0x2f,0x6c,0x42};
//    memcpy(version._recvAddr._ip, ip, 16);
//    version._recvAddr._port = 21007;
    version._recvAddr._services = 0;

    version._transAddr._services = NetMan::GetLocalServices();
    version._nonce = _nonce;
    version._userAgent      = "/fly:0.0.1/";
    version._startHeight    = BlockTipManager::Instance()->GetActiveTip() == nullptr ?
                                0: BlockTipManager::Instance()->GetActiveTip()->_height;
    version._relay          = true;


    PushMessage(version);

    return true;
}



// 打开连接
void Node::OpenConnect() {

    // TODO 记事
    // 以下所有主动调用都是在调用者线程中执行，因为都是异步调用，所以不会阻碍调用者线程
    // 而这些异步调用的回调执行都会在_ioContext线程中执行，asio 让这一切变的简单又好用.

    // 开启超时检查(连接/读取)
    _deadline.async_wait(boost::bind(&Node::CheckDeadline, this));

    // 域名解析
    boost::system::error_code ec;
    tcp::resolver resolver(_ioContext);
    auto endpoints = resolver.resolve(tcp::v4(),_strHost, boost::lexical_cast<std::string>(_iPort), ec);
    if (ec) {
        AppLog::Error("%s, 域名解析失败{%d,%s}", Info().c_str(), ec.value(), ec.message().c_str());
        return ;
    }

    // 开始连接
    AppLog::Info("%s, 开始连接...", Info().c_str());

    // 设置连接超时时间
    _deadline.expires_from_now(boost::posix_time::seconds(TCP_CONNECT_TIMEOUT));

    // 建立 socket 连接
    boost::asio::async_connect(_socket, endpoints, [this](boost::system::error_code ec, tcp::endpoint ep){
//        AppLog::Debug("%s, async_connect异步回调线程id:{%d}",Info().c_str(), GetThreadId());

        if (ec) {
            AppLog::Error("%s, 建立连接失败{%d,%s}", Info().c_str(), ec.value(), ec.message().c_str());

            // 如果不是超时取消，则主动关闭Socket
            if (ec != boost::system::errc::operation_canceled) {
                Disconnect(true);
            }
            return ;
        }

        AppLog::Info("%s, 建立连接成功", Info().c_str());

        // 开始循环读取数据
        DoReadHeader();

        // 初始化节点,发送bitcoin握手消息
        Initialize();
    });
}

// 检查连接与读取数据时的超时
void Node::CheckDeadline() {

    if (_isDisconnect) {
        return;
    }

    // 连接或读取超时了
    // 此处如果expires_at 返回的时间小雨 now 则代表未真正超时，而是调用了expires_from_now而触发了此处理函数
    if (_deadline.expires_at() <= deadline_timer::traits_type::now()) {

        // 定时器 设定为永不超时/不可用状态
        _deadline.expires_at(boost::posix_time::pos_infin);

        AppLog::Debug("%s, 超时关闭Socket", Info().c_str());

        Disconnect(true);
        return;
    }

    _deadline.async_wait(boost::bind(&Node::CheckDeadline, this));
}


// 读取头部数据
void Node::DoReadHeader() {
    if (_isDisconnect) {
        return;
    }

//    AppLog::Info("%s, 开始读取数据Header", Info().c_str());

    // 设置连接超时时间
    _deadline.expires_from_now(boost::posix_time::seconds(TCP_READ_TIMEOUT));

    _readCacheNetMessage.Resize(NetMessageHeader::HEADER_SIZE);
    boost::asio::async_read(_socket,
                            boost::asio::buffer(_readCacheNetMessage.HeaderData(), NetMessageHeader::HEADER_SIZE),
                            [this](boost::system::error_code ec, std::size_t n)
                            {
                                if (ASIOErrorCheck(Info(),ec)) {
                                    Disconnect(true);
                                    return ;
                                }

                                if (_isDisconnect) {
                                    return;
                                }


                                // 如果读取的body size 为0 则认为是无body数据的消息
                                if (_readCacheNetMessage.ParseBodySize() == 0) {

                                    _pMessageProc->ProcessMessages(_id);

                                    DoReadHeader();
                                    return;
                                }


                                // 开始读取完整的消息数据
                                DoReadBody();
                            }
    );
}

// 读取整体消息数据
void Node::DoReadBody() {
    if (_isDisconnect) {
        return;
    }

    size_t size = _readCacheNetMessage.ParseBodySize();
//    AppLog::Info("%s, 开始读取数据{Body Size: %d}", Info().c_str(), size);
    if (size <= 0 || size > MAX_PROTOCOL_MESSAGE_LENGTH) {
        AppLog::Error("%s, 读取的Body Size 超出范围: %d", Info().c_str(), size);

        // 开启下一条消息的读取
        Disconnect(true);
        return;
    }

    _readCacheNetMessage.Resize(_readCacheNetMessage.DataSize() + size);
    boost::asio::async_read(_socket,
                            boost::asio::buffer(_readCacheNetMessage.BodyData(), size),
                            [this](boost::system::error_code ec, std::size_t n) {

                                if (ASIOErrorCheck(Info(), ec)) {
                                    Disconnect(true);
                                    return ;
                                }

//                                AppLog::Debug("%s, 读取到Body数据{%d,%0X}", Info().c_str(),
//                                              _readCacheNetMessage.ParseBodySize(), _readCacheNetMessage.BodyData());


                                if (_isDisconnect) {
                                    return;
                                }

                                _pMessageProc->ProcessMessages(_id);

                                // 开启下一条消息的读取
                                DoReadHeader();
                            }
    );
}

void Node::Disconnect(bool removeNode) {



    if (_isDisconnect) {
        return;
    }

    _isDisconnect = true;
    AppLog::Info("%s, 关闭Socket", Info().c_str());


    // TODO 以下移除操作可直接用RemoveNode即可，随着Node被移除，则Node被销毁的同时也会销毁其内部的其它构件。
    boost::system::error_code ignored_ec;
    _socket.cancel(ignored_ec);
    _socket.close(ignored_ec);
    _deadline.cancel(ignored_ec);
    _heartbeatTimer.cancel(ignored_ec);

    if (removeNode) {
        // 从node缓存中移除此节点
        AppLog::Info("%s, 从NodeCache中移除此Node", Info().c_str());
        NodeCache::Instance()->RemoveNode(this);
    }
}

void Node::Start() {
    OpenConnect();
}

void Node::DoWrite() {

    if (_isDisconnect) {
        return;
    }

    boost::asio::async_write(
            _socket,
            boost::asio::buffer(_writeNetMessages.front().Data(), _writeNetMessages.front().DataSize()),
            [this](boost::system::error_code ec, size_t writeSize) {

                if (ec) {
                    AppLog::Debug("%s, 发送数据失败{%d,%s}", Info().c_str(), ec.value(), ec.message().c_str());
                    Disconnect(true);
                    return ;
                }

//                AppLog::Debug("%s, 发送数据成功{%d, 消息:%s}", Info().c_str(), writeSize,  _writeNetMessages.front().GetMessageCommand().c_str());

                _writeNetMessages.pop_front();
                if (!_writeNetMessages.empty()) {
                    DoWrite();
                }
            }
    );
}


/**
 *
 * NodeCache class 实现
 */
//NodeCache& NodeCache::Instance() {
//    static NodeCache instance;
//    return instance;
//}
IMP_CLASS_SINGLETON(NodeCache);


//NodeCache::NodeCache(const NodeCache &other) {
//
//}


/**
 * NetMan class 实现
 */
NetMan::NetMan(NetEventsInterface* pMessageProc)
        : _vNodes(*NodeCache::Instance()),
//          _workGuardSocket(boost::asio::make_work_guard(_ioContextSocket)),
//          _workGuardOpenConnect(boost::asio::make_work_guard(_ioContextOpenConnect)),
          _pMessageProc(pMessageProc),
          _nLastNodeId(0)
{


//    _vSeeds.emplace_back("94.130.11.6"); // Pieter Wuille, only supports x1, x5, x9, and xd
    _vSeeds.emplace_back("seed.bitcoin.sipa.be"); // Pieter Wuille, only supports x1, x5, x9, and xd
    _vSeeds.emplace_back("dnsseed.bluematt.me"); // Matt Corallo, only supports x9
//    _vSeeds.emplace_back("dnsseed.bitcoin.dashjr.org"); // Luke Dashjr
//    _vSeeds.emplace_back("seed.bitcoinstats.com"); // Christian Decker, supports x1 - xf
//    _vSeeds.emplace_back("seed.bitcoin.jonasschnelli.ch"); // Jonas Schnelli, only supports x1, x5, x9, and xd
//    _vSeeds.emplace_back("seed.btc.petertodd.org"); // Peter Todd, only supports x1, x5, x9, and xd
//    _vSeeds.emplace_back("seed.bitcoin.sprovoost.nl"); // Sjors Provoost
}

void NetMan::Initialize() {

    // 启动socket处理线程
//    _threadSocketHandler = std::thread(&boost::asio::io_context::run, &_ioContextSocket);

    // 启动open connect 处理线程
//    _threadOpenConnectHandler = std::thread(&boost::asio::io_context::run, &_ioContextOpenConnect);

    // 等待线程启动完成
    AppLog::Info("%s, 等待线程创建完成", Info().c_str());
    std::this_thread::sleep_for(std::chrono::seconds(1));

//     auto pNode = Node::MakeNode(_ioContextSocket, _vSeeds.at(0), 8333, this, _pMessageProc);
//    auto pNode = Node::MakeNode(_ioContextSocket, "localhost", 3344, this, _pMessageProc);


    // Regtest 网络
//    auto pNode = Node::MakeNode(_ioContextSocket, "localhost", 18444, this, _pMessageProc);
//    _vNodes.PushNode(pNode);
//
//    pNode->Start();

    OpenNode("localhost", 18444);
//    OpenNode("192.168.165.149", 18444);

}



void NetMan::Stop() {

    AppLog::Debug("%s, Stop", Info().c_str()) ;
    _vNodes.Each([](Node* pNode){
       pNode->Disconnect(false);
    });


//    for (auto node : _vNodes.GetNodes()) {
//        node.second->Disconnect(false);
//    }
//    _vNodes.GetNodes().clear();

//    _ioContextSocket.stop();
//    _ioContextOpenConnect.stop();
//    _threadSocketHandler.join();
//    _threadOpenConnectHandler.join();
}

// 主动连接一个节点
void NetMan::OpenNode(const std::string& host, uint16_t port) {

//    boost::asio::post(_ioContextOpenConnect, [this, host, port](){
//        auto pNode = Node::MakeNode(_ioContextSocket, host, port, this, _pMessageProc);
//        _vNodes.PushNode(pNode);
//        pNode->Start();
//    });

    boost::asio::post(_openConnectAsioWorker.IoContext(), [this, host, port](){
        auto pNode = Node::MakeNode(_sockerAsioWorker.IoContext(), host, port, this, _pMessageProc);
        _vNodes.PushNode(pNode);
        pNode->Start();
    });
}
