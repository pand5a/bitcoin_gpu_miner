//
// Created by fly on 2020/7/26.
//

#ifndef BITCOIN_GPU_MINER_PEER_LOGIC_MAN_H
#define BITCOIN_GPU_MINER_PEER_LOGIC_MAN_H

#include "net.h"
#include <boost/asio.hpp>
#include <deque>
#include <atomic>
#include "blockchain.h"
#include "keyio.h"

#include "btc_sha256_gpu.h"

// 采用 Headers-First 方式
// 暂时只向一个sync节点同步
class InitialBlockDownloadManager {
    std::set<uint256>   _hashIBDBlocks;
    std::deque<uint256> _hashSendingGetData;
    std::set<uint256>   _hashDownloadingBlock;
    bool                _isFinish;
    bool                _isGetHeaders;
    bool                _isFirstHeaders;

    using TNotifyIBDCompleteCallback = std::function<void()>;
    TNotifyIBDCompleteCallback _notifyIBDCompleteCallback;

    void PushGetHeadersMessage(Node::TNodePtr node) {
        // TODO 发送 getheaders消息
        NetMessageGetHeaders getHeaders(NetMsgTypeInstance.GETHEADERS);

        auto tipBlock = BlockTipManager::Instance()->GetActiveTip();
        getHeaders._blockHeaderHashes.push_back(tipBlock->_hash);
        getHeaders._version = NetMan::GetLocalVersion();
        node->PushMessage(getHeaders);
    }

    void PushGetDataMessage(Node::TNodePtr node) {
        NetMessageInvs getData(NetMsgTypeInstance.GETDATA);
        for (int i=0; !_hashSendingGetData.empty() && i < 16; ++i) {
            const auto& hash = _hashSendingGetData.front();
            getData._invs.push_back(Inv(GetDataMsg::MSG_WITNESS_BLOCK, hash));

            _hashDownloadingBlock.insert(hash);

            _hashSendingGetData.pop_front();

            AppLog::Debug("%s, GetData hashBlock: %s", "IBD", hash.ToString().c_str());
        }

        if (!getData._invs.empty())
            node->PushMessage(getData);
    }

    void IBDComplete() {
        auto tip = BlockTipManager::Instance()->GetActiveTip();
        AppLog::Info("%s, IBD已完成, 当前区块高度为:{%d, %s}", Info, tip->_height, tip->_hash.ToString().c_str());

        if (_notifyIBDCompleteCallback != nullptr) {
            _notifyIBDCompleteCallback();
        }
    }

public:
    const char* Info = "InitialBlockDownloadManager";
    InitialBlockDownloadManager() : _isFinish(false), _isGetHeaders(false), _isFirstHeaders(true), _notifyIBDCompleteCallback(nullptr) {}

    bool IsFinish() {
        return _isFinish;
    }

    bool IsIBDBlocks(const uint256& hash) {
       return _hashIBDBlocks.count(hash) > 0;
    }

    // to 开始IBD
    void Run(Node::TNodePtr node) {
        auto tip = BlockTipManager::Instance()->GetActiveTip();
        AppLog::Info("%s, 运行IBD, 当前区块高度为:{%d, %s}", Info, tip->_height, tip->_hash.ToString().c_str());
        PushGetHeadersMessage(node);
    }

