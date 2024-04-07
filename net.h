//
// Created by fly on 2020/7/9.
//

#ifndef BITCOIN_GPU_MINER_NET_H
#define BITCOIN_GPU_MINER_NET_H

#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <memory>
#include <stdio.h>
#include <functional>

#include "common.h"
#include "ease_log.h"
#include "serialize.h"
#include "uint256.h"
#include "hash.h"
#include "version.h"
#include "script/script.h"
#include "amount.h"
#include "utilstrencodings.h"
//#include "tinyformat.h"

using boost::asio::ip::tcp;
using boost::asio::deadline_timer;

static constexpr int16_t TCP_CONNECT_TIMEOUT        = 30;           // 连接超时
static constexpr int16_t TCP_HEARTBEAT_TIMEOUT      = 30 * 60;      // 心跳发送频率
static constexpr int16_t TCP_READ_TIMEOUT           = 90 * 60;      // 读取超时,也就是90分钟后没有收到任何消息则关闭此节点

static constexpr uint8_t REGTEST[] = {0xfa, 0xbf, 0xb5, 0xda};
static constexpr uint8_t EMPTY_BODY_HASH[] = {0x5d,0xf6,0xe0,0xe2};

/** getdata message type flags */
const uint32_t MSG_WITNESS_FLAG = 1 << 30;
const uint32_t MSG_TYPE_MASK = 0xffffffff >> 2;

/** getdata / inv message types.
 * These numbers are defined by the protocol. When adding a new value, be sure
 * to mention it in the respective BIP.
 */
enum GetDataMsg : uint32_t {
    UNDEFINED = 0,
    MSG_TX = 1,
    MSG_BLOCK = 2,
    MSG_WTX = 5,                                      //!< Defined in BIP 339
    // The following can only occur in getdata. Invs always use TX/WTX or BLOCK.
    MSG_FILTERED_BLOCK = 3,                           //!< Defined in BIP37
    MSG_CMPCT_BLOCK = 4,                              //!< Defined in BIP152
    MSG_WITNESS_BLOCK = MSG_BLOCK | MSG_WITNESS_FLAG, //!< Defined in BIP144
    MSG_WITNESS_TX = MSG_TX | MSG_WITNESS_FLAG,       //!< Defined in BIP144
    // MSG_FILTERED_WITNESS_BLOCK is defined in BIP144 as reserved for future
    // use and remains unused.
    // MSG_FILTERED_WITNESS_BLOCK = MSG_FILTERED_BLOCK | MSG_WITNESS_FLAG,
};

///** Time between pings automatically sent out for latency probing and keepalive (in seconds). */
//static const int PING_INTERVAL = 2 * 60;
///** Time after which to disconnect, after waiting for a ping response (or inactivity). */
//static const int TIMEOUT_INTERVAL = 20 * 60;
///** Run the feeler connection loop once every 2 minutes or 120 seconds. **/
//static const int FEELER_INTERVAL = 120;
///** The maximum number of entries in an 'inv' protocol message */
static constexpr unsigned int MAX_INV_SIZE = 50000;
///** The maximum number of new addresses to accumulate before announcing. */
//static const unsigned int MAX_ADDR_TO_SEND = 1000;
///** Maximum length of incoming protocol messages (no message over 4 MB is currently acceptable). */
static constexpr unsigned int MAX_PROTOCOL_MESSAGE_LENGTH = 4 * 1000 * 1000;
///** Maximum length of strSubVer in `version` message */
//static const unsigned int MAX_SUBVERSION_LENGTH = 256;
///** Maximum number of automatic outgoing nodes */
//static const int MAX_OUTBOUND_CONNECTIONS = 8;
///** Maximum number of addnode outgoing nodes */
//static const int MAX_ADDNODE_CONNECTIONS = 8;

static constexpr int SERIALIZE_TRANSACTION_NO_WITNESS = 0x40000000;

