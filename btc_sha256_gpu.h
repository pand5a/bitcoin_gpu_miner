//
// Created by fly on 2020/6/17.
//
/**
 * 1. 此模块只对bitcoin的block header(80 byte)做sha256，并不是通用的sha256算法
 * 2. 先改变nonce，如果nonce全部遍历完成还未找到有效的nonce，则改变header内的time范围限制在当前unix时间的前后共1800秒内
 *    如果还未找到，则定义为本次挖矿失败，需要改变block本身并重新生成merkle root hash后，再次执行本模块
 *    TODO 暂时只改变了Nonce
 * 3. 并行原理:
 *      1. 每次同时放置Compute Unit * Workitem 的并行任务给gpu
 *      2. 并行任务的内容为：sha256的64次运算
 *      3. 一次计算后，将结果(32byte*Compute Unit * Workitem)拷贝出，并交给另一线程Check结果
 */

#ifndef BITCOIN_GPU_MINER_BTC_SHA256_GPU_H
#define BITCOIN_GPU_MINER_BTC_SHA256_GPU_H

#include <string>
#include <ctime>
#include <thread>

#include "common.h"
#include "uint256.h"
#include "net.h"
#include "arith_uint256.h"
#include "block.h"


#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

class BTC_SHA256_GPU {
public:
    ~BTC_SHA256_GPU();
    DEF_CLASS_SINGLETON(BTC_SHA256_GPU);
    const char* Info = "BTC_SHA256_GPU";

    enum {
        RUNING = 1,
        RESTERT,
        STOP
    };

    static constexpr uint8_t HASH_SIZE                  = 32;
    static constexpr uint8_t PADDED_BLOCKHEADER_SIZE    = 128;

    using TBlockHeaderBytes                 = std::vector<uint8_t>;
    using TGetNewBlockHeaderBytesCallback   = std::function<void(Block::TBlockPtr& block)>;
    using TFoundNewBlockCallback            = TGetNewBlockHeaderBytesCallback;

    using TResults      = std::vector<uint256>;
    using TResultsPtr   = std::shared_ptr<TResults>;
private:
    BTC_SHA256_GPU();
    using KernelSourceBuffer = std::vector<char>;
    struct TCLState {
        struct TMEMBuffer {
            cl_mem buffer;
            size_t size;
        };
        cl_device_id        deviceId;
        cl_context          context;
        cl_kernel           kernel;
        cl_command_queue    commandQueue;
        cl_program          program;
        cl_ulong            maxGlobalMemSize;
        TMEMBuffer          inputBuffer;
        TMEMBuffer          outputBuffer;
        cl_uint             maxComputeUnits;
        size_t              maxGourpItems;

        inline uint32_t threadCount() {
            return maxComputeUnits * maxGourpItems;
        }
    };

    unsigned char   _headerData[80]; // header 数据
    std::time_t     _unixTime;
    TCLState        _clState;

    TGetNewBlockHeaderBytesCallback _getNewBlockHeaderBytesCallback; // 获取新的需要挖矿的blockheader数据的callback
    TFoundNewBlockCallback          _fondNewBlockCallback; // 发现新块(挖矿成功)时的callback
    std::atomic_char _runState;
    bool             _isStarted;
    AsioWorker       _checkResultWorker;
    std::thread      _doHash;

    void InitializeOpenCL();
    bool LoadKernelSource(KernelSourceBuffer& src);

    void CheckResults(TResultsPtr vResult, uint32_t nonceOffset, Block::TBlockPtr, const arith_uint256& bnTarget);
    void PadBlockHaderData(TBlockHeaderBytes& vBytes);
public:

    /**
     * 阻塞执行GPU指令
     * @param bnTarget 比特币目标难度
     * @param getNewDataCallback  获取新的需要挖矿的blockheader数据的callback
     * @param foundNewBlockCallback 发现新块(挖矿成功)时的callback
     */
    void SyncStart(TGetNewBlockHeaderBytesCallback getNewDataCallback,
                    TFoundNewBlockCallback foundNewBlockCallback);
    void Reset();
    void Stop();


    void CPUPperformance();
    void GPUPperformance();
};


#endif //BITCOIN_GPU_MINER_BTC_SHA256_GPU_H
