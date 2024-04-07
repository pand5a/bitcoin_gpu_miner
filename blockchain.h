//
// Created by fly on 2020/9/23.
//
/**
 * 区块链
 */

#ifndef BITCOIN_GPU_MINER_BLOCKCHAIN_H
#define BITCOIN_GPU_MINER_BLOCKCHAIN_H
#include <stack>
#include <map>
#include <mutex>
#include <leveldb/db.h>
#include "block.h"
#include "net.h"
#include "arith_uint256.h"
#include "script/interpreter.h"
#include "params.h"
#include "script/sigcache.h"


class DBWrapper {
protected:
    std::shared_ptr<leveldb::DB> _db;
public:

    DBWrapper() : _db(nullptr) {
//        AppLog::Debug("%s", "DB");
    };

    ~DBWrapper() {
//        AppLog::Debug("%s", "~DB");
    };

    bool GetInt64(const leveldb::Slice&& key, int64_t& value ) {
        std::string buf;
        auto s = _db->Get(leveldb::ReadOptions(), key, &buf);
        if (!s.ok()) {
            return false;
        }

        assert(buf.size() == sizeof(int64_t));
        if (buf.size() != sizeof(int64_t)) {
            return false;
        }
        memcpy((char*)&value, &buf[0], buf.size());
        return true;
    };

    bool SetInt64(const leveldb::Slice&& key, const int64_t& value) {
        auto s = _db->Put(leveldb::WriteOptions(), key, leveldb::Slice((char*)&value, sizeof(int64_t)));
        if (!s.ok()) {
            return false;
        }
        return true;
    };

    bool PutData(const uint256& key, const TBuffer& data) {
        return PutData(std::move(leveldb::Slice((char*)key.begin(), key.size())), data);
    }


    bool PutData(const leveldb::Slice&& key, const TBuffer& data) {
        auto s = _db->Put(leveldb::WriteOptions(), key, leveldb::Slice((char*)&data[0], data.size()));
        if (!s.ok()) {
            return false;
        }
        return true;
    }

    bool GetData(const uint256& key, TBuffer& data) {
        return GetData(std::move(leveldb::Slice((char*)key.begin(), key.size())),data);
    };

    bool GetData(const leveldb::Slice&& key, TBuffer& data) {
        std::string buf;
        auto s = _db->Get(leveldb::ReadOptions(), key, &buf);
        if (!s.ok()) {
            return false;
        }

        data.resize(buf.size());
        memcpy((char*)&data[0], &buf[0], buf.size());
        return true;
    };

    bool DeleteData(const leveldb::Slice&& key) {
        auto s = _db->Delete(leveldb::WriteOptions(), key);
        if (!s.ok()) {
            return false;
        }
        return true;
    }

    bool DeleteData(const uint256& key) {
        auto s = _db->Delete(leveldb::WriteOptions(), std::move(leveldb::Slice((char*)key.begin(), key.size())));
        if (!s.ok()) {
            return false;
        }
        return true;
    }



    bool PutHash256(const leveldb::Slice&& key, const uint256& value) {
        auto s = _db->Put(leveldb::WriteOptions(), key, leveldb::Slice((char*)value.begin(), value.size()));
        if (!s.ok()) {
            return false;
        }
        return true;
    }

    bool GetHash256(const leveldb::Slice&& key, uint256& value) {
        std::string buf;
        auto s = _db->Get(leveldb::ReadOptions(), key, &buf);
        if (!s.ok()) {
            return false;
        }

        assert(buf.size() == value.size());
        if (buf.size() != value.size()) {
            return false;
        }
        memcpy((char*)value.begin(), &buf[0], buf.size());
        return true;
    }
};

class DBBlockWrapper : public DBWrapper {
    const std::string _TIP_BLOCK_INFO_KEY = "_TIP_BLOCK_INFO_KEY";
public:


    struct TipBlockInfo {
        uint256     _hash;
        uint64_t    _height;
        uint32_t    _txCount;
        uint256     _hashPrevBlock;

        TipBlockInfo():_height(0) {

        }
        ADD_SERIALIZE_METHODS;

        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action) {
            READWRITE(_hash);
            READWRITE(_height);
            READWRITE(_txCount);
            READWRITE(_hashPrevBlock);
        };
    };

    DBBlockWrapper() {
        leveldb::Options options;
        options.create_if_missing = true;

        leveldb::DB *db = nullptr;
        leveldb::Status status = leveldb::DB::Open(options,
                                                   ParamsBase::Instance()->dbPaths.blocks,
                                                   &db);
        assert(status.ok());
        _db.reset(db);
    }

    static DBBlockWrapper* Instance();


    bool GetBlock(const uint256& hash, Block& value) {
        TBuffer buf;
        auto isOK = GetData(std::move(leveldb::Slice((char*)hash.begin(), hash.size())), buf);
        if (!isOK) {
            return false;
        }

        NetMessageData s(buf);
        s >> value;
        return true;
    }


    bool PutBlock(const uint256& hash, const Block& value) {
        NetMessageData s;
        s << value;

        return PutData(
                std::move(leveldb::Slice((char*)hash.begin(), hash.size())), s.Ref());
    }


    bool GetTipBlockInfo(TipBlockInfo& value) {
        TBuffer buf;
        auto isOK = GetData(_TIP_BLOCK_INFO_KEY, buf);
        if (!isOK) {
            return false;
        }

        NetMessageData s(buf);
        s >> value;
        return true;
    }


    bool PutTipBlockInfo(const TipBlockInfo& value) {
        NetMessageData s;
        s << value;

        return PutData(_TIP_BLOCK_INFO_KEY, s.Ref());
    }
};

class DBBlockIndexWrapper : public DBWrapper {
public:
    const char* Info = "DBBlockIndexWrapper";
    DBBlockIndexWrapper() {
        leveldb::Options options;
        options.create_if_missing = true;

        leveldb::DB *db = nullptr;
        leveldb::Status status = leveldb::DB::Open(options,
                                                   ParamsBase::Instance()->dbPaths.blockIndex,
                                                   &db);
        assert(status.ok());
        _db.reset(db);
    }

    static DBBlockIndexWrapper* Instance();


    bool IsExist(const uint256& hash) {
        TBuffer buf;
        return GetData(std::move(leveldb::Slice((char*)hash.begin(), hash.size())), buf);
    }

    bool Get(const uint256& hash, BlockIndex& value) {
//        AppLog::Info("%s, GET: %s", Info, hash.ToString().c_str());
        TBuffer buf;
        auto isOK = GetData(std::move(leveldb::Slice((char*)hash.begin(), hash.size())), buf);
        if (!isOK) {
            return false;
        }

        NetMessageData s(buf);
        s >> value;
        return true;
    }