static constexpr int WITNESS_SCALE_FACTOR = 4;
/** The maximum allowed size for a serialized block, in bytes (only for buffer size limits) */
static constexpr unsigned int MAX_BLOCK_SERIALIZED_SIZE = 4000000;
/** The maximum allowed weight for a block, see BIP 141 (network rule) */
static constexpr unsigned int MAX_BLOCK_WEIGHT = 4000000;
/** The maximum allowed number of signature check operations in a block (network rule) */
static constexpr int64_t MAX_BLOCK_SIGOPS_COST = 80000;


/** Coinbase transaction outputs can only be spent after this number of new blocks (network rule) */
static const int COINBASE_MATURITY = 100;



/** nServices flags */
enum ServiceFlags : uint64_t {
    // Nothing
    NODE_NONE = 0,
    // NODE_NETWORK means that the node is capable of serving the complete block chain. It is currently
    // set by all Bitcoin Core non pruned nodes, and is unset by SPV clients or other light clients.
    NODE_NETWORK = (1 << 0),
    // NODE_GETUTXO means the node is capable of responding to the getutxo protocol request.
    // Bitcoin Core does not support this but a patch set called Bitcoin XT does.
    // See BIP 64 for details on how this is implemented.
    NODE_GETUTXO = (1 << 1),
    // NODE_BLOOM means the node is capable and willing to handle bloom-filtered connections.
    // Bitcoin Core nodes used to support this by default, without advertising this bit,
    // but no longer do as of protocol version 70011 (= NO_BLOOM_VERSION)
    NODE_BLOOM = (1 << 2),
    // NODE_WITNESS indicates that a node can be asked for blocks and transactions including
    // witness data.
    NODE_WITNESS = (1 << 3),
    // NODE_XTHIN means the node supports Xtreme Thinblocks
    // If this is turned off then the node will not service nor make xthin requests
    NODE_XTHIN = (1 << 4),
    // NODE_NETWORK_LIMITED means the same as NODE_NETWORK with the limitation of only
    // serving the last 288 (2 day) blocks
    // See BIP159 for details on how this is implemented.
    NODE_NETWORK_LIMITED = (1 << 10),

    // Bits 24-31 are reserved for temporary experiments. Just pick a bit that
    // isn't getting used, or one not being used much, and notify the
    // bitcoin-development mailing list. Remember that service bits are just
    // unauthenticated advertisements, so your code must be robust against
    // collisions and other cases where nodes may be advertising a service they
    // do not actually support. Other service bits should be allocated via the
    // BIP process.
};

class AsioWorker {
private:
    std::thread _thread;
    boost::asio::io_context _ioContext;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> _workGuard;
public:
    AsioWorker(): _workGuard(boost::asio::make_work_guard(_ioContext)) {
        _thread = std::thread(&boost::asio::io_context::run, &_ioContext);
    };
    ~AsioWorker() {
        _ioContext.stop();
        _thread.join();
    }
    boost::asio::io_context& IoContext() { return _ioContext; }
};


static const bool DEFAULT_LISTEN = true;
using TBuffer = std::vector<byte>;
class NetMan;

/**
 * p2p 协议的 message 定义
 */

class NetMessageData;

struct NetMessageType {
    const char *VERSION="version";
    const char *VERACK="verack";
    const char *ALERT="alert";
    const char *ADDR="addr";
    const char *INV="inv";
    const char *GETDATA="getdata";

//    const char *MERKLEBGPU_MINER_LOCK="merkleblock";
//    const char *GETBGPU_MINER_LOCKS="getblocks";

    const char *MERKLEBLOCK="merkleblock";
    const char *GETBLOCKS="getblocks";

