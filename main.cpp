#include <iostream>


#include <vector>

#include <boost/asio.hpp>
#include <utility>
#include <boost/algorithm/hex.hpp>
//#include "btc_sha256.h"
#include "btc_sha256_gpu.h"
#include "sha256_cpu.h"
#include "time_utils.h"
#include "cmdline.h"
//#include "queue.h"

#include "ease_log.h"

#include "peer_logic_man.h"
#include "hash.h"

#include "block.h"

#include "blockchain.h"

#include "params.h"
//#include "time_utils.h"

static cmdline::parser _sCmdline;


void parserCmdline(int argc, char* argv[]) {
    _sCmdline.add("daemon", 'd', "running in the background");
    _sCmdline.add("debug", '\0', "show debug info");
    _sCmdline.parse_check(argc, argv);
}


// using TByteBuffer = std::vector<unsigned char>;

//Queue<Node::TBufferPair> queue(4096);


//void ff() {
//    Node::TByteBuffer a {"wfewfweew"};
//    Node::TBufferPair c(nullptr, a);
//    queue.push(c);
//}

class FF {
public:


    std::string c = "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF";

    FF() {
        AppLog::Debug("FF, %s", "模认");
    }

    FF(const FF& o) {
        AppLog::Debug("FF, %s", "拷贝");
        c = o.c;
    }

    FF(FF&& o) {
        AppLog::Debug("FF, %s,","移动");
        c = std::move(o.c);
    }

    const std::string & CC() const {
        return c;
    }

    std::string& Get() {
        return c;
    }
};

std::thread test1(FF& ff) {
   std::string aa="aa";

   std::string mc("iiiiiiiiiiii");


   auto a = std::thread([aa, cc{FF(ff)}]{

        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << aa << ", ff:" << cc.c << std::endl;
   });

    return a;
}

void TestVersion() {
    NetMessageVersion version;
    version._version        = 70015;
    version._services       = 0x0000000000000409;
    version._timestamp      = 0x000000005f3a2bac;// GetTime();
    // version._recvAddr.SetIPV4(_socket);
    uint8_t ip[16] = {00,00,00,00,00,00,00,00,00,00,0xff,0xff,0x6a,0x2f,0x6c,0x42};
    memcpy(version._recvAddr._ip, ip, 16);
    version._recvAddr._port = 21007;
    version._recvAddr._services = 0;

    version._transAddr._services = 0x0000000000000409;

    // version._nonce = _nonce;
    version._nonce = 0x7837504de492c88d;
    version._userAgent      = "/Satoshi:0.20.0/";
    version._startHeight    = 644100;
    version._relay          = true;

    // header
    // version.SetHeader(NetMsgTypeInstance.VERSION, )

    NetMessageData bodyBytes;
    version.Serialize(bodyBytes);


    std::string hex;
    boost::algorithm::hex(bodyBytes.Ref().begin(), bodyBytes.Ref().end(), std::back_inserter(hex));


    std::cout << "Hex: " << hex << std::endl;


    std::cout << "Hash: ";
    uint256 hash = Hash::Hash256(bodyBytes.Ref().begin(), bodyBytes.Ref().end());
    for (auto it = hash.begin(); it != hash.end(); ++it){
        printf("%02x", *it);
    }
    std::cout << std::endl;





//    NetMessageHeader hdr;
//    hdr.SetHeader(std::string(NetMsgTypeInstance.VERSION), bodyBytes.DataSize(), hashBlock);
//    NetMessageData headerBytes;
//    hdr.Serialize(headerBytes);
//
//    headerBytes.Resize(headerBytes.DataSize() + bodyBytes.DataSize());
}


void WaitForShutdown() {

    std::string cmd;

    while (true) {
        std::cin >> cmd;
        if (cmd == "exit") {
            break;
        }

        if (cmd == "createBlock") {
           // TODO 创建一个区块
        }
    }
}

int main(int argc, char* argv[]) {
    parserCmdline(argc, argv);

    PeerLogicMan logicMan;
    logicMan.Initialize();
    WaitForShutdown();
    return 0;



//    BTC_SHA256_GPU::Instance()->CPUPperformance();
//    BTC_SHA256_GPU::Instance()->GPUPperformance();

    return 0;
}
