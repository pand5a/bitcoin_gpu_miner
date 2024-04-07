//
// Created by fly on 2020/10/10.
//
/**
 * TODO 交易input内的scriptSig是解锁它引用preout的
 *
 * TODO 需要实现(否则不能被其它节点接受)
 *  1. BIP68(搜索比特币源码查看实现)
 *
 * TODO 需要了解的BIP列表
 *  1. BIP66 严格的DER签名
 *  2. BIP65 锁定交易输出(out)在一定时间内不能被消费
 *
 * TODO 收到孤儿区块时（即缺少了某个区块)，此时向其他节点请求新的区块。
 * 节点始终都将最长的链条视为正确的链条，并持续工作和延长它。如果有两个节点同时广播不同版本的新区块，
 * 那么其他节点在接收到该区块的时间上将存在先后差别。当此情形，他们将在率先收到的区块基础上进行工作，
 * 但也会保留另外一个链条，以防后者变成最长的链条。该僵局（tie）的打破要等到下一个工作量证明被发现，
 * 而其中的一条链条被证实为是较长的一条，那么在另一条分支链条上工作的节点将转换阵营，开始在较长的链条上工作。
 * 所谓“新的交易要广播”，实际上不需要抵达全部的节点。只要交易信息能够抵达足够多的节点，那么他们将很快被整合进一个区块中。
 * 而区块的广播对被丢弃的信息是具有容错能力的。如果一个节点没有收到某特定区块，那么该节点将会发现自己缺失了某个区块，
 * 也就可以提出自己下载该区块的请求。
 */

#ifndef BITCOIN_GPU_MINER_TX_H
#define BITCOIN_GPU_MINER_TX_H

#include "net.h"

static const int SERIALIZE_BLOCK_HASH = 0x20000000;
static const int SERIALIZE_TXOUT_ISSPENT = 0x10000000;

/** Flags for nSequence and nLockTime locks */
/** Interpret sequence numbers as relative lock-time constraints. */
static constexpr unsigned int LOCKTIME_VERIFY_SEQUENCE = (1 << 0);
/** Use GetMedianTimePast() instead of nTime for end point timestamp. */
static constexpr unsigned int LOCKTIME_MEDIAN_TIME_PAST = (1 << 1);

/** Used as the flags parameter to sequence and nLocktime checks in non-consensus code. */
static constexpr unsigned int STANDARD_LOCKTIME_VERIFY_FLAGS = LOCKTIME_VERIFY_SEQUENCE |
                                                               LOCKTIME_MEDIAN_TIME_PAST;

/** Minimum size of a witness commitment structure. Defined in BIP 141. **/
static constexpr size_t MINIMUM_WITNESS_COMMITMENT{38};

class OutPoint {

public:
    using TOutPointPtr = std::shared_ptr<const OutPoint>;
    static TOutPointPtr Make(uint256 hash, uint32_t idx) {
        return std::make_shared<const OutPoint>(hash, idx);
    }

    bool        _isUsed;
    uint256     _hashBlock;      // _hashTransaction 所属的block
    int64_t     _height;         // block height


    uint256     _hashTransaction;   // 交易hash
    uint32_t    _outPutIdx;         // 交易内的utxo的idx从0开始

    static constexpr uint32_t NULL_INDEX = std::numeric_limits<uint32_t>::max();

    OutPoint() {
        SetNull();
    }
    OutPoint(uint256 hash, uint32_t idx) : _hashTransaction(hash), _outPutIdx(idx), _isUsed(false) {}
    OutPoint(uint256 hash, uint32_t idx, const uint256& hashBlock, int64_t height, bool isUsed)
    : _hashTransaction(hash), _outPutIdx(idx), _hashBlock(hashBlock), _height(height), _isUsed(isUsed) {}


    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        if (s.GetVersion() & SERIALIZE_BLOCK_HASH) {
            READWRITE(_hashBlock);
            READWRITE(_height);
            READWRITE(_isUsed);
        }

        READWRITE(_hashTransaction);
        READWRITE(_outPutIdx);
    }

    void SetNull() { _hashTransaction.SetNull(); _hashBlock.SetNull(); _outPutIdx = NULL_INDEX; _isUsed=false; _height = -1; }
    bool IsNull() const { return (_hashTransaction.IsNull() && _outPutIdx == NULL_INDEX); }


    friend bool operator<(const OutPoint& a, const OutPoint& b) {
        int cmp = a._hashTransaction.Compare(b._hashTransaction);
        return cmp < 0 || (cmp == 0 && a._outPutIdx < b._outPutIdx);
    }

    friend bool operator==(const OutPoint& a, const OutPoint& b) {
        return (a._hashTransaction == b._hashTransaction && a._outPutIdx == b._outPutIdx);
    }

    friend bool operator!=(const OutPoint& a, const OutPoint& b)    {
        return !(a == b);
    }


    std::string ToString() const {
        return std::move(str_format("OutPoint(%s, %u)", _hashTransaction.ToString().substr(0, 10).c_str(), _outPutIdx));
    }
};