    const char *GETHEADERS="getheaders";
    const char *TX="tx";
    const char *HEADERS="headers";
    // const char *BGPU_MINER_LOCK="block";
    const char *BLOCK="block";
    const char *GETADDR="getaddr";
    const char *MEMPOOL="mempool";
    const char *PING="ping";
    const char *PONG="pong";
    const char *NOTFOUND="notfound";
    const char *FILTERLOAD="filterload";
    const char *FILTERADD="filteradd";
    const char *FILTERCLEAR="filterclear";
    const char *REJECT="reject";
    const char *SENDHEADERS="sendheaders";
    const char *FEEFILTER="feefilter";
    const char *SENDCMPCT="sendcmpct";
    const char *CMPCTBGPU_MINER_LOCK="cmpctblock";
    const char *GETBGPU_MINER_LOCKTXN="getblocktxn";
    const char *BGPU_MINER_LOCKTXN="blocktxn";
};
extern NetMessageType NetMsgTypeInstance;


class NetAddress {
public:
    uint64_t      _services; // services
    uint8_t       _ip[16]; // in network byte order
    uint16_t      _port;

    std::string Info() {
        return "NetAddress";
    }

    NetAddress(): _services(0), _port(0) {
        memset(_ip, 0, 16);
    }
    NetAddress(const NetAddress& other) {
        _services = other._services;
        memcpy(_ip, other._ip, 16);
        _port = other._port;
    }
    NetAddress& operator= (const NetAddress& other) {
        if (&other == this) {
            return *this;
        }

        _services = other._services;
        memcpy(_ip, other._ip, 16);
        _port = other._port;

        return *this;
    }

    void SetIPV4(const tcp::socket& socket) {
        boost::system::error_code ec;
        auto endpoint = socket.remote_endpoint(ec);
        if (ec) {
            return;
        }

        _port = endpoint.port();
        // endpoint.address().to_v4().to_bytes()
        static const unsigned char pchIPv4[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff };
        memcpy(_ip, pchIPv4, 12);
        memcpy((_ip+12), endpoint.address().to_v4().to_bytes().data(), 4);
    }

    bool IsIPV4() {
        // 格式: {::FFFF:129.144.52.38}
        // "::"是v6ip(80bits,10bytes), "FFFF"是分割符号(16bits,2bytes), 最后(32bits,4bytes)是v4ip地址
        static const unsigned char pchIPv4[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff };
        return memcmp(_ip, pchIPv4, sizeof(pchIPv4)) == 0;
    }

    uint8_t GetByte(int n) {
       return _ip[15-n];
    }

    std::string ToString() {
        std::string buf; buf.resize(32);
        sprintf((char*)buf.data(), "%u.%u.%u.%u", GetByte(3), GetByte(2), GetByte(1), GetByte(0));
        return std::move(buf);
    };



//    bool GetEndpoint(boost::asio::ip::tcp::endpoint &ep) {
//        boost::system::error_code ec;
//        auto addr = boost::asio::ip::make_address(ToString(), ec);
//        if (ec) {
//            AppLog::Error("%s, GetEndpoint error: %s", Info().c_str(), ec.message().c_str());
//            return false;
//        }
//
//        ep = boost::asio::ip::tcp::endpoint(addr, _port);
//        return true;
//    }

    ADD_SERIALIZE_METHODS;
    template<typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(_services);
        READWRITE(_ip);

        unsigned short portN = htons(_port);
        READWRITE(FLATDATA(portN));
        if (ser_action.ForRead())
            _port = ntohs(portN);
    }
};

class NetMessageBase {
protected:
    std::string _command;
    bool        _empty;

    void EmptyHelper(const char* command) {
        _command = command;
        _empty = true;
    }
public:
    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {};

    NetMessageBase(const std::string command, bool empty) : _command(command), _empty(empty) {};
    NetMessageBase() : _empty(false) {};
    virtual ~NetMessageBase(){};

    const std::string& CommandName() const { return _command; }
    bool IsEmptyMessage() const { return _empty; };
};

using NetEmptyMessageBase = NetMessageBase;


