//
// Created by fly on 2020/8/9.
//

#include "block.h"
#include "hash.h"
#include "blockchain.h"
#include "time_utils.h"


static Block CreateGenesisBlock(
        const char* pszTimestamp,
        const CScript& genesisOutputScript,
        uint32_t nTime,
        uint32_t nNonce,
        uint32_t nBits,
        int32_t nVersion,
        const CAmount& genesisReward) {

    NetMessageTransaction txNew;
    txNew._nVersion = 1;
    txNew._vin.resize(1);
    txNew._vout.resize(1);
    txNew._vin[0]._scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew._vout[0]._nValue = genesisReward;
    txNew._vout[0]._scriptPubKey = genesisOutputScript;

    Block genesis;
    genesis._nTime    = nTime;
    genesis._nBits    = nBits;
    genesis._nNonce   = nNonce;
    genesis._nVersion = nVersion;
    genesis._vtx.push_back(txNew);
    genesis._hashPrevBlock.SetNull();


    auto merkleRootResult = genesis.MakeMerkleRoot();
    genesis._hashMerkleRoot = merkleRootResult->hash;
    return genesis;
}


/**
 * 获取中位时间
 * @return
 */
int64_t BlockIndex::GetMedianTimePast() const {
    const static int medianTimeSpan = 11;

    Block it = GetBlock();

    int64_t pmedian[medianTimeSpan];
    for(int i = 0; i < 11; ++i) {
        pmedian[0] = it.GetBlockTime();

        if (!DBBlockWrapper::Instance()->GetBlock(it._hashPrevBlock, it)) {
            break;
        }
    }

    std::sort(pmedian, pmedian + medianTimeSpan);

    return pmedian[medianTimeSpan/2];
}


/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hashBlock=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashGenesisMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
 *   CTransaction(hashBlock=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
 *   vMerkleTree: 4a5e1e
 */
static Block CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward) {
    const char* pszTimestamp = "The Times 03/Jan/2009 Chancellor on brink of second bailout for banks";
    const CScript genesisOutputScript = CScript() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;

    AppLog::Debug("%s","CreateGenesisBlock");
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}


static Block::MerkleRootResult::TMerkleRootResult ComputeMerkleRoot(std::vector<uint256>& txHashes) {
    Block::MerkleRootResult::TMerkleRootResult r = std::make_shared<Block::MerkleRootResult>();
    uint256& hash = r->hash;
    bool& mutated = r->mutated;

    Hash::DoubleHash256 ctx;
    while (txHashes.size() > 1) {
        if (mutated) {

            // TODO 只是校验了相邻的两个hash是否相等，纯粹是为了解决CVE-2012-2459问题
            for (size_t pos = 0; pos + 1 < txHashes.size(); pos += 2) {
                if (txHashes[pos] == txHashes[pos + 1]) mutated = true;
            }
        }
        if (txHashes.size() & 1) { // 奇数
            txHashes.push_back(txHashes.back());
        }

        // 计算一层
        uint8_t* pIn = txHashes[0].begin();
        uint8_t* pOut = txHashes[0].begin();
        const size_t nextLevelHashCount = txHashes.size() / 2;
        size_t blocks = nextLevelHashCount;
        while(blocks > 0) {
            ctx.Write(pIn, 64);
            ctx.Finalize(pOut);

            pIn  += 64; // 两个hash长度
            pOut += 32; // 一个hash长度

            ctx.Reset();
            --blocks;
        }

        // 只是在缩小空间，所以不会触发重新分配内存的操作
        txHashes.resize(nextLevelHashCount);
    }
    if (txHashes.size() == 0) {
        return r;
    }

    hash = txHashes[0];

    return std::move(r);
}

Block::MerkleRootResult::TMerkleRootResult Block::MakeMerkleRoot() const {

    std::vector<uint256> txHashes;
    txHashes.reserve(_vtx.size());
    for (const auto &tx : _vtx) {
        txHashes.push_back(tx.GetHash());
    }

    return std::move(ComputeMerkleRoot(txHashes));
}


Block::MerkleRootResult::TMerkleRootResult Block::MakeWitnessMerkleRoot() const {

    std::vector<uint256> txHashes;
    txHashes.resize(_vtx.size());
    txHashes[0].SetNull(); // The witness hashBlock of the coinbase is 0.

    for(size_t i = 1; i < _vtx.size(); ++i) {
        txHashes[i] = _vtx[i].GetWitnessHash();
    }

    return std::move(ComputeMerkleRoot(txHashes));
}

Block& Block::GetGenesisBlock() {
    // RegNet genesis block
    static auto genesis = CreateGenesisBlock(1296688602, 2, 0x207fffff, 1, 50 * COIN);
    return genesis;
}




/**
 * 获取Block实体
 * @param cache 是否缓存此block
 * @return Block
 */
const Block& BlockIndex::GetBlock() const {
    if (!_block.IsNull()) {
        return _block;
    }

    if (!DBBlockWrapper::Instance()->GetBlock(_hash, _block)) {
        AppLog::Error("%s, BlockIndex::GetBlock, error", INFO);
    }

    return _block;
}

