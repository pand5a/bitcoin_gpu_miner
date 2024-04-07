//
// Created by fly on 2020/9/23.
//

#include "blockchain.h"
#include "peer_logic_man.h"

IMP_CLASS_SINGLETON(DBBlockWrapper);
IMP_CLASS_SINGLETON(DBBlockIndexWrapper);
IMP_CLASS_SINGLETON(DBUTXOWrapper);
IMP_CLASS_SINGLETON(UTXOManager);
IMP_CLASS_SINGLETON(BlockTipManager);
IMP_CLASS_SINGLETON(TXMemPool);

IMP_CLASS_SINGLETON(DBTransactionIndexWrapper);
IMP_CLASS_SINGLETON(TransactionIndexManager);


IMP_CLASS_SINGLETON(ScriptValidation);

IMP_CLASS_SINGLETON(BlockChain);


/**
 * TXMemPool
 */

bool TXMemPool::AcceptTx(const NetMessageTransaction& tx, bool isLock) {
    std::unique_lock<std::mutex> lck(_poolMutex, std::defer_lock);
    if (isLock) {
        lck.lock();
    }

    const auto& hash = tx.GetHash();

    bool isExistPool = _pool.count(hash) > 0;
    bool isExistOrphanPool = _orphanPool.count(hash) > 0;
    bool isExistMiningPool = _miningPool.count(hash) > 0;

    // 是否已经存在于内存池或孤儿池中或挖矿池中
    if (isExistPool || isExistOrphanPool || isExistMiningPool > 0) {
        AppLog::Error("%s, AcceptTx, 交易已在内存池中: {%s}", Info, hash.ToString().c_str());
        return false;
    }

    // 判断是否为孤儿交易
    for(size_t i =0; i < tx._vin.size(); ++i) {
        const auto& in =  tx._vin[i];
        if (!DBTransactionIndexWrapper::Instance()->IsExist(in._previousOutput._hashTransaction)) {
            // 放入孤儿交易池
            _miningPool[hash] = tx;
            AppLog::Debug("%s, AcceptTx, 孤儿交易: 未找到的in: {%s},  TX: {%s}", Info, in.ToString().c_str(), tx.ToString().c_str());
            return true;
        }
    }

    auto height = BlockTipManager::Instance()->GetActiveTip()->_height + 1;
    auto tmpBlockIndex = BlockIndex(height, uint256(), uint256());
    uint32_t scriptFlags = ScriptValidation::GetBlockScriptFlags(tmpBlockIndex);
    BlockChain::CheckTransactionResult r;

    if (!BlockChain::Instance()->CheckTransaction(tx, scriptFlags, height, r)) {
       return false;
    }

    _pool[hash] = tx;

    AppLog::Info("%s, AcceptTx, 成功添加交易到内存池: %s", Info, tx.ToString().c_str());

    return true;
}

bool TXMemPool::ProcOrphanTx() {
    std::unique_lock<std::mutex> lck(_poolMutex);

    auto FuncProcOrphanTx = [&](const NetMessageTransaction& tx) ->bool {

        // 先Check此孤儿交易是否已经入链
        if (DBTransactionIndexWrapper::Instance()->IsExist(tx.GetHash())) {
            return true;
        }

        bool isAllInFound = true;
        for(size_t i =0; i < tx._vin.size(); ++i) {
            const auto& in = tx._vin[i];
            if (!DBTransactionIndexWrapper::Instance()->IsExist(in._previousOutput._hashTransaction)) {
                isAllInFound = false;
                break;
            }
        }

        // tx的全部输入均能在主链上找到，则将其放入交易内存池
        if (isAllInFound) {
            AcceptTx(tx, false);
        }
        return isAllInFound;
    };


    for (auto it = _miningPool.begin(); it != _miningPool.end();) {
        if (FuncProcOrphanTx(it->second)) {
            it = _miningPool.erase(it);
        } else {
            ++it;
        }
    }

    return true;
}