class NetMessageHeader {
    const char* _info = "NetMessageHeader";
public:
    static constexpr size_t MESSAGE_START_SIZE = 4;
    static constexpr size_t COMMAND_SIZE = 12;
    static constexpr size_t MESSAGE_SIZE_SIZE = 4;
    static constexpr size_t CHECKSUM_SIZE = 4;
    static constexpr size_t MESSAGE_SIZE_OFFSET = MESSAGE_START_SIZE + COMMAND_SIZE;
    static constexpr size_t CHECKSUM_OFFSET = MESSAGE_SIZE_OFFSET + MESSAGE_SIZE_SIZE;
    static constexpr size_t HEADER_SIZE = MESSAGE_START_SIZE + COMMAND_SIZE + MESSAGE_SIZE_SIZE + CHECKSUM_SIZE;

    char _pchMessageStart[MESSAGE_START_SIZE];
    char _pchCommand[COMMAND_SIZE];
    uint32_t _nMessageSize;
    uint8_t _pchChecksum[CHECKSUM_SIZE];
    const char* Info() const { return _info ;}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(_pchMessageStart);
        READWRITE(_pchCommand);
        READWRITE(_nMessageSize);
        READWRITE(_pchChecksum);
    }

    NetMessageHeader() {};
    NetMessageHeader(NetMessageData& bytes) {
//        AppLog::Debug("%s, 反序列化构造函数", Info());
        Unserialize(bytes);
    }

    NetMessageHeader(const std::string& command,
                     uint32_t nMessageSize,
                     const uint8_t checksum[4]) {
       SetHeader(command, nMessageSize, checksum);
    }


    void SetHeader(const NetMessageHeader& other) {
        memcpy(_pchMessageStart, other._pchMessageStart, MESSAGE_START_SIZE);
        memset(_pchCommand, 0, COMMAND_SIZE);
        memcpy(_pchCommand, other._pchCommand, COMMAND_SIZE);
        _nMessageSize = other._nMessageSize;
        memcpy(_pchChecksum, other._pchChecksum, CHECKSUM_OFFSET);
    }

    void SetHeader( /*const std::string& start,*/
                    const std::string& command,
                    uint32_t nMessageSize,
                    const uint8_t checksum[4]) {

        // 主网络
        _pchMessageStart[0] = 0xf9;
        _pchMessageStart[1] = 0xbe;
        _pchMessageStart[2] = 0xb4;
        _pchMessageStart[3] = 0xd9;

        uint8_t  regtest[] = {0xfa,0xbf,0xb5,0xda};
        // Regtest 网络
        memcpy(_pchMessageStart, regtest, 4);

        memset(_pchCommand, 0, COMMAND_SIZE);
        memcpy(_pchCommand, command.data(), strnlen(command.c_str(), COMMAND_SIZE));
        _nMessageSize = nMessageSize;
        memcpy(_pchChecksum, checksum, CHECKSUM_SIZE);
    }
};

class NetMessageReject : public NetMessageBase {

public:
    // Varies类型数据
    std::string _message;
    int8_t      _code;
    // Varies类型数据
    std::string _reason;

    uint256     _hash; // extra data for “tx” messages or “block” messages

    NetMessageReject() {
        _command = NetMsgTypeInstance.REJECT;
    }

    // 反序列化构造函数
    NetMessageReject(NetMessageData& bytes) {
        _command = NetMsgTypeInstance.REJECT;
        Unserialize(bytes);
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        {
            LimitedString<12> tmp(_message);
            if (ser_action.ForRead()) {
                tmp.Unserialize(s);
            } else {
                tmp.Serialize(s);
            }
        }

        READWRITE(_code);

        {
            // 111 = max Reject length
            LimitedString<111> tmp(_reason);
            if (ser_action.ForRead()) {
                tmp.Unserialize(s);
            } else {
                tmp.Serialize(s);
            }
        }

        if (_message == NetMsgTypeInstance.BLOCK || _message == NetMsgTypeInstance.TX) {
            READWRITE(_hash);
        }
    }
};

