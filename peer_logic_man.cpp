//
// Created by fly on 2020/7/26.
//

#include "peer_logic_man.h"
//#include <boost/bind.hpp>
//#include <boost/function.hpp>
//#include "transaction.h"
#include "block.h"
#include "blockchain.h"

// TODO 需要实现向别的节点同步区块功能，否则本地生成的区块，但广播失败后导致此区块不能被同步

IMP_CLASS_SINGLETON(Miner);

PeerLogicMan::PeerLogicMan() :
_netMan(this)
{
    // 在IBD完成时，开始挖矿
    _IBDManager.SetIBDCompleteCallback([this]() {
//        AppLog::Debug("%s, IBD回调!", Info().c_str());

        // TODO 同步内存池, MemPool 消息必须bloom filters 开启 ??(需要理解)
//        NodeCache::Instance()->Each([](Node* node){
//            NetMessageMempool msg;
//            node->PushMessage(msg);
//        });

        Miner::Instance()->StartGPU();
    });
}

PeerLogicMan::~PeerLogicMan() {

    // 停止所有线程
   _netMan.Stop();
}



void PeerLogicMan::ProcessMessageHandler(Node::TNodeID id, NetMessageData& s) {
//    AppLog::Debug("%s, 线程id:{%d}, PeerLogicMan::ProcessMessageHandler",Info().c_str(), GetThreadId());

    auto pFromNode = NodeCache::Instance()->GetNode(id);
    if (pFromNode == nullptr) {
        return ;
    }

    NetMessageHeader hdr(s);
    AppLog::Debug("%s, 收到消息 <<<<-: %s", Info().c_str(), hdr._pchCommand);

    X_STR_SWITCH(hdr._pchCommand){

        X_CASE(NetMsgTypeInstance.VERSION) {
            s >> pFromNode->_version; // TODO 线程同步
            pFromNode->PushMessage(NetMessageVerack());

            // 握手完成
            pFromNode->SetHandshakeOK(true);
        }

        X_CASE(NetMsgTypeInstance.VERACK) {
            NetMessageVerack msg;
            s >> msg;


            // pFromNode->PushMessage(NetMessageGetAddr());
            // InitializeBlockDownload();


            // 开始IBD
            _IBDManager.Run(pFromNode);
        }

        X_CASE(NetMsgTypeInstance.ADDR) {
            NetMessageAddrs addrs;
            s >> addrs;
//            for (auto addr : addrs._addrs) {
//                _netMan.OpenNode(addr.ToString(), addr._port);
//            }
        }

        X_CASE(NetMsgTypeInstance.REJECT) {
            NetMessageReject msg;
            s >> msg;
            AppLog::Debug("%s, REJECT: ", Info().c_str(), msg._message.c_str());
        }


        X_CASE(NetMsgTypeInstance.ALERT) {

        }

        X_CASE(NetMsgTypeInstance.INV) {

            // IBD 未完成时不处理任何INV
            if (!_IBDManager.IsFinish()) {
                return ;
            }

            NetMessageInvs invs(NetMsgTypeInstance.INV);
            s >> invs;

            if (invs._invs.size() > MAX_INV_SIZE) {
                AppLog::Error("%s, Message inv size > %d", Info().c_str(), MAX_INV_SIZE);
                pFromNode->Disconnect(true);
                return;
            }

            for(auto &inv : invs._invs) {
                AppLog::Debug("%s, INV {type: %d, hashBlock: %s}", Info().c_str(), inv._type,
                              inv._hash.ToString().c_str());
                if (inv.IsGenTxMsg()) {
                    inv._type = GetDataMsg::MSG_WITNESS_TX; // 请求隔离见证消息
                } else
                if (inv.IsGenBlkMsg()) {
                    inv._type = GetDataMsg::MSG_WITNESS_BLOCK; // 请求区块
                } else {
                    AppLog::Error("%s, INV {type: %d, hashBlock: %s} 未知type!", Info().c_str(), inv._type,
                                  inv._hash.ToString().c_str());
                }
            }

            invs.SetCommand(NetMsgTypeInstance.GETDATA);
            pFromNode->PushMessage(invs);

        }

        X_CASE(NetMsgTypeInstance.PING) {
            NetMessagePingPong ping(true);
            s >> ping;

            ping.SwitchPing(false);

            pFromNode->PushMessage(ping);
        }

        X_CASE(NetMsgTypeInstance.TX) {

            // IBD 未完成时不处理任何INV
            if (!_IBDManager.IsFinish()) {
                return ;
            }

//            CTransactionRef ptx;
//            s >> ptx;
//
//            AppLog::Debug("%s, CTX{%s}", Info().c_str(), ptx->GetHash().GetHex().c_str());
//            AppLog::Debug("%s, CTX{%s}", Info().c_str(), ptx->ToString().c_str());
//            s.ResetBodyPos();


            // NetMessageTransaction::TNetMessageTransactionPtr tx = NetMessageTransaction::MakeTransaction();
            NetMessageTransaction tx;
            s >> tx;
//            AppLog::Debug("%s, TX{%s}", Info().c_str(), tx.GetHash().GetHex().c_str());
//            AppLog::Debug("%s, TX{%s}", Info().c_str(), tx.ToString().c_str());

            TXMemPool::Instance()->AcceptTx(tx);


            // TODO
            // 1. 是否在内存中已经存在
            // 2. 孤儿交易
            // 3. 在接受到块时如果孤儿交易的父交易被找到，则孤儿交易移到内存中
        }

        X_CASE(NetMsgTypeInstance.HEADERS) {
            if (!_IBDManager.Headers(pFromNode, s)) {
                return;
            }
            // 重置s的读取指针位置,使s可以重用
            s.ResetBodyPos();


        }

        X_CASE(NetMsgTypeInstance.BLOCK) {
            if (!_IBDManager.Blocks(pFromNode, s)) {
                return ;
            }
            // 重置s的读取指针位置,使s可以重用
            s.ResetBodyPos();

            Block::TCBlockPtr block;
            s >> block;

            BlockChain::Instance()->AcceptBlock(block);

        }

    } // End X_STR_SWITCH
}

bool PeerLogicMan::ProcessMessages(Node::TNodeID id) {

    auto pNode = NodeCache::Instance()->GetNode(id);
    if (pNode == nullptr) {
        return true;
    }

    // 将要处理的消息投递到 MessageProcess thread中
    const NetMessageData& msg = pNode->GetMessage();
    // boost::asio::post(_ioContextMessageProcess, [this, id, msg] () mutable {
    boost::asio::post(_asioWorker.IoContext(), [this, id, msg] () mutable {
        ProcessMessageHandler(id, REF(msg));
    });

    return false;
}

bool PeerLogicMan::SendMessages(Node::TNodeID id) {
    return false;
}

void PeerLogicMan::InitializeNode(Node::TNodeID id) {

}

void PeerLogicMan::FinalizeNode(Node::TNodeID id) {

}

bool PeerLogicMan::Initialize() {

    // 创建消息处理线程
//    _threadMessageProcessHandler = std::thread(&boost::asio::io_context::run, &_ioContextMessageProcess);

    BlockChain::Instance()->Initialize();

    _netMan.Initialize();



    return false;
}