    BlockIndex::TCBlockIndexPtr CGet(const uint256& hash) {
//        AppLog::Info("%s, GET: %s", Info, hash.ToString().c_str());
        TBuffer buf;
        auto isOK = GetData(std::move(leveldb::Slice((char*)hash.begin(), hash.size())), buf);
        if (!isOK) {
            return nullptr;
        }

        BlockIndex::TCBlockIndexPtr r = BlockIndex::Make();
        NetMessageData s(buf);
        s >> r;
        return std::move(r);
    }

    BlockIndex::TBlockIndexPtr Get(const uint256& hash) {
//        AppLog::Info("%s, GET: %s", Info, hash.ToString().c_str());
        TBuffer buf;
        auto isOK = GetData(std::move(leveldb::Slice((char*)hash.begin(), hash.size())), buf);
        if (!isOK) {
            return nullptr;
        }

        BlockIndex::TBlockIndexPtr r = std::make_shared<BlockIndex>();
        NetMessageData s(buf);
        s >> r;
        return std::move(r);
    }

    bool Put(const BlockIndex& index) {
//        AppLog::Info("%s, PUT: %s", Info, index._hash.ToString().c_str());
        NetMessageData s;
        s << index;
        return PutData(
                std::move(leveldb::Slice((char*)index._hash.begin(), index._hash.size())), s.Ref());
    }

    bool Put(const BlockIndex::TCBlockIndexPtr& index) {
        return Put(*index);
    }
};

/**
 * DBTransactionIndexWrapper中管理的全部为主链上的交易。
 */
class DBTransactionIndexWrapper : public DBWrapper {
public:
    DBTransactionIndexWrapper(){
        leveldb::Options options;
        options.create_if_missing = true;

        leveldb::DB *db = nullptr;
        leveldb::Status status = leveldb::DB::Open(options,
                                                   ParamsBase::Instance()->dbPaths.txIndex,
                                                   &db);
        assert(status.ok());
        _db.reset(db);
    };
    DEF_CLASS_SINGLETON(DBTransactionIndexWrapper);

    struct TxIdx{
        uint256 hashBlock; // block hashBlock
        int64_t height; // block height
        NetMessageTransaction tx;
        ADD_SERIALIZE_METHODS;

        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action) {
            READWRITE(hashBlock);
            READWRITE(height);
            READWRITE(tx);
        }
        using TTxIdxPtr = std::shared_ptr<TxIdx>;

        TxIdx(){};
        TxIdx(const NetMessageTransaction& tx, const uint256& hashBlock, int64_t height):
                tx(tx), hashBlock(hashBlock), height(height) {}

        bool IsNull() {
            return tx.IsNull();
        }
    };

    bool Put(const TxIdx& tx) {
       NetMessageData s;
       s << tx;
       return PutData(tx.tx.GetHash(), s.Ref());
    }

    bool Get(const uint256& txHash, TxIdx& v) {
        NetMessageData s;
        if (!GetData(txHash, s.Ref())) {
            return false;
        }
        s >> v;
        return true;
    }

    bool IsExist(const uint256& txHash) {
        NetMessageData s;
        return GetData(txHash, s.Ref());
    }
};
using TMemTXIndexPtrMap =  std::map<uint256, DBTransactionIndexWrapper::TxIdx::TTxIdxPtr>;


/**
 * TODO 待用
 * Tx 的Index管理，包含了UTXO的管理
 */
class TransactionIndexManager : DBWrapper{
   bool SetUTXOSpentStatus(const OutPoint& pt, bool spent) {
       if (pt._hashTransaction.IsNull()) {
           return false;
       }

       IndexEntity entity;
       if (!Get(pt._hashTransaction, entity)) {
           return false;
       }

       if (pt._outPutIdx < 0 || pt._outPutIdx >= entity.tx._vout.size()) {
           return false;
       }

       entity.tx._vout[pt._outPutIdx]._isSpent = spent;
       return Put(entity);
   }

public:
    DEF_CLASS_SINGLETON(TransactionIndexManager);
    struct IndexEntity {
        uint256         hashBlock; // block hash
        int64_t         height; // block height
        NetMessageTransaction tx;

        ADD_SERIALIZE_METHODS;
        template <typename Stream, typename Operation>
        inline void SerializationOp(Stream& s, Operation ser_action) {
            READWRITE(hashBlock);
            READWRITE(height);
            READWRITE(tx);
        }
    };

    bool Put(const IndexEntity entity) {
        NetMessageData s(SERIALIZE_TXOUT_ISSPENT); s << entity;
        return PutData(entity.tx.GetHash(), s.Ref());
    }

    bool Put(const uint256& hashBlock, int64_t height, const NetMessageTransaction& tx) {
        IndexEntity entity = {hashBlock, height, tx};
        return Put(entity);
    }

    bool Get(const uint256& hash, IndexEntity& outEntity) {
        NetMessageData s(SERIALIZE_TXOUT_ISSPENT);
        if (!GetData(hash, s.Ref())) {
            return false;
        }
        s >> outEntity;
        return true;
    }

    bool Remove(const uint256& hash) {
        return DeleteData(hash);
    }

    bool IsSpentUTXO(const OutPoint& pt) {
        if (pt._hashTransaction.IsNull()) {
            return false;
        }

        IndexEntity entity;
        if (!Get(pt._hashTransaction, entity)) {
            return false;
        }

        if (pt._outPutIdx < 0 || pt._outPutIdx >= entity.tx._vout.size()) {
            return false;
        }

        return entity.tx._vout[pt._outPutIdx].IsSpent();
    }

    bool UseUTXO(const OutPoint& pt) {
        return SetUTXOSpentStatus(pt, true);
    }

    bool RecoveryUTXO(const OutPoint& pt) {
        return SetUTXOSpentStatus(pt, false);
    }
};

class DBUTXOWrapper : public DBWrapper {
public:
    static DBUTXOWrapper* Instance();

    DBUTXOWrapper() {
        leveldb::Options options;
        options.create_if_missing = true;

        leveldb::DB *db = nullptr;
        leveldb::Status status = leveldb::DB::Open(options,
                                                   ParamsBase::Instance()->dbPaths.utxo,
                                                   &db);
        assert(status.ok());
        _db.reset(db);
    }
};
// 未消费的UTXO管理
class UTXOManager {
    DBUTXOWrapper _dbWrapper;

public:
    static UTXOManager* Instance();