class NetMessageVersion : public NetMessageBase {
public:
    int32_t     _version;
    uint64_t    _services;
    int64_t     _timestamp;
    NetAddress  _recvAddr;
    NetAddress  _transAddr;
    uint64_t    _nonce;

    // Varies类型数据
    // 包括: user_agent bytes(user_agent长度, compactSize类型), user_agent(string类型)
    std::string _userAgent;

    int32_t     _startHeight;
    bool        _relay;

    NetMessageVersion() {
        _command = NetMsgTypeInstance.VERSION;
    }

    // 反序列化构造函数
    NetMessageVersion(NetMessageData& bytes) {
        _command = NetMsgTypeInstance.VERSION;
        Unserialize(bytes);
    }


    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(_version);
        READWRITE(_services);
        READWRITE(_timestamp);
        READWRITE(*static_cast<NetAddress*>(&_recvAddr));
        READWRITE(*static_cast<NetAddress*>(&_transAddr));
        READWRITE(_nonce);

        // user_agent
        if (!s.empty()) {
            LimitedString<256> tmp(_userAgent);
            if (ser_action.ForRead()) {
                tmp.Unserialize(s);
            } else {
                tmp.Serialize(s);
            }
        }

        if (!s.empty()) {
            READWRITE(_startHeight);
        }

        if (!s.empty()) {
            READWRITE(_relay);
        }
    }

};

class NetMessageVerack :public NetEmptyMessageBase {
public:
    NetMessageVerack() : NetEmptyMessageBase(NetMsgTypeInstance.VERACK, true) {}
};

class NetMessageGetAddr :public NetEmptyMessageBase {
public:
    NetMessageGetAddr() : NetEmptyMessageBase(NetMsgTypeInstance.GETADDR, true) {}
};

class NetMessageMempool :public NetEmptyMessageBase {
public:
    NetMessageMempool() : NetEmptyMessageBase(NetMsgTypeInstance.MEMPOOL, true) {}
};

class NetMessagePingPong : public NetMessageBase {
public:
    uint64_t _nonce;

    NetMessagePingPong(bool isPing) {
        _command = isPing ? NetMsgTypeInstance.PING : NetMsgTypeInstance.PONG;
    }

    void SwitchPing(bool isPing) {
        _command = isPing ? NetMsgTypeInstance.PING : NetMsgTypeInstance.PONG;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(_nonce);
    }

};


class NetMessageGetBlocks : public NetMessageBase {
public:
    uint32_t                _version;
    std::vector<uint256>    _blockHeaderHashes;
    uint256                 _stopHash;

    NetMessageGetBlocks(const std::string& command = NetMsgTypeInstance.GETBLOCKS) {
        _command = command;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(_version);
        READWRITE(_blockHeaderHashes);
        READWRITE(_stopHash);
    }
};

using NetMessageGetHeaders = NetMessageGetBlocks;








class Inv {
public:
    uint32_t _type;
    uint256 _hash;
    Inv(uint32_t type, const uint256& hash): _type(type), _hash(hash) {}

    Inv() {
//        _command = NetMsgTypeInstance.INV;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(_type);
        READWRITE(_hash);
    }

    // Combined-message helper methods
    bool IsGenTxMsg() const {
        return _type == MSG_TX || _type == MSG_WTX || _type == MSG_WITNESS_TX;
    }
    bool IsGenBlkMsg() const    {
        return _type == MSG_BLOCK || _type == MSG_FILTERED_BLOCK || _type == MSG_CMPCT_BLOCK || _type == MSG_WITNESS_BLOCK;
    }
};

class NetMessageInvs : public NetMessageBase {
public:
    std::vector<Inv> _invs;
    NetMessageInvs(const std::string& command) {
        _command = command;
    }

    void SetCommand(const std::string& command) {
        _command = command;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(_invs);
    }
};

using NetMessageGetData = NetMessageInvs;