class TxIn {
public:
    OutPoint    _previousOutput;
    CScript     _scriptSig;
    uint32_t    _sequence;

    // 单独序列化(在NetMessageTransaction中)
    CScriptWitness _scriptWitness;

    /* Setting nSequence to this value for every input in a transaction
     * disables nLockTime. */
    static const uint32_t SEQUENCE_FINAL = 0xffffffff;

    /* Below flags apply in the context of BIP 68*/
    /* If this flag set, CTxIn::nSequence is NOT interpreted as a
     * relative lock-time. */
    static const uint32_t SEQUENCE_LOCKTIME_DISABLE_FLAG = (1U << 31);

    /* If CTxIn::nSequence encodes a relative lock-time and this flag
     * is set, the relative lock-time has units of 512 seconds,
     * otherwise it specifies blocks with a granularity of 1. */
    static const uint32_t SEQUENCE_LOCKTIME_TYPE_FLAG = (1 << 22);

    /* If CTxIn::nSequence encodes a relative lock-time, this mask is
     * applied to extract that lock-time from the sequence field. */
    static const uint32_t SEQUENCE_LOCKTIME_MASK = 0x0000ffff;

    /* In order to use the same number of bits to encode roughly the
     * same wall-clock duration, and because blocks are naturally
     * limited to occur every 600s on average, the minimum granularity
     * for time-based relative lock-time is fixed at 512 seconds.
     * Converting from CTxIn::nSequence to seconds is performed by
     * multiplying by 512 = 2^9, or equivalently shifting up by
     * 9 bits. */
    static const int SEQUENCE_LOCKTIME_GRANULARITY = 9;


    TxIn() : _sequence(SEQUENCE_FINAL) {}


    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(_previousOutput);
        READWRITE(_scriptSig);
        READWRITE(_sequence);
    }

    friend bool operator==(const TxIn& a, const TxIn& b) {
        return (a._previousOutput == b._previousOutput &&
                a._scriptSig == b._scriptSig &&
                a._sequence == b._sequence);
    }

    friend bool operator!=(const TxIn& a, const TxIn& b)    {
        return !(a == b);
    }

    // These implement the weight = (stripped_size * 4) + witness_size formula,
    // using only serialization with and without witness data. As witness_size
    // is equal to total_size - stripped_size, this formula is identical to:
    // weight = (stripped_size * 3) + total_size.
    inline int64_t GetWeight() {
        // scriptWitness size is added here because witnesses and txins are split up in segwit serialization.
        return ::GetSerializeSize(*this, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) * (WITNESS_SCALE_FACTOR - 1) + ::GetSerializeSize(*this, PROTOCOL_VERSION) + ::GetSerializeSize(_scriptWitness.stack, PROTOCOL_VERSION);
    }


    std::string ToString() const {
        std::string str;
        str += "TxIn(";
        str += _previousOutput.ToString();
        if (_previousOutput.IsNull())
            str += str_format(", coinbase %s", HexStr(_scriptSig).c_str());
        else
            str += str_format(", scriptSig=%s", HexStr(_scriptSig).substr(0, 24).c_str());
        if (_sequence != SEQUENCE_FINAL)
            str += str_format(", nSequence=%u", _sequence);
        str += ")";
        return str;
    }



};


class TxOut {
public:
    CAmount _nValue;
    CScript _scriptPubKey;

    bool    _isSpent; // 是否已经花费

    TxOut() {
        SetNull();
    }

    TxOut(const CAmount& nValueIn, CScript scriptPubKeyIn);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(_nValue);
        READWRITE(_scriptPubKey);