    // 返回true 代表 IBD完成
    bool Headers(Node::TNodePtr node, NetMessageData& s) {

        if (IsFinish()) {
            return true;
        }

        BlockHeaders headers;
        s >> headers;

        // headers 为空，代表已获取完成
        if (headers.empty()) {
            _isGetHeaders = true;

            // 如果第一次获取headers就为空,则认为已完成IBD
            if (_isFirstHeaders) {
                IBDComplete();
                _isFinish = true;
            }

            return true;
        }

        _isFirstHeaders = false;

        // 保存
        for (const auto& header : headers) {
            const auto& hash = header->GetHash();
            _hashIBDBlocks.insert(hash);
            _hashSendingGetData.push_back(hash);

            AppLog::Debug("%s, Headers hashBlock: %s", "IBD", hash.ToString().c_str());
        }

        // 获取区块
        PushGetDataMessage(node);

        // 2000 为Headers最大消息承载量，如果小于这个数，则认为对方只剩余此些数据
        // TODO ??如果最后一次正是2000时??
        if (headers.size() < 2000) {
            // TODO 暂定完成
            _isGetHeaders = true;
        } else {
            PushGetHeadersMessage(node); // 再次获取 Headers
        }
        return false;
    }

    // 返回true 代表 IBD完成
    bool Blocks(Node::TNodePtr node, NetMessageData& s) {
        if (IsFinish()) {
            return true;
        }

        Block::TCBlockPtr block;
        s >> block;

        const auto& hash = block->GetHash();
        const auto& it = _hashIBDBlocks.find(hash);
        if (it == _hashIBDBlocks.end()) {
            // 不是IBD的区块，则忽略
            AppLog::Error("%s, Not IBD blocks, hashBlock: %s", "IBD", hash.ToString().c_str());
            return false;
        }

        // 校验区块
        // 顺序与保存到本地的顺序一至
        if (!BlockChain::Instance()->AcceptBlock(block)) {
            return false;
        }

        // 移除已下载完成的区块
        _hashIBDBlocks.erase(it); // 需要下载的区块

        _hashDownloadingBlock.erase(hash); // 正在下载的区块

        // 已完成IBD
        if (_isGetHeaders && _hashIBDBlocks.empty()) {
            _isFinish = true;
            IBDComplete();
        } else {
            if (_hashDownloadingBlock.empty()) {
                PushGetDataMessage(node);
            }
        }

        return false;
    }

    void SetIBDCompleteCallback(TNotifyIBDCompleteCallback cb) {
        _notifyIBDCompleteCallback = cb;
    }
};

class Miner {
    enum {
        RUNING = 1,
        RESET_MINING,
        STOP_MINING,
    };
private:
    const char*      Info = "Miner";

    // 临时接受挖矿所得的地址 WP2PKH
    CScript          _minerAddress;

    AsioWorker       _asioWorker;
    std::atomic_char _state;
    bool             _isStarted;
    uint256          _miningPrevHash;
    std::mutex       _miningPrevHashMtx;

    Miner() : _state(RESET_MINING),
              _isStarted(false) {

        // 钱包地址 WP2PKH
        const char* addr = "bcrt1qwgnsjmzlrtmf27z2f20shhlr70vgfzuq554ssd";
        CTxDestination destination =  Keyio::Instance()->DecodeDestination(addr);
        _minerAddress = GetScriptForDestination(destination);
    }


    /**
     * 挖矿，暂时只修改Nonce
     * @param block
     * @return
     */
    bool DoMiner(Block::TBlockPtr block) {
        if (block->_nNonce == std::numeric_limits<uint32_t>::max()) {
            AppLog::Debug("%s, DoMiner, nonce 已超出范围", Info);
            return false;
        }

        auto hash = block->GetHash();
        if (BlockChain::CheckProofOfWork(hash, block->_nBits)) {
            AppLog::Info("%s, DoMiner, 挖矿成功: %s", Info, block->ToString().c_str());

            if (BlockChain::Instance()->AcceptBlock(block)) {
                // 广播
                NodeCache::Instance()->Each([&](Node *pNode) {
                    pNode->PushMessage(*block);
                });

            } else {
                AppLog::Error("%s, DoMiner, AcceptBlock error: %s", Info, block->ToString().c_str());
                // 丢弃本块，重新挖另一个块
            }

            // 进行下一次挖矿
            ResetMining();
            return true;
        }

        ++(block->_nNonce);

        return false;
    }

public:
    DEF_CLASS_SINGLETON(Miner);
    ~Miner() {
        Stop();
    }

