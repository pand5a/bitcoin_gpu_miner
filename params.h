//
// Created by fly on 2020/10/21.
//

#ifndef BITCOIN_GPU_MINER_PARAMS_H
#define BITCOIN_GPU_MINER_PARAMS_H

#include "common.h"
#include "uint256.h"

enum DeploymentPos {

    DEPLOYMENT_TESTDUMMY,
    // NOTE: Also add new deployments to VersionBitsDeploymentInfo in versionbits.cpp
    MAX_VERSION_BITS_DEPLOYMENTS
};

/**
 * Struct for each individual consensus rule change using BIP9.
 */
struct BIP9Deployment {
    /** Bit position to select the particular bit in nVersion. */
    int bit;
    /** Start MedianTime for version bits miner confirmation. Can be a date in the past */
    int64_t nStartTime;
    /** Timeout/expiry MedianTime for the deployment attempt. */
    int64_t nTimeout;

    /** Constant for nTimeout very far in the future. */
    static constexpr int64_t NO_TIMEOUT = std::numeric_limits<int64_t>::max();

    /** Special value for nStartTime indicating that the deployment is always active.
     *  This is useful for testing, as it means tests don't need to deal with the activation
     *  process (which takes at least 3 BIP9 intervals). Only tests that specifically test the
     *  behaviour during activation cannot use this. */
    static constexpr int64_t ALWAYS_ACTIVE = -1;
};

struct GenesisBlockParams {
    uint32_t nTime;
    uint32_t nNonce;
    uint32_t nBits;
    int32_t nVersion;
};

class ParamsBase {

public:
    DEF_CLASS_SINGLETON(ParamsBase);
    static void Initialize();

    uint16_t port;

    uint8_t pchMessageStart[4];
    GenesisBlockParams genesisBlockParams;
    uint256     hashGenesisBlock;
    uint256     hashGenesisMerkleRoot;
//    consensus.hashGenesisBlock = genesis.GetHash();
//    Block genesis = CreateGenesisBlock(1296688602, 2, 0x207fffff, 1, 50 * COIN);
//    consensus.hashGenesisBlock = genesis.GetHash();

    int nSubsidyHalvingInterval;
    /* Block hashBlock that is excepted from BIP16 enforcement */
    uint256 BIP16Exception;
    /** Block height and hashBlock at which BIP34 becomes active */
    int BIP34Height;
    uint256 BIP34Hash;
    /** Block height at which BIP65 becomes active */
    int BIP65Height;
    /** Block height at which BIP66 becomes active */
    int BIP66Height;
    /** Block height at which CSV (BIP68, BIP112 and BIP113) becomes active */
    int CSVHeight;
    /** Block height at which Segwit (BIP141, BIP143 and BIP147) becomes active.
     * Note that segwit v0 script rules are enforced on all blocks except the
     * BIP 16 exception blocks. */
    int SegwitHeight;
    /** Don't warn about unknown BIP 9 activations below this height.
     * This prevents us from warning about the CSV and segwit activations. */
    int MinBIP9WarningHeight;
    /**
     * Minimum blocks including miner confirmation of the total of 2016 blocks in a retargeting period,
     * (nPowTargetTimespan / nPowTargetSpacing) which is also used for BIP9 deployments.
     * Examples: 1916 for 95%, 1512 for testchains.
     */
    uint32_t nRuleChangeActivationThreshold;
    uint32_t nMinerConfirmationWindow;
    BIP9Deployment vDeployments[MAX_VERSION_BITS_DEPLOYMENTS];
    /** Proof of work parameters */
    uint256 powLimit;
    bool fPowAllowMinDifficultyBlocks;
    bool fPowNoRetargeting;
    int64_t nPowTargetSpacing;
    int64_t nPowTargetTimespan;
    int64_t DifficultyAdjustmentInterval() const { return nPowTargetTimespan / nPowTargetSpacing; }
    uint256 nMinimumChainWork;
    uint256 defaultAssumeValid;

    std::string bech32_hrp;

    struct TDBPath {
        std::string blocks;
        std::string blockIndex;
        std::string txIndex;
        std::string utxo;
    };

    TDBPath dbPaths;
    /**
     * If true, witness commitments contain a payload equal to a Bitcoin Script solution
     * to the signet challenge. See BIP325.
     */
//    bool signet_blocks{false};
//    std::vector<uint8_t> signet_challenge;
};

class RegTestParams : public ParamsBase {
public:
    RegTestParams() {
        port = 18444;

//        signet_blocks = false;
//        signet_challenge.clear();
        nSubsidyHalvingInterval = 150;
        BIP16Exception = uint256();
        BIP34Height = 500; // BIP34 activated on regtest (Used in functional tests)
        BIP34Hash = uint256();
        BIP65Height = 1351; // BIP65 activated on regtest (Used in functional tests)
        BIP66Height = 1251; // BIP66 activated on regtest (Used in functional tests)
        CSVHeight = 432; // CSV activated on regtest (Used in rpc activation tests)
        SegwitHeight = 0; // SEGWIT is always activated on regtest unless overridden
        MinBIP9WarningHeight = 0;
        powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        nPowTargetSpacing = 10 * 60;
        fPowAllowMinDifficultyBlocks = true;
        fPowNoRetargeting = true;
        nRuleChangeActivationThreshold = 108; // 75% for testchains
        nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)
        vDeployments[DEPLOYMENT_TESTDUMMY].bit = 28;
        vDeployments[DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        vDeployments[DEPLOYMENT_TESTDUMMY].nTimeout = BIP9Deployment::NO_TIMEOUT;

        // The best chain should have at least this much work.
        nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        defaultAssumeValid = uint256S("0x00");

        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xda;
        genesisBlockParams = {1296688602,2, 0x207fffff, 1};
        hashGenesisBlock = uint256S("0x0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206");
        hashGenesisMerkleRoot   = uint256S("0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b");

        bech32_hrp = "bcrt";

        dbPaths.blocks = std::string(DB_BASE_PATH) + "/data/blocks";
        dbPaths.blockIndex = std::string(DB_BASE_PATH) + "/data/index";
        dbPaths.txIndex =  std::string(DB_BASE_PATH) + "/data/tx";
        dbPaths.utxo =  std::string(DB_BASE_PATH) + "/data/utxo";
    }
};

class SigNetParams :public ParamsBase {
public:
    SigNetParams() {

    }
};

#endif //BITCOIN_GPU_MINER_PARAMS_H