        if (s.GetVersion() & SERIALIZE_TXOUT_ISSPENT) {
            READWRITE(_isSpent);
        }
    }

    void SetNull() {
        _nValue = -1;
        _scriptPubKey.clear();
        _isSpent = false;
    }

    bool IsNull() const {
        return (_nValue == -1);
    }

    bool IsSpent() {
        return _isSpent;
    }

    friend bool operator==(const TxOut& a, const TxOut& b) {
        return (a._nValue == b._nValue &&
                a._scriptPubKey == b._scriptPubKey);
    }

    friend bool operator!=(const TxOut& a, const TxOut& b) {
        return !(a == b);
    }

    std::string ToString() const {
        return std::move(str_format("TxOut(_nValue=%d.%08d, _scriptPubKey=%s)",
                                    (int)(_nValue / COIN),
                                    (int)(_nValue % COIN),
                                    HexStr(_scriptPubKey).substr(0, 30).c_str()));
    };

};



class NetMessageTransaction : public NetMessageBase {
private:
    /** Memory only. */
    uint256 _hash;
    uint256 _witnessHash;

    // Default transaction version.
    static const int32_t CURRENT_VERSION=2;
public:
    using TNetMessageTransactionPtr = std::shared_ptr<NetMessageTransaction>;
    static TNetMessageTransactionPtr MakeTransaction() {
        return std::make_shared<NetMessageTransaction>();
    }

    std::vector<TxIn>   _vin;
    std::vector<TxOut>  _vout;
    int32_t             _nVersion;
    uint32_t            _nLockTime;

    NetMessageTransaction() :
            _vin(),
            _vout(),
            _nVersion(CURRENT_VERSION),
            _nLockTime(0),
            _hash{},
            _witnessHash{} {
        _command = NetMsgTypeInstance.TX;
    }


    template<typename Stream>
    NetMessageTransaction(deserialize_type, Stream& s) {
        SetNull();
        Unserialize(s);
    }

    void SetNull() {
        _nVersion = CURRENT_VERSION;
        _nLockTime = 0;
        _command = NetMsgTypeInstance.VERSION;
    }

    bool IsNull() const {
        return _vin.empty() && _vout.empty();
    }

    ADD_SERIALIZE_METHODS;

    /**
    * Basic transaction serialization format:
    * - int32_t nVersion
    * - std::vector<TxIn> vin
    * - std::vector<TxOut> vout
    * - uint32_t nLockTime
    *
    * Extended transaction serialization format:
    * - int32_t nVersion
    * - unsigned char dummy = 0x00
    * - unsigned char flags (!= 0)
    * - std::vector<TxIn> vin
    * - std::vector<TxOut> vout
    * - if (flags & 1):
    *   - TxWitness wit;
    * - uint32_t nLockTime
    */
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {

        // bFallowWitness 是否序列化隔离见证的参数标志，由调用序列化时设置s的version实现
        const bool bFallowWitness = !(s.GetVersion() & SERIALIZE_TRANSACTION_NO_WITNESS);

        uint8_t flags = 0;

        READWRITE(_nVersion);

        //
        // 反序列化
        if (ser_action.ForRead()) {

            // 如果是隔离见证则flag是2 byte:
            //  byte1 是dummy = 0
            //  byte2 是flags != 0 (二进制00000001)
            // 如果不是隔离见证，则不存在falg字段，所以直接读取的是vin。
            // 所以读取时，则先读取一次vin，如果vin的size=0则意味着是隔离见证
            // 则进一步读取falgs确认。

            // 先读取vin，如果vin的size==0则读取隔离见证的flags
            s >> _vin;
            if (_vin.size() == 0 && bFallowWitness) {
                // 隔离见证交易
                s >> flags;
                if (flags != 0) {
                    // 如果是隔离见证则再次读取交易的输入及输出
                    s >> _vin;
                    s >> _vout;
                }
            } else {
                // 非隔离见证,读取交易输出
                s >> _vout;
            }

            // 读取隔离见证信息(解锁脚本)
            if (flags & 1 && bFallowWitness) {

                // 去掉标记，用以判断是否为非法标识
                flags ^= 1;

                // 读取见证信息(解锁脚本)
                for(auto &vin : _vin) {
                    s >> vin._scriptWitness.stack;
                }
            }

            if (flags) {
                // 无效的flags,即无效的交易
                _vin.clear();
                _vout.clear();
            }


        } else {

            // 序列化
            if (HasWitness() && bFallowWitness) {
                // 隔离见证交易
                flags |= 1;

                // flag 的两个byte
                s << (uint8_t)0; // byte1
                s << flags; // byte2
            }

            s << _vin;  // 交易输入
            s << _vout; // 交易输出

            if (flags & 1) {
                // 隔离见证的见证人信息(解锁脚本)
                for (const auto  &vin : _vin) {
                    s << vin._scriptWitness.stack;
                }
            }

        } // end if


        READWRITE(_nLockTime);
    }