class NetMessageAddrs : public NetMessageBase {
public:
    std::vector<NetAddress> _addrs;
    NetMessageAddrs() { _command = NetMsgTypeInstance.ADDR; }

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(_addrs);
    }
};






/**
 * p2p 协议message序列化
 * TODO: 优化使用队列来分解,一次性大内存的场景.
 */
class NetMessageData {
    TBuffer             _data;
    uint32_t            _readPos;
    std::string         _command;
    int                 _nVersion;
public:
    using NetMessageDataQueue = std::deque<NetMessageData>;

    std::string Info(){ return std::move("NetMessageData");};
    int GetVersion() const { return _nVersion; }
    void SetNull() {
        _nVersion = 0;
        _readPos  = 0;
    }
    NetMessageData() { SetNull(); }
    NetMessageData(int version) : _nVersion(version), _readPos(0) {}
    NetMessageData(TBuffer& other, int version) : _data(std::move(other)), _nVersion(version), _readPos(0) {}
    NetMessageData(TBuffer& other) : _data(std::move(other)) { SetNull(); }

    // 拷贝构造函数
    NetMessageData(const NetMessageData& other) :
    _data(other._data), _readPos(0), _nVersion(other._nVersion), _command(other._command) {}

    // 移动构造函数
    NetMessageData(NetMessageData&& other) :
    _data(std::move(other._data)), _readPos(0), _nVersion(other._nVersion), _command(other._command) {}

    void ResetPos() {
        _readPos = 0;
    }

    void ResetBodyPos() {
        _readPos = NetMessageHeader::HEADER_SIZE;
    }

    size_t ParseBodySize() {
        int32_t n = 0;
        memcpy((byte*)&n, &_data[NetMessageHeader::MESSAGE_SIZE_OFFSET], 4);
        return le32toh(n);
    }

    byte* HeaderData() {
        return &_data[0];
    }

    byte* BodyData() {
        return &_data[NetMessageHeader::HEADER_SIZE];
    }

    void Resize(size_t n) {
        if (_data.size() < n) {
            _data.resize(n);
        }
    }

    byte* Data() {
        return &_data[0];
    }

    size_t DataSize() {
        return _data.size();
    }

    const std::string& GetMessageCommand() {
        if (!_command.empty()) {
           return _command;
        }

        if (_data.size() < NetMessageHeader::HEADER_SIZE) {
            return _command;
        }

        auto p = &_data[NetMessageHeader::MESSAGE_START_SIZE];
        _command.insert(_command.end(), p, p + strnlen(p, NetMessageHeader::COMMAND_SIZE));
        return _command;
    }

    void read(byte* pBuf, size_t size) {
        if (size <= 0) return;

        size_t pos = _readPos + size;
        if (pos > _data.size()) return ;

        memcpy(pBuf, &_data[_readPos] , size);

        _readPos += size;

    }

    void write(const byte* pBuf, size_t size) {
        _data.insert(_data.end(), pBuf, pBuf + size);
    }

    void ignore(int size) {
        // Ignore from the beginning of the buffer
        if (size < 0 || _readPos + size > _data.size() ) {
            return;
        }

        _readPos += size;
    }

    bool empty() const {
        return _data.size() == _readPos;
    }

    TBuffer& Ref() { return _data ;}


    template<typename T>
    NetMessageData& operator<<(const T& obj)
    {
        // Serialize to this stream
        ::Serialize(*this, obj);
        return (*this);
    }

    template<typename T>
    NetMessageData& operator>>(T&& obj)
    {
        // Unserialize from this stream
        ::Unserialize(*this, obj);
        return (*this);
    }

    std::string ToString();


};






class NetEventsInterface;
class NetMan;

class Node {
public:
    using TNodeID = uint64_t;

private:
//    unsigned char   _ip[16]; // in network byte order
//    unsigned short  _port; // host order
    TNodeID           _id;
    uint64_t          _nonce;
    const std::string _strHost;
    const int16_t     _iPort;