/**
 * 创建一个新的块
 * @param scriptPubKeyIn coinbase交易输出的地址
 * @return 新块
 */
Block::TBlockPtr Block::CreateNewBlock(const CScript& scriptPubKeyIn) {

    auto tip        = BlockTipManager::Instance()->GetActiveTip();
    auto lastBlock  = tip->GetBlock();
    auto block      = Block::MakeBlock();
    auto height     = tip->_height + 1;

    // TODO
    block->_nVersion = 0x20000000;

    block->_hashPrevBlock = tip->_hash;

    // 先添加一个空Coinbase交易占位
    block->_vtx.push_back(NetMessageTransaction());

    auto nMedianTimePast = tip->GetMedianTimePast();

    auto nLockTimeCutoff = (STANDARD_LOCKTIME_VERIFY_FLAGS & LOCKTIME_MEDIAN_TIME_PAST)
                            ? nMedianTimePast
                            : tip->GetBlock().GetBlockTime();

    // 从内存池中获取其它tx
    CAmount totalFees = 0;
    TXMemPool::Instance()->GetNewBlockTxs(height, nLockTimeCutoff, totalFees, block->_vtx);

    // 构造Coinbase交易
    auto& cbtx = block->_vtx[0];

    // 输入
    cbtx._vin.resize(1);
    cbtx._vin[0]._previousOutput.SetNull();
    cbtx._vin[0]._scriptSig = CScript() << height << OP_0;
    if (ScriptValidation::IsWitnessEnabled(height)) {
        cbtx._vin[0]._scriptWitness.stack.emplace_back(32, 0x00);
    }

    // 输出
    cbtx._vout.resize(2);
    cbtx._vout[0]._scriptPubKey = scriptPubKeyIn;
    cbtx._vout[0]._nValue = totalFees + BlockChain::GetBlockSubsidy(height);

    // 1-byte - OP_RETURN (0x6a)
    // 1-byte - Push the following 36 bytes (0x24)
    // 4-byte - Commitment header (0xaa21a9ed)
    // 32-byte - Commitment hash: Double-SHA256(witness root hash|witness reserved value)
    TxOut& commitmentOut = cbtx._vout[1];
    commitmentOut._nValue = 0;
    commitmentOut._scriptPubKey.resize(MINIMUM_WITNESS_COMMITMENT);
    commitmentOut._scriptPubKey[0] = OP_RETURN; // 1-byte - OP_RETURN (0x6a)
    commitmentOut._scriptPubKey[1] = 0x24;      // 1-byte - Push the following 36 bytes (0x24)
    // 4-byte - Commitment header (0xaa21a9ed)
    commitmentOut._scriptPubKey[2] = 0xaa;
    commitmentOut._scriptPubKey[3] = 0x21;
    commitmentOut._scriptPubKey[4] = 0xa9;
    commitmentOut._scriptPubKey[5] = 0xed;

    // 32-byte - Commitment hash: Double-SHA256(witness root hash|witness reserved value)
    // 隔离见证MerkelRoot
    auto witnessroot = block->MakeWitnessMerkleRoot();
    std::vector<unsigned char> ret(32, 0x00); // witness reserved value
    Hash::DoubleHash256()
            .Write((uint8_t*)witnessroot->hash.begin(), witnessroot->hash.size())
            .Write((uint8_t*)&ret[0], ret.size())
            .Finalize((uint8_t*)witnessroot->hash.begin());
    memcpy(&commitmentOut._scriptPubKey[6], witnessroot->hash.begin(), 32);

    // TODO 应该取已连接节点(应该是6个)的的中位时间
    block->_nTime = GetTime();

    block->_nBits  = BlockChain::GetNextWorkRequired(lastBlock, *block);
    block->_nNonce = 0;

    block->_hashMerkleRoot = block->MakeMerkleRoot()->hash;


    return block;
}

/**
 * 获取Block header 实体
 * @param cache 是否缓存此header
 * @return BlockHeader
 */
const BlockHeader& BlockIndex::GetBlockHeader() {
    if (_block.IsNull()) {
        GetBlock();
    }

    if (_blockHeader.IsNull()) {
        return _blockHeader;
    }

    _blockHeader = _block.GetBlockHeader();
    return _blockHeader;
}


void TestBlock() {
    auto genesis = CreateGenesisBlock(1296688602, 2, 0x207fffff, 1, 50 * COIN);
    auto hashGenesisBlock = genesis.GetHash();

    auto ok = uint256S("0x0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206");
    AppLog::Debug("      hashBlock: {%s, %s}", hashGenesisBlock.ToString().c_str(), ok.ToString().c_str());

    auto okMerkleRoot = uint256S("0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b");
    AppLog::Debug("MerkleRoot: {%s, %s}", genesis._hashMerkleRoot.ToString().c_str(), okMerkleRoot.ToString().c_str());

    assert(hashGenesisBlock == ok);
}