    // Return sum of txouts.
    CAmount GetValueOut() const {
        CAmount nValueOut = 0;
        for (const auto& tx_out : _vout) {
            nValueOut += tx_out._nValue;
            if (!MoneyRange(tx_out._nValue) || !MoneyRange(nValueOut)) {
                // throw std::runtime_error(std::string(__func__) + ": value out of range");
                return -1;
            }
        }
        return nValueOut;
    };

    CAmount GetFees() const;



    uint256 GetHash() const {
        if (!_hash.IsNull()) {
            return _hash;
        }

        _hash = Hash::SerializeHash(*this, SER_GETHASH, SERIALIZE_TRANSACTION_NO_WITNESS);
        return _hash;
    }

    const uint256& GetWitnessHash() const {
        if (!_witnessHash.IsNull()) {
            return _witnessHash;
        }
        if (!HasWitness()) {
            _witnessHash = _hash;
            return _witnessHash;
        }
        _witnessHash = Hash::SerializeHash(*this, SER_GETHASH, 0);
        return _witnessHash;
    }

    /**
     * Get the total transaction size in bytes, including witness data.
     * "Total Size" defined in BIP141 and BIP144.
     * @return Total transaction size in bytes
     */
    unsigned int GetTotalSize() const {
        return ::GetSerializeSize(*this, PROTOCOL_VERSION);
    };

    bool IsCoinBase() const {
        return (_vin.size() == 1 && _vin[0]._previousOutput.IsNull());
    }

    friend bool operator==(const NetMessageTransaction& a, const NetMessageTransaction& b) {
        return a.GetHash() == b.GetHash();
    }

    friend bool operator!=(const NetMessageTransaction& a, const NetMessageTransaction& b)    {
        return a.GetHash() != b.GetHash();
    }


    /**
     * 判断交易是否为最终不可修改交易
     * @param nBlockHeight 交易所在块的高度, 如果是内存中的交易，则此值是当前激活链最顶端块的高度+1
     * @param nBlockTime
     * @return
     */
    bool IsFinalTx(int nBlockHeight, int64_t nBlockTime) {
        if (_nLockTime == 0)
            return true;
        if ((int64_t)_nLockTime < ((int64_t)_nLockTime < LOCKTIME_THRESHOLD ? (int64_t)nBlockHeight : nBlockTime))
            return true;
        for (const auto& txin : _vin) {
            if (!(txin._sequence == TxIn::SEQUENCE_FINAL))
                return false;
        }
        return true;
    }

    bool HasWitness() const {
        for (size_t i = 0; i < _vin.size(); i++) {
            if (!_vin[i]._scriptWitness.IsNull()) {
                return true;
            }
        }
        return false;
    }





    /**
    * Count ECDSA signature operations the old-fashioned (pre-0.6) way
    * @return number of sigops this transaction's outputs will produce when spent
    * @see CTransaction::FetchInputs
    */
    static uint32_t GetLegacySigOpCount(const NetMessageTransaction& tx) {
        uint32_t nSigOps = 0;
        for (const auto& txin : tx._vin) {
            nSigOps += txin._scriptSig.GetSigOpCount(false);
        }
        for (const auto& txout : tx._vout) {
            nSigOps += txout._scriptPubKey.GetSigOpCount(false);
        }
        return nSigOps;
    }


    // These implement the weight = (stripped_size * 4) + witness_size formula,
    // using only serialization with and without witness data. As witness_size
    // is equal to total_size - stripped_size, this formula is identical to:
    // weight = (stripped_size * 3) + total_size.
    inline int64_t GetWeight() {
        return ::GetSerializeSize(*this, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) * (WITNESS_SCALE_FACTOR - 1) + ::GetSerializeSize(*this, PROTOCOL_VERSION);
    }

    std::string ToString() const {
        std::string str;
        str += str_format("NetMessageTransaction(hashBlock=%s, ver=%d, vin.size=%u, vout.size=%u, nLockTime=%u)\n",
                          GetHash().ToString().substr(0,10).c_str(),
                          _nVersion,
                          _vin.size(),
                          _vout.size(),
                          _nLockTime);
        for (const auto& tx_in : _vin)
            str += "    " + tx_in.ToString() + "\n";
        for (const auto& tx_in : _vin)
            str += "    " + tx_in._scriptWitness.ToString() + "\n";
        for (const auto& tx_out : _vout)
            str += "    " + tx_out.ToString() + "\n";
        return str;
    };


};

#endif //BITCOIN_GPU_MINER_TX_H