    boost::asio::io_context&    _ioContext;
    tcp::socket                 _socket;
    std::atomic_bool            _isDisconnect;
    std::atomic_bool            _isHandshakeOK;

    // 连接/读取 超时器
    deadline_timer              _deadline;

    // 心跳定时器
    deadline_timer              _heartbeatTimer;

    // 消息缓冲区
    NetMessageData              _readCacheNetMessage;
    NetMessageData::NetMessageDataQueue _writeNetMessages;

    NetMan*                     _pNetMan;
    NetEventsInterface*         _pMessageProc;




    void OpenConnect();
    void CheckDeadline();

    void DoReadHeader();
    void DoReadBody();
    void DoWrite();

    // 2. 执行bitcoin握手流程
    bool Initialize();

    void PushMessage(const NetMessageData& bytes) {

        boost::asio::post(_ioContext, [this, bytes] {

            bool isWriteIdel = _writeNetMessages.empty();

            _writeNetMessages.push_back(std::move(bytes));

            // 只在队列中只有刚刚入队的数据时，才激活循环队列发送
            if (isWriteIdel) {
                DoWrite();
            }
        });
    }
public:
    NetMessageVersion _version;
    std::string Info();

    TNodeID Id() { return _id;};

    // typedefs
    using TByteBuffer   = std::array<byte, 0x8000>; // 32K
    using TBufferPair   = std::pair<Node*, TByteBuffer>;
    using TNodePtr      = std::shared_ptr<Node>;


    static std::shared_ptr<Node> MakeNode(boost::asio::io_context& ioContext,
                                          const std::string& host,
                                          uint16_t port,
                                          NetMan* pNetMan,
                                          NetEventsInterface* pMessageProc) {
        return std::make_shared<Node>(ioContext, host, port, pNetMan, pMessageProc);
    }

    Node(boost::asio::io_context& ioContext,
         const std::string& host,
         uint16_t port,
         NetMan* pNetMan,
         NetEventsInterface* pMessageProc);

    void Start();
    void Disconnect(bool removeNode);
    bool IsDisconnect() { return _isDisconnect; };
    NetMessageData& GetMessage() { return _readCacheNetMessage; };
    bool isHandshakeOK() { return _isHandshakeOK; }
    void SetHandshakeOK(bool isOK) { _isHandshakeOK = isOK;}

    template <typename TMessage, std::enable_if_t< std::is_base_of<NetMessageBase, TMessage>::value, int> = 0>
    void PushMessage(const TMessage& msg) {

        NetMessageData   bytes;
        NetMessageHeader hdr;

        // 空body体消息
        if (msg.IsEmptyMessage()) {
            hdr.SetHeader(msg.CommandName(), 0, EMPTY_BODY_HASH);
            hdr.Serialize(bytes);
        } else {

            // 序列化body
            NetMessageData bodyBytes;
            msg.Serialize(bodyBytes);

            // Hash body
            uint256 bodyHash = Hash::Hash256(bodyBytes.Ref().begin(), bodyBytes.Ref().end());

            // 重置内存空间
            bytes.Ref().reserve(bodyBytes.DataSize() + NetMessageHeader::HEADER_SIZE);

            // Make headeer
            hdr.SetHeader(msg.CommandName(), bodyBytes.DataSize(), bodyHash.begin());
            hdr.Serialize(bytes);

            // 合并header + body
            bytes.Ref().insert(bytes.Ref().end(), bodyBytes.Ref().begin(), bodyBytes.Ref().end());
        }


        AppLog::Debug("%s, 发送消息 ->>>>: %s", Info().c_str(), hdr._pchCommand);

        // 没有用两次分别发送Header和Body的原因：
        // 因为在Post到asio的队列时会多次产生bytes的副本导致多次拷贝(因为是多线程缘故)
        PushMessage(bytes);
    }
};