    bool CacheUTXO(const NetMessageTransaction& tx,
            const uint256& hashBlock, int64_t height) {

        const auto& hash = tx.GetHash();

        // 如果是非CoinBase需要先移除已花费的utxo
        if (!tx.IsCoinBase()) {
            // 先移除tx.input.prevout(即已花费的utxo)
            for(const auto& in : tx._vin) {
                Use(in._previousOutput);
            }
        }

        // Cache 输出
        for(int i = 0; i < tx._vout.size(); ++i) {
            Put(OutPoint(hash, i, hashBlock, height, false));
        }
        return true;
    }

    bool UnCacheUTXO(const NetMessageTransaction& tx) {

        const auto& hash = tx.GetHash();

        // 如果是非CoinBase需要先移除已花费的utxo
        if (!tx.IsCoinBase()) {
            // 先撤销tx.input.prevout
            for(const auto& in : tx._vin) {
                UnUsed(in._previousOutput);
            }
        }

        // Delete 输出
        for(int i = 0; i < tx._vout.size(); ++i) {
            Remove(OutPoint(hash, i ));
        }
        return true;
    }

    bool Put(const OutPoint& pt) {
        if (pt.IsNull()) {
            return false;
        }
        NetMessageData k; k << pt;
        NetMessageData v(SERIALIZE_BLOCK_HASH); v << pt;
        return _dbWrapper.PutData(std::move(leveldb::Slice((char*)k.Data(), k.DataSize())), v.Ref());
    }

    bool IsUnspent(const OutPoint& pt) {
        OutPoint v;
        if (!Get(pt, v)) {
            return false;
        }

        return !v._isUsed;
    }

    bool Get(const OutPoint& pt, OutPoint& out) {
        NetMessageData s; s << pt;
        TBuffer buf;
        if (!_dbWrapper.GetData(std::move(leveldb::Slice((char*)s.Data(), s.DataSize())), buf)) {
            return false;
        }

        NetMessageData v(buf, SERIALIZE_BLOCK_HASH);
        v >> out;
        return true;
    }

    bool CanUse(const OutPoint& pt, OutPoint* out) {

        OutPoint v;
        if (!Get(pt, v)) {
            return false;
        }

        if (out != nullptr) {
            *out = v;
        }
        return !v._isUsed;
    }

    bool Use(const OutPoint& pt) {
        OutPoint v;
        if (!Get(pt, v)) {
            return false;
        }

        v._isUsed = true;

        return Put(v);
    }

    bool UnUsed(const OutPoint& pt) {
        OutPoint v;
        if (!Get(pt, v)) {
            return false;
        }

        v._isUsed = false;

        return Put(v);
    }

    bool Remove(const OutPoint& pt) {
        NetMessageData s; s << pt;
        return _dbWrapper.DeleteData(std::move(leveldb::Slice((char*)s.Data(), s.DataSize())));
    }
};

class BlockTipManager {
protected:
    const std::string TIP_ACTIVE_KEY = "TIP_ACTIVE_KEY"; // 当前激活链的TIP key
    const std::string TIP_ALL_KEY_KEY = "TIP_ALL_KEY_KEY"; // 所有Tip的Key

    BlockTipManager(){};

    std::mutex      _mtx;

    // 在写入时更新
    BlockIndex::TCBlockIndexPtrMap _tipMap;

    /**
     * 更新所有Tip
     * @param map
     * @return
     */
    bool UpdateBlockTipMap() {
        NetMessageData s; s << _tipMap;
        return DBBlockIndexWrapper::Instance()->PutData(TIP_ALL_KEY_KEY, s.Ref());
    }

    /**
   * 获取所有Tip
   * @return Tip map
   */
    BlockIndex::TCBlockIndexPtrMap& GetBlockTipMap() {

        if (!_tipMap.empty()) {
            return _tipMap;
        }

        TBuffer buf;
        if (!DBBlockIndexWrapper::Instance()->GetData(TIP_ALL_KEY_KEY, buf)) {
            return _tipMap;
        }

        _tipMap.clear();

        NetMessageData data(buf);
        data >> _tipMap;

        return _tipMap;
    }

public:
    const char* Info = "BlockTipManager";
    static BlockTipManager* Instance();

    /**
     *
     * @param tip
     * @return
     */
    bool UpdateTip(const BlockIndex::TCBlockIndexPtr& tip) {
        std::unique_lock<std::mutex> lck(_mtx);

        auto map = GetBlockTipMap();
        auto it  = map.find(tip->_hashPrevBlock);

        if (it != map.end()) {
            // 删除本链上旧的tip
            map.erase(it);
        } else {
            AppLog::Warn("%s, UpdateTip newTip.PrevBlock not found, {hashBlock: %s, prevHash: %s}",
                         Info, tip->_hash.ToString().c_str(),
                         tip->_hashPrevBlock.ToString().c_str());
        }

        return AddTip(tip, false); // 使用lck给AddTip加锁
    }

    /**
     * 获取hash指定的Tip
     * @param 区块hash
     * @return tip index
     */
    BlockIndex::TCBlockIndexPtr GetTip(const uint256& hash) {
        std::unique_lock<std::mutex> lck(_mtx);
        auto map = GetBlockTipMap();
        auto it = map.find(hash);
        if (it != map.end()) {
            return it->second;
        }
        return nullptr;
    }

    /**
     * 获取当前激活链的tip
     * @return tip index
     */
    BlockIndex::TCBlockIndexPtr GetActiveTip() {
        std::unique_lock<std::mutex> lck(_mtx);
        BlockIndex::TCBlockIndexPtr active = nullptr;
        for (const auto& v : GetBlockTipMap()) {
            if (!active) {
                active = v.second;
            } else if (v.second->_height > active->_height ){
                active = v.second;
            }
        }

        return active;
    }

    /**
     * 添加一个Tip
     * @param index
     * @return ok
     */
    bool AddTip(BlockIndex::TCBlockIndexPtr index, bool isLock = true) {
        std::unique_lock<std::mutex> lck(_mtx, std::defer_lock);
        if (isLock) {
            lck.lock();
        }

        GetBlockTipMap()[index->_hash] = index;
        return UpdateBlockTipMap();
    }

};

class TXMemPool {
    std::map<uint256, NetMessageTransaction> _pool;
    std::mutex                               _poolMutex;

    std::map<uint256, NetMessageTransaction> _orphanPool;

    std::map<uint256, NetMessageTransaction> _miningPool; // 正在挖矿的tx


