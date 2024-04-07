//
// Created by fly on 2020/8/9.
//

#ifndef BITCOIN_GPU_MINER_BLOCK_H
#define BITCOIN_GPU_MINER_BLOCK_H

#include "net.h"
#include "tx.h"

class Block;
class BlockHeader : public NetMessageBase {

public:
    using  TCBlockHeaderPtr = std::shared_ptr<const BlockHeader>;

    static TCBlockHeaderPtr MakeBlockHeader() {
        return std::make_shared<const BlockHeader>();
    }

    // header
    int32_t _nVersion;
    uint256 _hashPrevBlock;
    uint256 _hashMerkleRoot;
    uint32_t _nTime;
    uint32_t _nBits;
    uint32_t _nNonce;


    uint64_t _nTxCount; // only memroy

    BlockHeader() {
        SetNull();
    }

    template<typename Stream>
    BlockHeader(deserialize_type, Stream& s) {
       SetNull();
       Unserialize(s);
    }

    virtual bool IsHeader() {
        return true;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(_nVersion);
        READWRITE(_hashPrevBlock);
        READWRITE(_hashMerkleRoot);
        READWRITE(_nTime);
        READWRITE(_nBits);
        READWRITE(_nNonce);

        if (IsHeader() && !(s.GetVersion() & SER_GETHASH)) {
            if (ser_action.ForRead()) {
                _nTxCount = ReadCompactSize(s);
            } else {
                WriteCompactSize(s, _nTxCount);
            }
        }

    }
    void SetNull() {
        _nVersion = 0;
        _hashPrevBlock.SetNull();
        _hashMerkleRoot.SetNull();
        _nTime = 0;
        _nBits = 0;
        _nNonce = 0;
        _nTxCount = 0;
    }

    bool IsNull() const {
        return (_nBits == 0);
    }

    uint256 GetHash() const {
        return Hash::SerializeHash(*this);
    };

    int64_t GetBlockTime() const
    {
        return (int64_t)_nTime;
    }
};

using BlockHeaders = std::vector<BlockHeader::TCBlockHeaderPtr>;


class Block : public BlockHeader {
public:
    static const std::string& Info() {
        return std::move(std::string("Block"));
    }

    using TCBlockPtr = std::shared_ptr<const Block>;
    static TCBlockPtr MakeCBlock() {
        return std::make_shared<const Block>();
    }

    using TBlockPtr = std::shared_ptr<Block>;
    static TBlockPtr MakeBlock() {
        return std::make_shared<Block>();
    }



    // network and disk
    // TODO 优化为shard_ptr
    std::vector<NetMessageTransaction> _vtx;

    Block() {
        _command = NetMsgTypeInstance.BLOCK;
        SetNull();
    }

    template<typename Stream>
    Block(deserialize_type, Stream& s) {
        _command = NetMsgTypeInstance.BLOCK;

        SetNull();
        Unserialize(s);
    }

    virtual bool IsHeader() {
        return false;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(*static_cast<BlockHeader*>(this));
        READWRITE(_vtx);
    };

    void SetNull() {
        BlockHeader::SetNull();
        _vtx.clear();
    }

    BlockHeader GetBlockHeader() const
    {
        BlockHeader header;
        header._nVersion       = _nVersion;
        header._hashPrevBlock  = _hashPrevBlock;
        header._hashMerkleRoot = _hashMerkleRoot;
        header._nTime          = _nTime;
        header._nBits          = _nBits;
        header._nNonce         = _nNonce;
        return header;
    }

    // These implement the weight = (stripped_size * 4) + witness_size formula,
    // using only serialization with and without witness data. As witness_size
    // is equal to total_size - stripped_size, this formula is identical to:
    // weight = (stripped_size * 3) + total_size.
    inline int64_t GetWeight() {
        return ::GetSerializeSize(*this, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS)
        * (WITNESS_SCALE_FACTOR - 1) + ::GetSerializeSize(*this, PROTOCOL_VERSION);
    }

    std::string ToString() const {
        std::stringstream s;
        s << str_format("Block(hashBlock=%s, ver=0x%08x, hashPrevBlock=%s, hashGenesisMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%u)\n",
                        GetHash().ToString().c_str(),
                        _nVersion,
                        _hashPrevBlock.ToString().c_str(),
                        _hashMerkleRoot.ToString().c_str(),
                        _nTime,
                        _nBits,
                        _nNonce,
                        _vtx.size());
        for (const auto& tx : _vtx) {
            s << "  " << tx.ToString() << "\n";
        }
        return s.str();
    };