/**
 * Interface for message handling
 */
class NetEventsInterface
{
public:
    virtual bool ProcessMessages(Node::TNodeID id) = 0;
    virtual bool SendMessages(Node::TNodeID id) = 0;
    virtual void InitializeNode(Node::TNodeID id) = 0;
    virtual void FinalizeNode(Node::TNodeID id) = 0;

protected:
    /**
     * Protected destructor so that instances can only be deleted by derived classes.
     * If that restriction is no longer desired, this should be made public and virtual.
     */
    ~NetEventsInterface() = default;
};

/**
 * 缓存Node
 */
class NodeCache {
public:
    // using TNodeVector = std::vector<Node::TNodePtr>;
    using TNodeContainer = std::map<Node::TNodeID, Node::TNodePtr>;

private:
    NodeCache() {}
//    NodeCache(const NodeCache& other);

    std::mutex      _vNodesMutex;
    TNodeContainer  _vNodes;

public:
//    static NodeCache& Instance();
    DEF_CLASS_SINGLETON(NodeCache);

    void PushNode(Node::TNodePtr node) {
        GPU_MINER_LOCK(_vNodesMutex);
        _vNodes[node->Id()] =  node;
    }

    void RemoveNode(Node::TNodePtr node) {
        RemoveNode(node.get());
    }

    void RemoveNode(Node* node) {
        GPU_MINER_LOCK(_vNodesMutex);
        _vNodes.erase(node->Id());
    }

    Node::TNodePtr GetNode(Node::TNodeID id) {
        GPU_MINER_LOCK(_vNodesMutex);
        auto it = _vNodes.find(id);
        if (it != _vNodes.end()) {
            return it->second;
        }
        return nullptr;
    }

    template <typename TFN>
    void Each(TFN fn) {
        GPU_MINER_LOCK(_vNodesMutex);
        for(auto it : _vNodes) {
            fn(it.second.get());
        }
    }

    void RemoveAll() {
        GPU_MINER_LOCK(_vNodesMutex);
        _vNodes.clear();
    }

//    const TNodeContainer& GetNodes() {
//        return _vNodes;
//    }
};








class NetMan {
private:
    // 种子节点
    std::vector<std::string> _vSeeds;
    NodeCache&               _vNodes;

    AsioWorker               _sockerAsioWorker;
    AsioWorker               _openConnectAsioWorker;
//    // boost::asio
//    boost::asio::io_context  _ioContextSocket;
//    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> _workGuardSocket;
//
//    boost::asio::io_context  _ioContextOpenConnect;
//    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> _workGuardOpenConnect;

    // threads
//    std::thread              _threadSocketHandler;
//    std::thread              _threadOpenConnectHandler;

    //
    NetEventsInterface*      _pMessageProc;

    // Node Id 生成器基础
    std::atomic_int64_t     _nLastNodeId;

private:
//    void ThreadSocketHandler();
//    void ThreadMessageHandler();
//    void ThreadOpenConnectHandler();

public:

    std::string Info() { return "NetMan";};


    NetMan(NetEventsInterface* pMessageProc);

    void Initialize();
    void Stop();


    static ServiceFlags GetLocalServices() {
       //  ServiceFlags nLocalServices = ServiceFlags(NODE_NETWORK | NODE_WITNESS);
        ServiceFlags nLocalServices = ServiceFlags(NODE_WITNESS);
        // ServiceFlags nLocalServices = ServiceFlags(NODE_NETWORK);
        // ServiceFlags nLocalServices = ServiceFlags(0x04d);
        return nLocalServices;
    };

    static uint32_t  GetLocalVersion() {
        return NO_BLOOM_VERSION;
    }

    int64_t CreateNewNodeId() {
        return _nLastNodeId.fetch_add(1, std::memory_order_relaxed);
    }

    void OpenNode(const std::string& host, uint16_t port);
};


#endif //BITCOIN_GPU_MINER_NET_H