    TXMemPool(){};

public:
    const char* Info = "TXMemPool";

    DEF_CLASS_SINGLETON(TXMemPool);
    std::mutex& Mutex() {return _poolMutex;};

    void Remove(const uint256& hash, bool lock = true) {
        if (lock) GPU_MINER_LOCK(_poolMutex);
        _pool.erase(hash);
        _miningPool.erase(hash);
    }

    void Push(const NetMessageTransaction& tx, bool lock = true) {
        if (lock) GPU_MINER_LOCK(_poolMutex);
        _pool[tx.GetHash()] = tx;
    }

    // 判断交易是否在挖矿
    bool IsMining(const NetMessageTransaction& tx) {
        return _miningPool.find(tx.GetHash()) != _miningPool.end();
    }

    // 筛选进新块的txs
    bool GetNewBlockTxs(uint64_t blockHeight, int64_t blockTime, CAmount& nFees, std::vector<NetMessageTransaction>& vtx) {
        std::unique_lock<std::mutex> lck(_poolMutex);

        nFees = 0;
        auto nBlockMaxWeight = MAX_BLOCK_WEIGHT - 4000;
        auto nBlockWeight = 80; // 80 is header sieze

        _miningPool.clear();
        for (auto it = _pool.begin(); it != _pool.end();) {
            const auto& tx = it->second;

            auto fee = tx.GetFees();
            if (fee == 0) {
                ++it;
                continue;
            }

            nBlockWeight += ::GetSerializeSize(tx, SERIALIZE_TRANSACTION_NO_WITNESS) * WITNESS_SCALE_FACTOR;
            if (nBlockWeight > nBlockMaxWeight) {
                return true;
            }

            // TODO 判断签名数量


            nFees += fee;
            vtx.push_back(tx);

            _miningPool[it->first] = it->second;
            it = _pool.erase(it);
        }

        return true;
    }

    void UndoMiningTxs() {
        std::unique_lock<std::mutex> lck(_poolMutex);

        for (const auto& it : _miningPool) {
            _pool[it.first] = it.second;
        }
        _miningPool.clear();
    }

    bool AcceptTx(NetMessageTransaction::TNetMessageTransactionPtr tx) {
        return AcceptTx(*tx);
    }

    bool AcceptTx(const NetMessageTransaction& tx, bool isLock = true);

    // 处理平遥交易
    bool ProcOrphanTx();
};

class ScriptValidation {
public:
    ScriptValidation() {};
    DEF_CLASS_SINGLETON(ScriptValidation);

    static bool IsScriptWitnessEnabled() {
        auto& params(*ParamsBase::Instance());
        return params.SegwitHeight != std::numeric_limits<int>::max();
    }

    static bool IsWitnessEnabled(const BlockIndex& index) {
        auto& params(*ParamsBase::Instance());
        int height = index._hashPrevBlock.IsNull() ? 0 : index._height;
        return (height >= params.SegwitHeight);
    }

    static bool IsWitnessEnabled(int64_t height) {
        auto& params(*ParamsBase::Instance());
        return (height >= params.SegwitHeight);
    }


    static uint32_t GetBlockScriptFlags(const BlockIndex& index) {
        auto& params(*ParamsBase::Instance());

        unsigned int flags = SCRIPT_VERIFY_NONE;

        // fly 是否支持 pay to script hash 格式
        // BIP16 didn't become active until Apr 1 2012 (on mainnet, and
        // retroactively applied to testnet)
        // However, only one historical block violated the P2SH rules (on both
        // mainnet and testnet), so for simplicity, always leave P2SH
        // on except for the one violating block.
        if (params.BIP16Exception.IsNull() || // no bip16 exception on this chain
            index._hash.IsNull() || // this is a new candidate block, eg from TestBlockValidity()
            index._hash != params.BIP16Exception) // this block isn't the historical exception
        {
            flags |= SCRIPT_VERIFY_P2SH;
        }

        // fly 设置校验隔离见证脚本
        // Enforce WITNESS rules whenever P2SH is in effect (and the segwit
        // deployment is defined).
        if (flags & SCRIPT_VERIFY_P2SH && IsScriptWitnessEnabled()) {
            flags |= SCRIPT_VERIFY_WITNESS;
        }

        // fly BIP66 严格的DER签名
        // Start enforcing the DERSIG (BIP66) rule
        if (index._height >= params.BIP66Height) {
            flags |= SCRIPT_VERIFY_DERSIG;
        }

        // fly 锁定交易输出(out)在一定时间内不能被消费
        // Start enforcing CHECKLOCKTIMEVERIFY (BIP65) rule
        if (index._height >= params.BIP65Height) {
            flags |= SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY;
        }

        // Start enforcing BIP112 (CHECKSEQUENCEVERIFY)
        if (index._height >= params.CSVHeight) {
            flags |= SCRIPT_VERIFY_CHECKSEQUENCEVERIFY;
        }

        // 是否已经激活隔离见证
        // Start enforcing BIP147 NULLDUMMY (activated simultaneously with segwit)
        if (IsWitnessEnabled(index)) {
            flags |= SCRIPT_VERIFY_NULLDUMMY;
        }

        return flags;
    }


};

class BlockChain {

    DBBlockWrapper&                 _blockWrapper;
    DBBlockIndexWrapper&            _blockIndexWrapper;

    BlockTipManager&                _tipManager;
    UTXOManager&                    _utxoManager;

    std::multimap<uint256, Block::TCBlockPtr> _orphanBlocks; // key = block._hashPrevBlock

    static constexpr uint64_t s_PowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
    static constexpr uint64_t s_PowTargetSpacing  = 10 * 60;


    // Regtest时为true
    // MainNet时为false
    static constexpr bool s_PowAllowMinDifficultyBlocks = true;

    // Regtest时为true
    static constexpr bool s_PowNoRetargeting = true;