    uint256 GetMiningPrevHash() {
        std::unique_lock<std::mutex> lck(_miningPrevHashMtx);
        return _miningPrevHash;
    }

    void ResetMining() {
        // CPU 重置
        _state = RESET_MINING;

        // GPU 重置
        BTC_SHA256_GPU::Instance()->Reset();
    }

    void Stop() {
        _state = STOP_MINING;
        BTC_SHA256_GPU::Instance()->Stop();
    }

    void StartCPU() {

        return ;
        if (_isStarted)
            return;
        _isStarted = true;

        boost::asio::post(_asioWorker.IoContext(), [this]() {
            AppLog::Info("%s, 挖矿开始...", Info);

            Block::TBlockPtr blockPtr = nullptr;

            while(true) {

                switch(_state) {
                    case RESET_MINING: {
                        // 测试用，间隔30秒挖一下块
                        std::this_thread::sleep_for(std::chrono::seconds(30));
                        AppLog::Info("%s, 开始下一个出块...", Info);
                        _state = RUNING;

                        blockPtr = std::move(Block::CreateNewBlock(_minerAddress));

                        std::unique_lock<std::mutex> lck(_miningPrevHashMtx);{
                            _miningPrevHash = blockPtr->_hashPrevBlock;
                        }

                        break;
                    }

                    case STOP_MINING: {
                        AppLog::Info("%s, 停止挖矿...", Info);
                        return ;
                    }
                }

                // 执行挖矿
//                AppLog::Info("%s, 挖矿中...", Info);

                if (blockPtr == nullptr) {
                    AppLog::Error("%s, 挖矿Error block is nullptr", Info);
                    continue;
                }


                //
                DoMiner(blockPtr);
            }
        });
    }

    void StartGPU() {
        if (_isStarted)
            return;
        _isStarted = true;


        boost::asio::post(_asioWorker.IoContext(), [this]() {
            BTC_SHA256_GPU::Instance()->SyncStart([this](Block::TBlockPtr& block) {

                                                      block = std::move(Block::CreateNewBlock(_minerAddress));

                                                      AppLog::Debug("%s, 获取新块: %s", Info, block->GetHash().ToString().c_str());

                                                      std::unique_lock<std::mutex> lck(_miningPrevHashMtx);
                                                      {
                                                        _miningPrevHash = block->_hashPrevBlock;
                                                      }
                                                  },
                                                  [&](Block::TBlockPtr block){
                                                      AppLog::Info("%s, 挖矿成功: nonce: %d, %s", Info, block->_nNonce, block->ToString().c_str());
                                                      if (BlockChain::Instance()->AcceptBlock(block)) {
                                                          // 广播
                                                          NodeCache::Instance()->Each([&](Node *pNode) {
                                                              pNode->PushMessage(*block);
                                                          });

                                                      } else {
                                                          AppLog::Error("%s, StartGPU, AcceptBlock error: %s", Info, block->ToString().c_str());
                                                          // 丢弃本块，重新挖另一个块
                                                      }

                                                  });

        });
    }
};

class PeerLogicMan : public NetEventsInterface {

    NetMan      _netMan;
    AsioWorker  _asioWorker;

    InitialBlockDownloadManager _IBDManager;

    void ProcessMessageHandler(Node::TNodeID id, NetMessageData& bytes);
public:
    PeerLogicMan();
    ~PeerLogicMan();
    std::string Info() { return std::move("PeerLogicMan"); }


    virtual bool ProcessMessages(Node::TNodeID id);
    virtual bool SendMessages(Node::TNodeID id);
    virtual void InitializeNode(Node::TNodeID id);
    virtual void FinalizeNode(Node::TNodeID id);



    bool Initialize();



};


#endif //BITCOIN_GPU_MINER_PEER_LOGIC_MAN_H
