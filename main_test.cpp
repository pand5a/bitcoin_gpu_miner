//
// Created by fly on 2020/8/4.
//
#include <iostream>
#include <boost/asio.hpp>
#include <thread>
#include <queue>
#include <mutex>
#include <boost/algorithm/hex.hpp>
#include <boost/interprocess/streams/bufferstream.hpp>
#include <sstream>
#include <iostream>
#include <array>

#include "ease_log.h"
#include "common.h"

using boost::asio::ip::tcp;
using TBuffer = std::vector<byte>;

class EaseNode;
using TEaseNodePtr = std::shared_ptr<EaseNode>;
using TNodeList = std::list<TEaseNodePtr>;

TNodeList nodeList;
std::mutex nodeListMutex;

void RemoveNode(EaseNode* node) {
    GPU_MINER_LOCK(nodeListMutex);

    nodeList.erase(remove_if(nodeList.begin(), nodeList.end(), [node](TEaseNodePtr  a) -> bool{
        if (node == a.get()) {
            return true;
        }

        return false;
    }), nodeList.end());

};

//
//TBuffer HexToBytes(const TBuffer & hex) {
//    TBuffer bytes;
//
//    for (unsigned int i = 0; i < hex.length(); i += 2) {
//        std::string byteString = hex.substr(i, 2);
//        byte b = (byte)strtol(byteString.c_str(), NULL, 16);
//        bytes.push_back(b);
//    }
//
//    return std::move(bytes);
//}

boost::asio::io_context ioContext;
class EaseNode {
    tcp::socket _socket;
    std::deque<TBuffer> _sendMessageQueue;
    std::mutex _sendMessageQueueMutex;
    std::array<byte, 0x8000> _buf;
    void DoWrite() {

        boost::asio::async_write(
                _socket,
                boost::asio::buffer(_sendMessageQueue.front().data(), _sendMessageQueue.front().size()),
                [this](boost::system::error_code ec, size_t sendSize) {

                    if (ec) {
                        // AppLog::Error("MainTest, 写入数据错误: {%d, %s}", ec.value(), ec.message().c_str());
                        RemoveNode(this);
                        return;
                    }
                    AppLog::Debug("MainTest, 写入数据成功Size:{%d}", sendSize);

                    std::cout << "msgs count: " << _sendMessageQueue.size() << std::endl;
                    _sendMessageQueue.pop_front();
                    if (_sendMessageQueue.empty()) {
                        return;
                    }

                    DoWrite(); // 第二次调用是在 ioContext线程中
                });
    }

    void DoRead() {

        // byte buf[0x8000] = {0};
        _socket.async_read_some(
                boost::asio::buffer(_buf, _buf.size()),
                [this](boost::system::error_code ec, size_t size) {

                    if (ec) {
                        AppLog::Error("MainTest, 读取错误,删除此节点: {%d, %s}", ec.value(), ec.message().c_str());
                        RemoveNode(this);
                        return ;
                    }

//                    if (_buf[size-2] == '\r' && _buf[size-1] == '\n') {
//                        AppLog::Debug("%s, 读取数据，并删除 \\r\\n", "MainTest");
//                        size -= 2;
//                    }

                    // copy 一份
//                    std::vector<byte> tmp(_buf.begin(), _buf.begin() + size);
                    std::string str;
                    str.resize(size);
                    memcpy((char*)str.data(), &_buf[0], size);
                    auto sstr = boost::algorithm::hex(str);


                    AppLog::Debug("MainTest, 读取到数据: {Size:%d, 0x%s, ascii:%s}", size, sstr.c_str(), _buf);

                    DoRead();
                });
    }
public:
    EaseNode(tcp::socket socket) : _socket(std::move(socket)) {};
    ~EaseNode(){
        AppLog::Debug("%s, ~EaseNode", "MainTest");
    }
    void PushMessage(const TBuffer & msg) {
        boost::asio::post(ioContext,
                          [this, msg]
                          {
                              // 此处做如果队列内还有未完成发送的，则只是向队列中添加内容，不再调用DoWrite
                              bool write_in_progress = !_sendMessageQueue.empty();
                              _sendMessageQueue.push_back(msg);
                              if (!write_in_progress) {
                                  DoWrite(); // 第一次调用是在main线程中
                              }
                          });
    }