/**
 * BlockChain
 */
// MainTest
// static auto powLimit = uint256S("00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
// TestNet
// static auto powLimit = uint256S("00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
// RegTest
//uint256 BlockChain::s_PowLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
//BlockChain* BlockChain::Instance() {
//    static BlockChain instance;
//    return &instance;
//}


bool BlockChain::AcceptBlock(const Block::TCBlockPtr& block, bool isLocalMining) {

    /**
     * TODO 因挖矿线程及消息处理线程都会实用本函数，而此处用到了链数据，所以暂时在这里加个锁
     */
    static std::mutex mtx;
    std::unique_lock<std::mutex> lck(mtx);

    // TODO 注意重复的非法区块的到来。防止(DDos)
    BlockIndex  prevBlockIndex;
    const auto& hash        = block->GetHash();
    auto tip                = _tipManager.GetTip(block->_hashPrevBlock); // 当前块所属的某个链
    auto bFoundTip          = tip != nullptr;
    auto activeTip          = _tipManager.GetActiveTip(); // 当前激活链
    auto bPrevBlockFound    = _blockIndexWrapper.Get(block->_hashPrevBlock, prevBlockIndex); // 父区块index
    auto bOrphanBlock       = false; // 新块是否为孤块
    auto bIsGenesis         = hash == ParamsBase::Instance()->hashGenesisBlock;

    // 获取当前块的高度
    auto FuncGetCurrentHeight = [&]() -> int64_t {
        if (bIsGenesis) {
            return 0;
        }

        if (!bPrevBlockFound) {
            return BlockIndex::INVALID_BLOCK_HEIGHT;
        }

        return prevBlockIndex._height + 1;
    };


    // 新块接收成功后的处理
    auto FuncAcceptBlockOK = [&](uint256 prevHash) {

        // 处理当前收到的块，与当前正在挖块的冲突
        if (isLocalMining && prevHash == Miner::Instance()->GetMiningPrevHash()) {
            // 1. 撤销内存池中已使用的交易
            // 2. 重新开始挖矿
            TXMemPool::Instance()->UndoMiningTxs();
            Miner::Instance()->ResetMining();
        }

        // 处理孤儿交易
        // 在收到新的块时，重新校验孤儿交易。
        TXMemPool::Instance()->ProcOrphanTx();
    };

    // 判断新块是否已经接收并处理过了
    auto FuncIsAlertExist = [&]() -> bool  {
        return DBBlockIndexWrapper::Instance()-> IsExist(hash);
    };

    /**
     * 注: tip存在的同时prevBlock必存在，反之未必.
     */

    AppLog::Debug("%s, BlockChain, AcceptBlock, NewBlockHash: %s, tip: %s, activeTip: %s, prevBlock: %s",
                  Info(), hash.ToString().c_str(),
                  tip == nullptr ? "NULL" : tip->_hash.ToString().c_str(),
                  activeTip == nullptr ? "NULL" : activeTip->_hash.ToString().c_str(),
                  !bPrevBlockFound ? "NULL" : prevBlockIndex._hash.ToString().c_str());

    if (FuncIsAlertExist()) {
        AppLog::Debug("%s, BlockChain, AcceptBlock, NewBlockHash: %s, is exist in localhost", Info(), hash.ToString().c_str());
        return false;
    }

    // 更新父块的next=本块
    if (bPrevBlockFound) {
        prevBlockIndex._hashNextBlock = hash;
    }

    // 最长链
    if (bFoundTip && tip == activeTip) {

        if (!CheckBlock(block)) {
            return false;
        }

        auto height = FuncGetCurrentHeight();

        auto tmpBlockIndex = BlockIndex(height, hash, block->_hashPrevBlock);
        uint32_t scriptFlags = ScriptValidation::GetBlockScriptFlags(tmpBlockIndex);

        // 校验块内交易(包含UTXO的校验)
        if (!CheckTransactions(block->_vtx, scriptFlags, height)) {
            return false;
        }

        auto index = SaveBlock(*block, prevBlockIndex, true);
        if (index == nullptr) {
            return false;
        }

        if (!ProcessTransactions(block->_vtx, hash, index->_height)) {
            return false;
        };

        AppLog::Info("%s, 接收新块成功，高度: %d, %s", Info(), height, block->ToString().c_str());


        FuncAcceptBlockOK(prevBlockIndex._hashPrevBlock);

        // TODO 此处不Return用于孤块处理: 本块接受成功后，以本块为父块到孤块列表中查找子块
    } else

        // 分叉链+新块后的高度超过了当前的Active链，即此分叉链变为了Active链，则回溯此链，并按块增长顺序逐个块Check
    if (bFoundTip && (tip->_height + 1) > activeTip->_height) {

        auto height = FuncGetCurrentHeight();

        auto tmpBlockIndex = BlockIndex(height, hash, block->_hashPrevBlock);
        uint32_t scriptFlags = ScriptValidation::GetBlockScriptFlags(tmpBlockIndex);

        // 回溯的停止点，即是与当前Active链交叉的那个点
        std::list<BlockIndex::TCBlockIndexPtr> forkChain;

        // 无效的候选链：如果当前候选链成功变为最长链，则将当前激活链从交叉点开始到最顶端保存在此stack中
        // 用于清除DBTXIndex及UTXO
        std::list<BlockIndex::TCBlockIndexPtr> inValidChain;

        // 分叉链与Active链的高度差
        auto chainHeightDiff = (tip->_height + 1) - activeTip->_height;

        // 先保存比Active链高的块
        BlockIndex::TBlockIndexPtr it = std::make_shared<BlockIndex>(*block, tip->_height + 1);
        while(--chainHeightDiff > 0 && it != nullptr && !it->_hashPrevBlock.IsNull()) {
            forkChain.push_back(it);
            it = _blockIndexWrapper.Get(it->_hashPrevBlock);
        }

        int64_t forkBlockHeight = -1; // 交叉点块高度
        // 回溯到与当前Active链交叉的那个点
        BlockIndex::TCBlockIndexPtr activeIt = activeTip;
        while(it != nullptr &&
              !activeIt->_hashPrevBlock.IsNull() && //
              !it->_hashPrevBlock.IsNull() &&
              activeIt != it) { // 当前Active链与分叉链的交点，遇到则停止。

            forkChain.push_back(it);
            inValidChain.push_back(activeTip);

            it       = _blockIndexWrapper.Get(it->_hashPrevBlock);
            activeIt = _blockIndexWrapper.CGet(activeIt->_hashPrevBlock);

            // 分叉点块的高度
            forkBlockHeight = it->_height;
        }


        // 从分叉点后第一个块开始做Check并更新交易池，及UTXO集合
        std::list<NetMessageTransaction> okTxList;
        BlockIndex::TBlockIndexPtr newBlockIndex = nullptr;

        // 用于临时存放，分叉链Check完成的交易，供下个需要Check候选链块校验使用
        TMemTXIndexPtrMap tmpMemTxIndexMap; // 只保存校验好的
        std::set<OutPoint> tmpMemUTXOSet; // 只保存校验好的

        // while(!forkChain.empty()) {
        for(auto it = forkChain.rbegin(); it != forkChain.rend(); ++it) {
            // auto& index = forkChain.top();
            auto& index = *it;

            // 先做Block的check
            if (!CheckBlock(block)) {
                return false;
            }

            const std::vector<NetMessageTransaction>* vtx = nullptr;
            const Block* tmpBlock = nullptr;

            // 如果回溯到当前的新块，则先保存此块
            if (index->_hash == hash) {
                newBlockIndex = SaveBlock(*block, prevBlockIndex, false);
                if (newBlockIndex == nullptr) {
                    return false;
                }

                // 获取当前块中的tx
                vtx = &(block->_vtx);
                tmpBlock = block.get();
            } else {
                // 获取当前块中的tx
                tmpBlock = &const_cast<BlockIndex*>(index.get())->GetBlock();
                vtx = &(tmpBlock->_vtx);
            }

            // 校验块内交易(包含UTXO的校验)
            if (!CheckTransactions(tmpBlock->_vtx,
                                   scriptFlags,
                                   newBlockIndex->_height,
                                   &tmpMemTxIndexMap,
                                   &tmpMemUTXOSet,
                                   forkBlockHeight)) {
                return false;
            }

            // 保存校验好的交易,及UTXO到临时内存中
            for(const auto& tx : tmpBlock->_vtx) {

                // DBTXIndex
                auto idx = std::make_shared<DBTransactionIndexWrapper::TxIdx>(tx, index->_hash, index->_height);
                tmpMemTxIndexMap[idx->hashBlock] = idx;

                // UTXO
                for (const auto& in : tx._vin) {
                    // 先删除已经花费的
                    tmpMemUTXOSet.erase(in._previousOutput);
                }
                for (auto i = 0; i < tx._vout.size(); ++i) {
                    auto out = tx._vout[i];
                    tmpMemUTXOSet.insert(OutPoint(tx.GetHash(), i, index->_hash, index->_height, false));
                }
            }
        }

        // 清除废弃链的DBTXIndex及UTXO
        for(auto it = inValidChain.rbegin(); it != inValidChain.rend(); ++it) {
            const auto block = const_cast<BlockIndex*>(it->get())->GetBlock();
            // 清除TX
            for (const auto& tx: block._vtx) {

                DBTransactionIndexWrapper::Instance()->DeleteData(tx.GetHash());

                _utxoManager.UnCacheUTXO(tx);
            }
        }

        // 在上述Check完成的情况下, 更新交易池，及UTXO集合
        for(auto it = forkChain.rbegin(); it != forkChain.rend(); ++it) {
            const auto tmpBlock = const_cast<BlockIndex*>(it->get())->GetBlock();

            ProcessTransactions(tmpBlock._vtx, (*it)->_hash, (*it)->_height);
        }

        if (!_tipManager.UpdateTip(newBlockIndex)) {
            AppLog::Error("%s, 更新NewBlockIndex或创建NewBlockBlockTip错误.", Info());
            return false;
        }

        FuncAcceptBlockOK(newBlockIndex->_hashPrevBlock);
        // TODO 此处不Return用于孤块处理: 本块接受成功后，以本块为父块到孤块列表中查找子块
    } else

    if ((bFoundTip && tip->_height + 1 <= activeTip->_height) || // 分叉链+新块后的高度未超过当前的Active链，则只将新块挂入此链，并更新此链tip
        (!bFoundTip && bPrevBlockFound)) // 新块的prev != 某个Tip && prev被找到，则是分叉块，则以此块做为分叉块挂入，并以此块创建一个Tip(新的分叉链)
    {
        // 先做Block的check
        if (!CheckBlock(block)) {
            return false;
        }

        return SaveBlock(*block, prevBlockIndex, true) != nullptr;
    } else



        //
        //
        // 新块的prev != 某个Tip && prev未找到，则是孤块，放入孤块池内
    if (!bFoundTip && !bPrevBlockFound) {
        // Check 是否重复
        for (auto it = _orphanBlocks.find(hash); it != _orphanBlocks.end(); ++it) {
            if (it->second->GetHash() == hash) {
                return false;
            }
        }
        _orphanBlocks.insert(std::make_pair(block->_hashPrevBlock, block));
        bOrphanBlock = true;
    }


    //
    //
    // 如果新块不是孤块，则查找此块是否有子块存在于孤块池中，有,则递归层处理(有可还存在孙子块，曾孙子块).
    // 如果有多个子块，则是分叉.
    if (!bOrphanBlock) {
        for (auto it = _orphanBlocks.find(hash); it != _orphanBlocks.end(); ++it) {
            return AcceptBlock(it->second);
        }
    }

    return true;
}