    /**
     * 保存区块与index并返回index
     * @param block 新块
     * @param prevBlockIndex 新块的父块index
     * @param createTip 是否创建tip
     * @return 新块的index
     */
    BlockIndex::TBlockIndexPtr SaveBlock(const Block& block, const BlockIndex& prevBlockIndex, bool updateOrCreateTip = false) {
        const uint256& hash = block.GetHash();
        int64_t height = BlockIndex::INVALID_BLOCK_HEIGHT;

        // 判断是否为创世区块
        if (prevBlockIndex.IsNull()) {
            if (hash != ParamsBase::Instance()->hashGenesisBlock) {
                return nullptr;
            }

            height = 0;
        } else {
            // 高度
            height = prevBlockIndex._height + 1;
        }

        // 保存区块
        if (!_blockWrapper.PutBlock(hash, block)) {
            AppLog::Error("%s, SaveBlock, PutBlock error", Info());
            return nullptr;
        }

        // 更新上一区块的Next指针
        prevBlockIndex._hashNextBlock = hash;
        if (!prevBlockIndex.IsNull() && !_blockIndexWrapper.Put(prevBlockIndex)) {
            AppLog::Error("%s, SaveBlock, update prev block index error", Info());
            return nullptr;
        }

        // 保存Index
        BlockIndex::TBlockIndexPtr index = std::make_shared<BlockIndex>(height, hash, block._hashPrevBlock);
        if (!_blockIndexWrapper.Put(index)) {
            AppLog::Error("%s, SaveBlock, save block index error", Info());
            return nullptr;
        }

        // 创建Tip
        if (updateOrCreateTip) {
            if (!_tipManager.UpdateTip(index)) {
                AppLog::Error("%s, SaveBlock, 更新或创建BlockTip错误.", Info());
                return nullptr;
            }
        }

        return std::move(index);
    }

public:
    const char* Info() {
        return "BlockChain";
    };

    static BlockChain* Instance();

    BlockChain():
        _blockWrapper(*DBBlockWrapper::Instance()),
        _blockIndexWrapper(*DBBlockIndexWrapper::Instance()),
        _tipManager(*BlockTipManager::Instance()),
        _utxoManager(*UTXOManager::Instance())
        {}

    void Initialize() {
        if (_tipManager.GetActiveTip() == nullptr) {
            // 保存创世块到DB中
            const auto& block = Block::GetGenesisBlock();

            auto index = SaveBlock(block, BlockIndex(), true);
            if (index == nullptr) {
                AppLog::Error("%s, Initialize, save genesis block and index error", Info());
                return ;
            }

            // 更新交易池，UTXO集合
            ProcessTransactions(block._vtx, index->_hash, index->_height);
        }
    }

    bool GetBlock(const uint256& hash, Block& block) {
        return _blockWrapper.GetBlock(hash, block);
    }