    void Start() {
       DoRead();
    }

    void Stop() {
        RemoveNode(this);
    }
};




int main(int argc, char** argv) {

    std::cout << "this is test main exe" << std::endl;




    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> _workGuard(boost::asio::make_work_guard(ioContext));
    auto serverThread = std::thread(&boost::asio::io_context::run, &ioContext);


    tcp::endpoint endpoint(tcp::v4(), 3344);
    tcp::acceptor acceptor(ioContext, endpoint);
    std::function<void()> funcDoAccept = [&]{
        acceptor.async_accept([&](boost::system::error_code ec, tcp::socket socket) {

            if (ec) {
                AppLog::Error("MainTest, Accept 错误: {%d, %s}", ec.value(), ec.message().c_str());
            } else {
                boost::asio::ip::tcp::endpoint remote_ep = socket.remote_endpoint();
                boost::asio::ip::address remote_ad = remote_ep.address();
                std::string s = remote_ad.to_string();
                AppLog::Debug("MainTest, 新的连接: {%s,%d}", s.c_str(), remote_ep.port());

                GPU_MINER_LOCK(nodeListMutex);
                auto node = std::make_shared<EaseNode>(std::move(socket));
                node->Start();

                nodeList.push_back(node);
            }

            funcDoAccept();
        });

    };
    funcDoAccept();



    auto funcDefaultNode = [&]() -> TEaseNodePtr {
        GPU_MINER_LOCK(nodeListMutex);
        if (nodeList.empty()) {
            return nullptr;
        }
        return nodeList.front();
    };

//    std::string aa("aa");
//    size_t n = aa.size();
//    AppLog::Debug("%s, len:%d, n:%d", "MainTest", n, aa.length());

//    constexpr size_t lineSize = 1024 * 10;
//    char line[lineSize];
    std::string line;
    while(true) {
        std::cin >> line;
        if (line == "exit") {
            break;
        }


//        TBuffer sendBuf;
//        boost::algorithm::unhex(sendHexData.begin(), sendHexData.end(), std::back_inserter(sendBuf));





//        auto data = line.c_str();
//        size_t size = line.size();

//        if (data[size-2] == '\r' && data[size-1] == '\n') {
//            AppLog::Debug("%s, 读取数据，并删除 \\r\\n", "MainTest");
//            size -= 2;
//        }


        TBuffer sendBuf;
        boost::algorithm::unhex(line.begin(), line.begin() + line.size(), std::back_inserter(sendBuf));

        auto node = funcDefaultNode();
        if (node == nullptr) {
            AppLog::Info("%s, 没有默认连接", "MainTest");
            continue;
        }


        if (line == "close") {
            // 关闭 default 连接
            node->Stop();
            continue;
        }

        // size_t n = sizeof("F9BEB4D976657273696F6E000000000066000000C8B973F07F1101000D0400000000000004D9375F000000000D0000000000000000000000000000000000FFFF5E820B06208D0D040000000000000000000000000000000000000000000000009494176837ABA99B102F5361746F7368693A302E31362E302FD173000001");
        // std::string tmpa = "F9BEB4D976657273696F6E000000000066000000C8B973F07F1101000D0400000000000004D9375F000000000D0000000000000000000000000000000000FFFF5E820B06208D0D040000000000000000000000000000000000000000000000009494176837ABA99B102F5361746F7368693A302E31362E302FD173000001";
//        std::string tmpa = "F9BEB4D976657273696F6E000000000066000000C8B973F0";
//        TBuffer buff(tmpa.size());
//        memcpy(&buff[0],tmpa.data(), tmpa.size());

        // buff.insert(buff.end(), (byte*)"F9BEB4D976657273696F6E000000000066000000C8B973F07F1101000D0400000000000004D9375F000000000D0000000000000000000000000000000000FFFF5E820B06208D0D040000000000000000000000000000000000000000000000009494176837ABA99B102F5361746F7368693A302E31362E302FD173000001", n);

//        memcpy(&buff[0], data, size);


//        node->PushMessage(sendBuf);

        node->PushMessage(sendBuf);
//        for(auto i = 0; i < 100000; ++i) {
//            AppLog::Debug("%s, Loop: %d", "MainTest", i);
//        }
    }


    ioContext.stop();
    serverThread.join();

    return 0;
}