    // using MerkleRootResult = std::tuple<uint256, bool>;
    struct MerkleRootResult {
        uint256 hash;
        bool    mutated;
        MerkleRootResult() : mutated(false) {}
        using TMerkleRootResult = std::shared_ptr<MerkleRootResult>;
    };

    MerkleRootResult::TMerkleRootResult MakeMerkleRoot() const;
    MerkleRootResult::TMerkleRootResult MakeWitnessMerkleRoot() const;

    static Block& GetGenesisBlock();

    /**
     * 创建一个新的块
     * @param scriptPubKeyIn coinbase交易输出的地址
     * @param outBlock 新的block
     * @return true:ok
     */
    static Block::TBlockPtr CreateNewBlock(const CScript& scriptPubKeyIn);
};

 static Block CreateGenesisBlock(
         const char* pszTimestamp,
         const CScript& genesisOutputScript,
         uint32_t nTime,
         uint32_t nNonce,
         uint32_t nBits,
         int32_t nVersion,
         const CAmount& genesisReward);

 static Block CreateGenesisBlock(uint32_t nTime,
                                 uint32_t nNonce,
                                 uint32_t nBits,
                                 int32_t nVersion,
                                 const CAmount& genesisReward);
 extern void TestBlock();


 class BlockIndex {
 public:
     const char* INFO = "BlockIndex";

     const  static int64_t INVALID_BLOCK_HEIGHT = -1; // 无效区块高度
     using   TCBlockIndexPtr = std::shared_ptr<const BlockIndex>;
     using   TBlockIndexPtr  = std::shared_ptr<BlockIndex>;
     using   TCBlockIndexPtrs = std::vector<TCBlockIndexPtr>;
     using   TCBlockIndexPtrMap = std::map<uint256, TCBlockIndexPtr>;

     int64_t                         _height;
     uint256                         _hash;
     uint256                         _hashPrevBlock;
     uint256                         _hashNextBlock;

     // Only memory
     mutable Block                   _block;
     BlockHeader                     _blockHeader;
 public:
     ADD_SERIALIZE_METHODS;
     template <typename Stream, typename Operation>
     inline void SerializationOp(Stream& s, Operation ser_action) {
         READWRITE(_height);
         READWRITE(_hash);
         READWRITE(_hashPrevBlock);
         READWRITE(_hashNextBlock);
     };

     BlockIndex() :_height(INVALID_BLOCK_HEIGHT){};
     BlockIndex(int64_t height, const uint256& hash, const uint256& prevBlock):
         _height(height),
         _hash(hash),
         _hashPrevBlock(prevBlock) {};

     BlockIndex(const Block& block, int64_t height = INVALID_BLOCK_HEIGHT):
             _height(height),
             _hash(block.GetHash()),
             _hashPrevBlock(block._hashPrevBlock) {};

     template<typename Stream>
     BlockIndex(deserialize_type, Stream& s) :BlockIndex() {
         Unserialize(s);
     }
     ~BlockIndex() {};

     bool operator == (const BlockIndex& o) {
         if (this == &o) {
             return true;
         }

         return o._hash == _hash;
     }

     const bool IsNull () const {
         return _hash.IsNull();
     }


     /**
      * 创建const BlockIndex实体
      * @return const BlockIndex
      */
    static TCBlockIndexPtr Make() {
        return std::make_shared<const BlockIndex>();
    }

    /**
     * 创建BlockIndex实体
     * @return BlockIndex
     */
//    static TBlockIndexPtr Make() {
//        return std::make_shared<BlockIndex>();
//    }



    /**
     * 获取Block实体
     * @param cache 是否缓存此block
     * @return Block
     */
    const Block& GetBlock() const;

    /**
     * 获取Block header 实体
     * @param cache 是否缓存此header
     * @return BlockHeader
     */
    const BlockHeader& GetBlockHeader();

    /**
     * 获取中位时间
     * @return
     */
    int64_t GetMedianTimePast() const;
};


#endif //BITCOIN_GPU_MINER_BLOCK_H