    struct CheckTransactionResult {
        CAmount nFees;
        int     nInputs;
        int64_t nSigOpsCost;
        CheckTransactionResult() :nFees(0), nInputs(0), nSigOpsCost(0) {}
    };
    bool CheckTransaction(const NetMessageTransaction& tx,
                          uint32_t scriptFlags,
                          int64_t height,
                          CheckTransactionResult& result,
                          TMemTXIndexPtrMap* memValidTxs = nullptr,
                          std::set<OutPoint>* memValidUTXO = nullptr,
                          int64_t forkHeight = BlockIndex::INVALID_BLOCK_HEIGHT) {

        const auto& hash = tx.GetHash();
        CAmount&     fees = result.nFees;
        int&         inputCount = result.nInputs;
        int64_t&     sigOpsCostCount = result.nSigOpsCost;
        CAmount      valueIn = 0;
        CAmount      valueOut = 0;


        inputCount += tx._vin.size();

        // Basic checks that don't depend on any context
        if (tx._vin.empty()) {
            AppLog::Error("%s, CheckTransaction, txhash: {%s}, bad-txns-vin-empty",
                    Info(), hash.ToString().c_str());
            return false;
        }

        if (tx._vout.empty()) {
            AppLog::Error("%s, CheckTransaction, txhash: {%s}, bad-txns-vout-empty",
                          Info(), hash.ToString().c_str());
            return false;
        }

        // Size limits (this doesn't take the witness into account, as that hasn't been checked for malleability)
        if (::GetSerializeSize(tx, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) * WITNESS_SCALE_FACTOR > MAX_BLOCK_WEIGHT) {
            AppLog::Error("%s, CheckTransaction, txhash: {%s}, bad-txns-oversize",
                          Info(), hash.ToString().c_str());
            return false;
        }

        // Check for negative or overflow output values (see CVE-2010-5139)
        int txoutIdx = 0;
        for (const auto& txOut : tx._vout) {
            AppLog::Info("%s, CheckTransaction,TxHash: {%s}, TxOUT: {%d}, {%s}",
                          Info(), hash.ToString().c_str(),
                          txoutIdx, tx.ToString().c_str());
            ++txoutIdx;
            if (txOut._nValue < 0) {
                AppLog::Error("%s, CheckTransaction, txhash: {%s}, bad-txns-vout-negative",
                              Info(), hash.ToString().c_str());
                return false;
            }
            if (txOut._nValue > MAX_MONEY) {
                AppLog::Error("%s, CheckTransaction, txhash: {%s}, bad-txns-vout-toolarge",
                              Info(), hash.ToString().c_str());
                return false;
            }
            valueOut += txOut._nValue;
            if (!MoneyRange(valueOut)) {
                AppLog::Error("%s, CheckTransaction, txhash: {%s}, bad-txns-txouttotal-toolarge",
                              Info(), hash.ToString().c_str());
                return false;
            }
        }

        // TODO Check 脚本内签名数量(暂时放弃)
        // GetTransactionSigOpCost counts 3 types of sigops:
        // * legacy (always)
        // * p2sh (when P2SH enabled in flags and excludes coinbase)
        // * witness (when witness enabled in flags and excludes coinbase)
//        sigOpsCostCount += GetTransactionSigOpCost(tx, view, flags);
//        if (nSigOpsCost > MAX_BLOCK_SIGOPS_COST) {
//            LogPrintf("ERROR: ConnectBlock(): too many sigops\n");
//            return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-blk-sigops");
//        }



        bool isCoinBase = tx.IsCoinBase();
        if (isCoinBase) {
            if (tx._vin.front()._scriptSig.size() < 2 || tx._vin.front()._scriptSig.size() > 100) {
                AppLog::Error("%s, CheckTransaction, txhash: {%s}, bad-cb-length",
                              Info(), hash.ToString().c_str());
                return false;
            }

            // 如果是coin base 交易，则不用校验input项
            return true;
        }


        // tx input Check
        std::set<OutPoint> vInOutPoints;
        for (auto txInI = 0; txInI < tx._vin.size(); ++txInI) {
            const auto& txIn = tx._vin[txInI];

            // Check txIn
            if (!isCoinBase && txIn._previousOutput.IsNull()) {
                AppLog::Error("%s, CheckTransaction, txhash: {%s}, bad-txns-prevout-null",
                              Info(), hash.ToString().c_str());
                return false;
            }

            // Check for duplicate inputs (see CVE-2018-17144)
            // While Consensus::CheckTxInputs does check if all inputs of a tx are available, and UpdateCoins marks all inputs
            // of a tx as spent, it does not check if the tx has duplicate inputs.
            // Failure to run this check will result in either a crash or an inflation bug, depending on the implementation of
            // the underlying coins database.
            if (!vInOutPoints.insert(txIn._previousOutput).second) {
                AppLog::Error("%s, CheckTransaction, txhash: {%s}, bad-txns-inputs-duplicate",
                              Info(), hash.ToString().c_str());
                return false;
            }


            ///////////////////////////////////////////////////////////////////////////////////////////////
            // 以下逻辑与下边的[获取input._previousOutput所引用的tx]逻辑相同
            // Check in utxo是否已有效
            {
                OutPoint out;
                const OutPoint *pOut = nullptr;
                if (memValidUTXO != nullptr && forkHeight > 0) {
                    auto it = memValidUTXO->find(txIn._previousOutput);
                    if (it == memValidUTXO->end()) {
                        if (UTXOManager::Instance()->CanUse(txIn._previousOutput, &out)) {
                            if (out._height > forkHeight) {
                                AppLog::Error("%s, CheckTransaction, Check in utxo是否已有效 error, txhash: {%s}, bad-txns-inputs-duplicate",
                                              Info(), hash.ToString().c_str());
                                return false;
                            }
                            pOut = &out;
                        }
                    } else {
                        pOut = &(*it);
                    }
                } else {
                    if (UTXOManager::Instance()->CanUse(txIn._previousOutput, &out)) {
                        pOut = &out;
                    }
                }

                // input 所引用的utxo不存在，或已经花费
                if (pOut == nullptr) {
                    AppLog::Error("%s, CheckTransaction, Check in utxo是否已有效 error, 引用的utxo不存在，或已经花费"
                                  "txhash: {%s}, bad-txns-inputs-duplicate",
                                  Info(), hash.ToString().c_str());
                    return false;
                }
            }
            ////////////////////////////////////////////////////////////////////////////////////////////


            ////////////////////////////////////////////////////////////////////////////////////////////
            // Check 签名解锁脚本
            // 获取input._previousOutput所引用的交易，获取其交易内的对所引用utxo的脚本.
            {
                DBTransactionIndexWrapper::TxIdx prevOutputTxIdx;
                DBTransactionIndexWrapper::TxIdx *pPrevOutputTxIdx = nullptr;

                // 如果memValidTxs(候选链已校验完的交易)不为空，则先在内存中查找(即在候选链内查找,代表此时正在进行候选链的Check)
                if (memValidTxs != nullptr && forkHeight > 0) {
                    auto it = memValidTxs->find(txIn._previousOutput._hashTransaction);
                    if (it == memValidTxs->end()) {

                        // 如果在候选链的交易中(memValidTxs)没有找到，则在DBTXIndex中查找
                        if (DBTransactionIndexWrapper::Instance()->Get(txIn._previousOutput._hashTransaction,
                                                                       prevOutputTxIdx)) {

                            // 如果是在进行候选链的Check时，在DBTXIndex找到的交易所在块的高度高于候选链与当前激活链的交叉点时，
                            // 则代表此交易对于候选链来说是无效的。因为DBTXIndex保存的是激活链的所有交易，所以如果在激活链中找到并且
                            // 高度 > 交叉点则代表交易是在高于交叉点的激活链上，相对于候选链来说，此找到的交易存在于其它分叉链上(当前激活链)。
                            if (pPrevOutputTxIdx->height > forkHeight) {
                                AppLog::Error("%s, CheckTransaction, Check 签名解锁脚本 error"
                                              "txhash: {%s}, bad-txns-inputs-duplicate",
                                              Info(), hash.ToString().c_str());
                                return false;
                            }

                            pPrevOutputTxIdx = &prevOutputTxIdx;
                        }
                    } else {
                        pPrevOutputTxIdx = it->second.get();
                    }

                } else {
                    // 如果memValidTxs为空,则直接在DBTXIndex中查找，即代表工作在激活链中(最长链)
                    if (DBTransactionIndexWrapper::Instance()->Get(txIn._previousOutput._hashTransaction,
                                                                   prevOutputTxIdx)) {
                        pPrevOutputTxIdx = &prevOutputTxIdx;
                    }
                }

                // 交易未找到
                if (pPrevOutputTxIdx == nullptr) {
                    AppLog::Error("%s, CheckTransaction, Check 签名解锁脚本 error, 引用的交易未找到: %s"
                                  "txhash: {%s}, bad-txns-inputs-duplicate",
                                  Info(),
                                  txIn._previousOutput._hashTransaction.ToString().c_str(),
                                  hash.ToString().c_str());
                    return false;
                }

                if (txIn._previousOutput._outPutIdx >= pPrevOutputTxIdx->tx._vout.size()) {
                    AppLog::Error("%s, CheckTransaction, prevOutpout.idx 超出范围", Info());
                    return false;
                }

                // 被input引用的utxo
                TxOut &utxo = pPrevOutputTxIdx->tx._vout[txIn._previousOutput._outPutIdx];

                // fly 如果input引用的tx是coinbase，则必须是成熟的(100个块以后)
                // If prev is coinbase, check that it's matured
                if (pPrevOutputTxIdx->tx.IsCoinBase() && height - pPrevOutputTxIdx->height < COINBASE_MATURITY) {
                    AppLog::Error("%s, CheckTransaction, Input 引用的是CoinBase，并且未成熟(100个块以后CoinBase才可用)", Info());
                    return false;
                }

                // fly 校验coin的范围
                // Check for negative or overflow input values
                valueIn += utxo._nValue;
                if (!MoneyRange(utxo._nValue) || !MoneyRange(valueIn)) {
                    AppLog::Error("%s, CheckTransaction, bad-txns-inputvalues-outofrange", Info());
                    return false;
                }




                // 校验脚本
                const CScript &scriptSig = txIn._scriptSig;
                const CScriptWitness *witness = &txIn._scriptWitness;
                ScriptError error;
                PrecomputedTransactionData txData(tx);
                bool isOKScriptCheck = VerifyScript(
                        scriptSig,
                        utxo._scriptPubKey,
                        witness,
                        scriptFlags,
                        CachingTransactionSignatureChecker(&tx, txInI, utxo._nValue, false, txData),
                        &error);
                if (!isOKScriptCheck) {
                    AppLog::Error("%s, CheckTransaction, VerifyScript error: %d", Info(), error);
                    return false;
                }

            }
            ////////////////////////////////////////////////////////////////////////////////////////////
        }


        // fly 校验比较输入的总和小于输出的总和
        // const CAmount value_out = tx.GetValueOut();
//                const CAmount value_out = tx.GetValueOut();
        if (valueIn < valueOut) {
            AppLog::Error("%s, CheckTransaction, bad-txns-in-belowout, inTotal: %d, outTotal: %d",Info(), valueIn, valueOut);
            return false;
//                    return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-in-belowout",
//                                         strprintf("value in (%s) < value out (%s)", FormatMoney(nValueIn), FormatMoney(value_out)));
        }

        // 校验并获取手续费
        // Tally transaction fees
        fees = (valueIn - valueOut);
        if (!MoneyRange(fees)) {
            // return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-fee-outofrange");
            AppLog::Error("%s, CheckTransaction, bad-txns-fee-outofrange, sumFees: %d",Info(), fees);
            return false;
        }


        return true;
    }

    // bool CheckTransactions(const std::list<NetMessageTransaction>& vtx) {
    bool CheckTransactions(const std::vector<NetMessageTransaction>& vtx,
                           uint32_t scriptFlags,
                           int64_t height,
                           TMemTXIndexPtrMap* memValidTxs = nullptr,
                           std::set<OutPoint>* memValidUTXO = nullptr,
                           int64_t forkHeight = BlockIndex::INVALID_BLOCK_HEIGHT) {

        CheckTransactionResult r;

        for(const auto& tx : vtx) {
            if (!CheckTransaction(tx, scriptFlags, height, r, memValidTxs, memValidUTXO, forkHeight)) {
               return false;
            }
        }


        return true;
    }



    /**
     * 1. 校验块块内所有交易
     * 2. 更新交易内存池
     * 3. 更新UTXO集合
     * @param block
     * @return
     */
    bool ProcessTransactions(const std::vector<NetMessageTransaction>& vtx,
                             const uint256& hashBlock, int64_t height) {
        // 更新交易内存池
        GPU_MINER_LOCK(TXMemPool::Instance()->Mutex());
        for (auto& tx : vtx) {

            // Remove已经被打包的交易
            TXMemPool::Instance()->Remove(tx.GetHash(), false);

            // 更新DBTXIndex
            DBTransactionIndexWrapper::TxIdx txIdx(tx, hashBlock, height);
            DBTransactionIndexWrapper::Instance()->Put(txIdx);

            // 更新UTXO集合
            _utxoManager.CacheUTXO(tx, hashBlock, height);
        }

        return true;
    }


    bool CheckBlock(const Block::TCBlockPtr block) {
        const auto& hash = block->GetHash();

        // 校验区块重量
        // 校验区块size
        // Size limits
        if (block->_vtx.empty() ||
            block->_vtx.size() * WITNESS_SCALE_FACTOR > MAX_BLOCK_WEIGHT ||
            GetSerializeSize(block, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) * WITNESS_SCALE_FACTOR > MAX_BLOCK_WEIGHT) {
            AppLog::Error("%s, CheckBlock, 无效的区块重量(size limits failed), Block: {%s}", Info(), block->ToString().c_str());
            return false;
        }

        // TODO Check 区块时间要> 过去11个区块的中间时间(非IDB场景)
        // Check timestamp against prev
//        if (block.GetBlockTime() <= pindexPrev->GetMedianTimePast())
//            return state.Invalid(BlockValidationResult::BLOCK_INVALID_HEADER, "time-too-old", "block's timestamp is too early");

        // TODO Check 区块时间要 < 未来2小时(非IBD场景)
//        // Check timestamp
//        if (block.GetBlockTime() > nAdjustedTime + MAX_FUTURE_BLOCK_TIME)
//            return state.Invalid(BlockValidationResult::BLOCK_TIME_FUTURE, "time-too-new", "block timestamp too far in the future");

        // TODO Block.version 版本也需要检查 (非IBD场景)

        // 1. 重复交易的校验
        // 2. 必须只是第一个交易是coinbase
        // 3. 新块内不能有重复的utxo
        // 4. 新块内的utxo校验是否已花费
        // 5. block问的签名数量

        // TODO 校验block总的签名数量(??)
        unsigned int nSigOps = 0;

        std::set<uint256> txids;
        std::set<OutPoint> utxos;
        for (auto it = block->_vtx.begin(); it != block->_vtx.end(); ++it) {

            const auto& txHash = it->GetHash();

            // 交易不能重复
            if (!txids.insert(txHash).second) {
                AppLog::Error("%s, Duplicate transaction check failed (tx hashBlock %s), Block: {%s}", Info(), block->ToString().c_str());
                return false;
            }

            // 第一笔交易必须是coinbase
            if (it == block->_vtx.begin() && !it->IsCoinBase()) {
                AppLog::Error("%s, First tx is not coinbase, Block: {%s}", Info(), block->ToString().c_str());
                return false;
            }

            // 其它交易不能是coinbase
            if (it != block->_vtx.begin() && it->IsCoinBase()) {
                AppLog::Error("%s, More than one coinbase, Block: {%s}", Info(), block->ToString().c_str());
                return false;
            }

            // 块内引用utxo重复校验,即是否有重复引且一个utxo的情况
            for (const auto& in : it->_vin) {
                if (!utxos.insert(in._previousOutput).second) {
                    AppLog::Error("%s, bad-txns-inputs-duplicate, Block: {%s}", Info(), block->ToString().c_str());
                    return false;
                }
            }

            nSigOps += NetMessageTransaction::GetLegacySigOpCount(*it);
            if (nSigOps * WITNESS_SCALE_FACTOR > MAX_BLOCK_SIGOPS_COST)  {
                AppLog::Error("%s, out-of-bounds SigOpCount, Block: {%s}", Info(), block->ToString().c_str());
                return false;
            }
        }

        // 校验MerkelRoot
        auto merkelRoot2 = block->MakeMerkleRoot();
        if (merkelRoot2->mutated || block->_hashMerkleRoot != merkelRoot2->hash) {
            AppLog::Error("%s, CheckBlock, MerkleRoot无效或存在重复交易hash, 原Merkle{%s}, 计算Merkle(%s), Block: %s",
                          Info(),
                          block->_hashMerkleRoot.ToString().c_str(),
                          merkelRoot2->hash.ToString().c_str(),
                          block->ToString().c_str());

            return false;
        }

        //
        // 校验块的数学问题 BEGIN
        const auto lastBlock = const_cast<BlockIndex*>(_tipManager.GetActiveTip().get())->GetBlock();
        if (lastBlock.IsNull()) {
            AppLog::Error("%s, CheckBlock, Get tip block in DB error {BlockHash: %s}",
                          Info(),
                          hash.ToString().c_str());
            return false;
        }

        // 区块nBits是否正确
        auto nowBits = GetNextWorkRequired(lastBlock, *block);
        if (block->_nBits != nowBits) {
            AppLog::Error("%s, CheckBlock, 新区块的难度值(nBits: %0X)无效, 现在的nBits: %0x",
                          Info(), block->_nBits, nowBits);
            return false;
        }

        // 校验块的数学问题
        if (!CheckProofOfWork(hash, block->_nBits)) {
            AppLog::Error("%s, CheckBlock, Check proof of work error {%s}", Info(),
                          hash.ToString().c_str());
            return false;
        }

        // 校验块的数学问题 End


        // TODO ContextualCheckBlock内的其它校验

        return true;
    }

    /**
     * 关于接收新块的规则:
     *  1. Index中存在多个链的Tip(链最新区块的index),其中只有一个是Active链(最长链，或存在多个高度相等分叉链时第一接收到块的那条链)
     *     本地关于TX的Check,UTXO的整理Check,及挖矿全部在Active链上进行。
     *     TODO 如果某个tip的高度与Active链的高度差超过6个块，则认为此链已无效，可以删除了。
     *  2. 新块的prev=ActiveTip，则是最长链。进行block的check,block内交易check,交易内存池更新，utxo集更新。
     *  3. 新块的prev=某个Tip && 此tip高度+1后高于Active链(即高于其它分叉链)，则根据prev向前追溯全部没有Check的块，并按块的增长顺序逐个对块进行Check，并同时
     *     更新交易内存池，UTXO集合，及更新此链分叉点区块的Next，使用其变为Active链
     *  4. 新块的prev=某个Tip && 此tip高度+1后<=Active链(即未超过最高链)，则只挂入此分叉链，不进行块的Check.
     *  5. 新块的prev != 某个Tip && prev找到，则是分叉块，则以此块做为分叉块挂入，并以此块创建一个Tip(新的分叉链)
     *  6. 新块的prev != 某个Tip && prev未找到，则是孤块，放入孤块池内。
     *
     * @param block
     * @return true: 接收成功
     */
    bool AcceptBlock(const Block::TCBlockPtr& block, bool isLocalMining = false);


    static const int64_t DifficultyAdjustmentInterval() {
        return s_PowTargetTimespan / s_PowTargetSpacing;
    }

    /**
     * 获取当前挖矿难度
     * @param lastBlock 最新的一个区块
     * @param block 要挖的区块
     * @return
     */
    static uint32_t GetNextWorkRequired(const Block& lastBlock, const Block& block) {
        auto& powLimit = ParamsBase::Instance()->powLimit;

        uint32_t nProofOfWorkLimit = UintToArith256(powLimit).GetCompact();

        auto difficultyAdjustmentInterval = DifficultyAdjustmentInterval();

        // Only change once per difficulty adjustment interval
        if ((BlockTipManager::Instance()->GetActiveTip()->_height+1) % difficultyAdjustmentInterval != 0) {
            if (s_PowAllowMinDifficultyBlocks) {

                if (block._nTime > lastBlock._nTime + s_PowTargetSpacing * 2)
                    return nProofOfWorkLimit;
                else {

                    // lastBlock == Instance()->GetTipBlockInfo()
                    Block tmpBlock  = lastBlock;
                    while(  !tmpBlock._hashPrevBlock.IsNull() &&
                            tmpBlock._nBits == nProofOfWorkLimit) {

                        // TODO 优化为内存中的Index
                        Instance()->GetBlock(tmpBlock._hashPrevBlock, tmpBlock);
                    }
                    return tmpBlock._nBits;
                }
            }
            return lastBlock._nBits;
        }

        // Go back by what we want to be 14 days worth of blocks(14天的块=2016个)
        // 获取前第2015块
        Block firstBlock = lastBlock;
        for (int i = 0; i < 2016 - 1; ++i) {
            Instance()->GetBlock(firstBlock._hashPrevBlock, firstBlock);
        }

        return CalculateNextWorkRequired(lastBlock, firstBlock._nTime);

    }

    static uint32_t CalculateNextWorkRequired(const Block& lastBlock,
            int64_t nFirstBlockTime) {

        if (s_PowNoRetargeting)
            return lastBlock._nBits;

        // Limit adjustment step
        int64_t nActualTimespan = lastBlock._nTime - nFirstBlockTime;
        if (nActualTimespan < s_PowTargetTimespan/4)
            nActualTimespan = s_PowTargetTimespan/4;
        if (nActualTimespan > s_PowTargetTimespan*4)
            nActualTimespan = s_PowTargetTimespan*4;

        // Retarget
        const arith_uint256 bnPowLimit = UintToArith256(ParamsBase::Instance()->powLimit);
        arith_uint256 bnNew;
        bnNew.SetCompact(lastBlock._nBits);
        bnNew *= nActualTimespan;
        bnNew /= s_PowTargetTimespan;

        if (bnNew > bnPowLimit)
            bnNew = bnPowLimit;

        return bnNew.GetCompact();
    }



    /**
     * 校验工作量证明
     * @param hash 需要校验的hash
     * @param nBits 难度值
     * @return true: ok, false: failured
     */
    static bool CheckProofOfWork(uint256 hash, unsigned int nBits) {

        // MainTest
        // static auto powLimit = uint256S("00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        // TestNet
        // static auto powLimit = uint256S("00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        // RegTest
        auto& powLimit = ParamsBase::Instance()->powLimit;

        bool fNegative;
        bool fOverflow;
        arith_uint256 bnTarget;

        bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

        AppLog::Debug("bnTarget: %s", bnTarget.ToString().c_str());
        // Check range
        if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(powLimit))
            return false;

        // Check proof of work matches claimed amount
        if (UintToArith256(hash) > bnTarget)
            return false;

        return true;
    }


    static CAmount GetBlockSubsidy(int nHeight) {
        int halvings = nHeight / ParamsBase::Instance()->nSubsidyHalvingInterval;

        // Force block reward to zero when right shift is undefined.
        if (halvings >= 64)
            return 0;

        CAmount nSubsidy = 50 * COIN;
        // Subsidy is cut in half every 210,000 blocks which will occur approximately every 4 years.
        nSubsidy >>= halvings;
        return nSubsidy;
    }
};




#endif //BITCOIN_GPU_MINER_BLOCKCHAIN_H
