//
// Created by fly on 2020/6/17.
//

#include "btc_sha256_gpu.h"

#include <iostream>
#include <vector>
#include <fstream>
#include "ease_log.h"
#include "time_utils.h"
#include "blockchain.h"


#include "keyio.h"

using namespace std;

#define ToBE16(x) \
    ((__uint16_t)((((__uint16_t)(x) & 0xff00) >> 8) | \
	        (((__uint16_t)(x) & 0x00ff) << 8)))

#define ToBE32(x) \
    ((uint32_t)((((uint32_t)(x) & 0xff000000) >> 24) | \
	        (((uint32_t)(x) & 0x00ff0000) >>  8) | \
	        (((uint32_t)(x) & 0x0000ff00) <<  8) | \
	        (((uint32_t)(x) & 0x000000ff) << 24)))

#define ToBE64(x) \
    ((uint64_t)((((uint64_t)(x) & 0xff00000000000000ULL) >> 56) | \
	        (((uint64_t)(x) & 0x00ff000000000000ULL) >> 40) | \
	        (((uint64_t)(x) & 0x0000ff0000000000ULL) >> 24) | \
	        (((uint64_t)(x) & 0x000000ff00000000ULL) >>  8) | \
	        (((uint64_t)(x) & 0x00000000ff000000ULL) <<  8) | \
	        (((uint64_t)(x) & 0x0000000000ff0000ULL) << 24) | \
	        (((uint64_t)(x) & 0x000000000000ff00ULL) << 40) | \
	        (((uint64_t)(x) & 0x00000000000000ffULL) << 56)))


//获取设备信息
void PrintDeviceInfo(cl_device_id id) {
    char        value[1024] = {0};
    size_t      valueSize;
    size_t      maxWorkItemPerGroup;
    cl_uint     maxComputeUnits=0;
    cl_ulong    maxGlobalMemSize=0;
    cl_ulong    maxConstantBufferSize=0;
    cl_ulong    maxLocalMemSize=0;

    cl_ulong    maxPrivateMemSize=0;

    memset(value, 0, 1024);

    ///print the device name
    clGetDeviceInfo(id, CL_DEVICE_NAME, 0, NULL, &valueSize);
    clGetDeviceInfo(id, CL_DEVICE_NAME, valueSize, value, NULL);
    printf("Device Name: %s\n", value);

    /// print parallel compute units(CU)
    clGetDeviceInfo(id, CL_DEVICE_MAX_COMPUTE_UNITS,sizeof(maxComputeUnits), &maxComputeUnits, NULL);
    printf("Parallel compute units: %u\n", maxComputeUnits);

    ///maxWorkItemPerGroup
    clGetDeviceInfo(id, CL_DEVICE_MAX_WORK_GROUP_SIZE,sizeof(maxWorkItemPerGroup), &maxWorkItemPerGroup, NULL);
    printf("maxWorkItemPerGroup: %zd\n", maxWorkItemPerGroup);

    /// print maxGlobalMemSize
    clGetDeviceInfo(id, CL_DEVICE_GLOBAL_MEM_SIZE,sizeof(maxGlobalMemSize), &maxGlobalMemSize, NULL);
    printf("maxGlobalMemSize: %llu(MB)\n", maxGlobalMemSize/1024/1024);

    /// print maxConstantBufferSize
    clGetDeviceInfo(id, CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE,sizeof(maxConstantBufferSize), &maxConstantBufferSize, NULL);
    printf("maxConstantBufferSize: %llu(KB)\n", maxConstantBufferSize/1024);


    /// print maxLocalMemSize
    clGetDeviceInfo(id, CL_DEVICE_LOCAL_MEM_SIZE,sizeof(maxLocalMemSize), &maxLocalMemSize, NULL);
    printf("maxLocalMemSize: %llu(KB)\n", maxLocalMemSize/1024);

//    /// print maxLocalMemSize
//    clGetDeviceInfo(id, CL_KERNEL_PRIVATE_MEM_SIZE,sizeof(maxPrivateMemSize), &maxPrivateMemSize, NULL);
//    printf("maxPrivateMemSize: %llu(KB)\n", maxPrivateMemSize/1024);



}




IMP_CLASS_SINGLETON(BTC_SHA256_GPU);
BTC_SHA256_GPU::BTC_SHA256_GPU() :
_unixTime(0),
_getNewBlockHeaderBytesCallback(nullptr),
_runState(RESTERT),
_isStarted(false)
{

    InitializeOpenCL();
}

void BTC_SHA256_GPU::InitializeOpenCL() {
    // 初始化
    memset(&_clState, 0, sizeof(_clState));

    // 默认使用第一个显卡
    auto err = clGetDeviceIDs(NULL, CL_DEVICE_TYPE_GPU, 1, &_clState.deviceId,  NULL);
    AppLog::CheckError(err, "Error: Failed get count for device");

//    PrintDeviceInfo(_clState.deviceId);

    // 获取最大计算单元数
    err = clGetDeviceInfo(_clState.deviceId, CL_DEVICE_MAX_COMPUTE_UNITS,sizeof(_clState.maxComputeUnits), &_clState.maxComputeUnits, NULL);
    AppLog::CheckError(err, "Error: Failed get parallel compute units ");

    // 获取最全局内存size
    err = clGetDeviceInfo(_clState.deviceId, CL_DEVICE_GLOBAL_MEM_SIZE,sizeof(_clState.maxGlobalMemSize), &_clState.maxGlobalMemSize, NULL);
    AppLog::CheckError(err, "Error: Failed get global memory size");

    // Create a compute context
    _clState.context = clCreateContext(0, 1, &_clState.deviceId, NULL, NULL, &err);
    AppLog::CheckError(_clState.context , "Error: Failed to create a compute context");

    // Create a command commands
    // 使用获取到的GPU设备和上下文环境监理一个命令队列，其实就是给GPU的任务队列
    _clState.commandQueue = clCreateCommandQueue(_clState.context, _clState.deviceId, 0, &err);
    AppLog::CheckError(_clState.commandQueue, "Error: Failed to create a command commands!");

    // load kernel source
    KernelSourceBuffer kernelSourceBuffer;
    if (!LoadKernelSource(kernelSourceBuffer)) {
        AppLog::CheckError(1, "Error: Failed to loading kernel sources!");
    }
    size_t kernelSize = kernelSourceBuffer.size();
    char* kernelBuf = &kernelSourceBuffer[0];

    _clState.program = clCreateProgramWithSource(_clState.context, 1, (const char **)&kernelBuf, &kernelSize, &err);
    AppLog::CheckError(err, "Error: Failed get count for device");

    // 编译kernel程序
    err = clBuildProgram(_clState.program, 1, &_clState.deviceId, NULL, NULL, NULL);
    if (err != CL_SUCCESS) {
        std::string buf;
        size_t logSize = 0;

        clGetProgramBuildInfo(_clState.program, _clState.deviceId, CL_PROGRAM_BUILD_LOG, 0, NULL, &logSize);
        buf.resize(logSize + 1);

        clGetProgramBuildInfo(_clState.program, _clState.deviceId, CL_PROGRAM_BUILD_LOG, logSize+1, &buf[0], NULL);
        AppLog::CheckError(err, string("Build kernel error: ") + buf);
    }

    _clState.kernel = clCreateKernel(_clState.program, "BtcDoubleSHA256", &err);
    AppLog::CheckError(err, "Error: Failed create kernel");

    // Get the maximum work group size for executing the kernel on the device
    err = clGetKernelWorkGroupInfo(_clState.kernel, _clState.deviceId, CL_KERNEL_WORK_GROUP_SIZE, sizeof(_clState.maxGourpItems), &_clState.maxGourpItems, NULL);
    AppLog::CheckError(err, "Error: Failed to retrieve kernel work group info!");

    // 创建输入内存
    // sha256补位后的blockheader数据长度
    _clState.inputBuffer.size = PADDED_BLOCKHEADER_SIZE;
    _clState.inputBuffer.buffer = clCreateBuffer(_clState.context, CL_MEM_READ_ONLY, _clState.inputBuffer.size, NULL, &err);
    AppLog::CheckError(_clState.inputBuffer.buffer, "Error: Memory input failed to create!");

    // 创建输出内存
    // 输出内存Size = 当前显卡最大并行数(CU*PE) * Hash size(32Byte)
    _clState.outputBuffer.size = _clState.threadCount() * HASH_SIZE;
    _clState.outputBuffer.buffer = clCreateBuffer(_clState.context, CL_MEM_WRITE_ONLY, _clState.outputBuffer.size, NULL, &err);
    AppLog::CheckError(_clState.outputBuffer.buffer, "Error: Memory output failed to create!");

    err = clSetKernelArg(_clState.kernel, 0, sizeof(cl_mem), &_clState.inputBuffer.buffer);
    AppLog::CheckError(err, "Error: clSetKernelArg 1");
    err = clSetKernelArg(_clState.kernel, 1, sizeof(cl_mem), &_clState.outputBuffer.buffer);
    AppLog::CheckError(err, "Error: clSetKernelArg 2") ;
}
template<typename FN, typename T>
void ReleaseCLObjects(FN* fn, T& t) {
    if (t != nullptr) {
        fn(t);
    }
}

BTC_SHA256_GPU::~BTC_SHA256_GPU() {

    // Shutdown and cleanup
    //
    ReleaseCLObjects(&clReleaseMemObject, _clState.inputBuffer.buffer);
    ReleaseCLObjects(&clReleaseMemObject, _clState.outputBuffer.buffer);
    ReleaseCLObjects(&clReleaseProgram, _clState.program);
    ReleaseCLObjects(&clReleaseKernel, _clState.kernel);
    ReleaseCLObjects(&clReleaseCommandQueue, _clState.commandQueue);
    ReleaseCLObjects(&clReleaseContext, _clState.context);
}

bool BTC_SHA256_GPU::LoadKernelSource(KernelSourceBuffer& src) {

    std::ifstream is ("btc_sha256_gpu.cl", std::ifstream::in);
    if (!is) {
        return false;
    }

    // 获取文件长度
    is.seekg(0, is.end);
    auto sourceLength = is.tellg();
    is.seekg(0, is.beg);

    try { src.resize(sourceLength); }
    catch (const std::bad_alloc& ex) {
        is.close();
        return false;
    }

    is.read(src.data(),sourceLength);
    is.close();
    return true;
}

void BTC_SHA256_GPU::CheckResults(TResultsPtr vResult,
                                  uint32_t nonceOffset,
                                  Block::TBlockPtr block,
                                  const arith_uint256& bnTarget) {
    boost::asio::post(_checkResultWorker.IoContext(), [this, vResult, nonceOffset, block, bnTarget]() {

        for(auto i = 0; i < vResult->size(); ++i) {

            auto nonce = nonceOffset + i;
            const auto& hash = (*vResult)[i];

             if (UintToArith256(hash) <= bnTarget) {
                 AppLog::Debug("CheckRESULT: nonce {%d}, %s", nonce, block->GetHash().ToString().c_str());

                 // 保存有效Nonce到区块头中
                 block->_nNonce = nonce;
                 Reset();
                 return ;
            }
        }
    });
}

// 将blockheader的80byte数据补位到128byte
void BTC_SHA256_GPU::PadBlockHaderData(TBlockHeaderBytes& vBytes) {
    // 128 = 80(block头数据) + 48(sha256补位数据)
    uint8_t* buf = &vBytes[0];

    // 将后48byte置0
    memset(buf+80, 0, 48);

    // 补位
    *(buf+80) = 0x80; // 数据末尾补 1000 0000

    uint8_t rawDataSize[8];
    WriteBE64(rawDataSize, 80 << 3); // 写入big endian (<<3的是乘以8)
    memcpy(buf + 120, rawDataSize, 8);
}

void BTC_SHA256_GPU::SyncStart( TGetNewBlockHeaderBytesCallback getNewDataCallback,
                                TFoundNewBlockCallback foundNewBlockCallback) {


    if (getNewDataCallback == nullptr || foundNewBlockCallback == nullptr) {
        AppLog::Error("%s, Start, 参数错误.", Info);
        return;
    }

    if (_isStarted) {
        return ;
    }
    _isStarted = true;

    // 获取新块数据callback
    _getNewBlockHeaderBytesCallback = getNewDataCallback;
    // 发现新块的callback
    _fondNewBlockCallback           = foundNewBlockCallback;

    Block::TBlockPtr block;
    uint32_t         nonceOffset = 0;
    arith_uint256    bnTarget;

    auto funcBuildTarget = [&](uint32_t nBits) -> bool {

        auto& powLimit = ParamsBase::Instance()->powLimit;

        bool fNegative;
        bool fOverflow;
        bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

        // Check range
        if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(powLimit))
            return false;

        return true;
    };

    // GPU并行数量
    auto workerCount = _clState.threadCount();

    while(true) {

        switch (_runState) {
            case RESTERT: {
                _runState = RUNING;

                cl_int err = 0;

                // 因为只用到了Nonce挖矿，所以假设一定会挖到，因为Nonce如果超出范围，则程序会直接退出了
                if (block != nullptr) {

                    // 保存新块。目前,此处理目前必须与挖矿同处于一个线程,因为下一个块必须是本次挖出的这个块
                    // 如果是在异步线程中，可能会造成处理新块线程还未完保存完毕，挖矿线程就已经开始了
                    // 此时挖矿线程获取到的链顶端块，就不会是刚刚挖到的块，导致本机直接分叉了.
                    // TODO 可以优化
                    AppLog::Debug("RESET: %d, %s", block->_nNonce, block->GetHash().ToString().c_str());
                    if (_fondNewBlockCallback) {

                        _fondNewBlockCallback(block);
                    }
                }


                // 隔一秒再进行下一次的挖矿,因为在RegNet模式下，挖矿太快会被BitcoinCore强制关闭连接（可能以为是DDOS了)
                std::this_thread::sleep_for(std::chrono::seconds(1));


                // 重新获取块头数据
                TBlockHeaderBytes headerBytes(PADDED_BLOCKHEADER_SIZE);

                // 获取下一个块的header数据
                block.reset();
                _getNewBlockHeaderBytesCallback(block);
                auto header = block->GetBlockHeader();

                // 序列化区块头数据
                NetMessageData s(SER_GETHASH); s << header;
                memcpy(&headerBytes[0], s.Data(), s.DataSize());

                // sha256补位
                PadBlockHaderData(headerBytes);

                // copy host block header bytes to device global memory
                err = clEnqueueWriteBuffer(_clState.commandQueue,
                                           _clState.inputBuffer.buffer,
                                           CL_TRUE,
                                           0,
                                           _clState.inputBuffer.size,
                                           &headerBytes[0],
                                           0,
                                           NULL,
                                           NULL);
                AppLog::CheckError(err, "Error: 写入BlockHeader数据到设备global内存时错误.");

                // 重置nonce参数
                nonceOffset = 0;

                funcBuildTarget(block->_nBits);
                break;
            }
            case STOP:
                AppLog::Debug("%s, GPU STOP", Info);
                return;
        }


        // 设置nonce偏移量
        auto err = clSetKernelArg(_clState.kernel, 2, sizeof(int), (void*)&nonceOffset);
        AppLog::CheckError(err, "Error: clSetKernelArg 3");

        // GPU执行
        err = clEnqueueNDRangeKernel(_clState.commandQueue,
                                     _clState.kernel,
                                     1,
                                     NULL,
                                     (size_t*)&workerCount,
                                     NULL,
                                     0,
                                     NULL,
                                     NULL);
        AppLog::CheckError(err, "Error: clEnqueueNDRangeKernel error");

        // 等待执行完成
        clFinish(_clState.commandQueue);

        // 读取执行结果
        // TODO 优化为内存池
        TResultsPtr result = std::make_shared<TResults>(_clState.outputBuffer.size / HASH_SIZE);
        err = clEnqueueReadBuffer(_clState.commandQueue,
                                  _clState.outputBuffer.buffer,
                                  CL_TRUE,
                                  0,
                                  _clState.outputBuffer.size,
                                  &(*result)[0],
                                  0,
                                  NULL,
                                  NULL);
        AppLog::CheckError(err, "Error: Failed to read output array");
        CheckResults(result, nonceOffset, block, bnTarget);



        // TODO 更严格边界的校验
        auto tmp = nonceOffset + _clState.threadCount();
        if (tmp > std::numeric_limits<uint32_t>::max()) {
            AppLog::CheckError(1, "Error: Nonce 超出范围");
        }
        nonceOffset += _clState.threadCount();
    }
}


void BTC_SHA256_GPU::Reset(){
    _runState = RESTERT;
}

void BTC_SHA256_GPU::Stop() {
    _runState = STOP;
}



namespace TT {
    uint32_t _s[8];

    void Print() {
        printf("TT Hash: ");
        for (auto i : _s) {
            printf("%08x", i);
        }
        printf("\n");
    }


    void inline WriteBE32_EX(unsigned char* ptr, uint32_t x) {
        uint32_t v = ToBE32(x);
        memcpy(ptr, (char*)&v, 4);
    }

    uint32_t static inline ReadBE32_EX(const unsigned char* ptr)
    {
        uint32_t x;
        memcpy((char*)&x, ptr, 4);
        // return be32toh(x);
        return ToBE32(x);
    }

    uint32_t inline Ch(uint32_t x, uint32_t y, uint32_t z) { return z ^ (x & (y ^ z)); }
    uint32_t inline Maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (z & (x | y)); }
    uint32_t inline Sigma0(uint32_t x) { return (x >> 2 | x << 30) ^ (x >> 13 | x << 19) ^ (x >> 22 | x << 10); }
    uint32_t inline Sigma1(uint32_t x) { return (x >> 6 | x << 26) ^ (x >> 11 | x << 21) ^ (x >> 25 | x << 7); }
    uint32_t inline sigma0(uint32_t x) { return (x >> 7 | x << 25) ^ (x >> 18 | x << 14) ^ (x >> 3); }
    uint32_t inline sigma1(uint32_t x) { return (x >> 17 | x << 15) ^ (x >> 19 | x << 13) ^ (x >> 10); }

    void inline Round(
            uint32_t a,
            uint32_t b,
            uint32_t c,
            uint32_t* d,
            uint32_t e,
            uint32_t f,
            uint32_t g,
            uint32_t* h,
            uint32_t k,
            uint32_t w) {

        uint32_t t1 = *h + Sigma1(e) + Ch(e, f, g) + k + w;
        uint32_t t2 = Sigma0(a) + Maj(a, b, c);
        *d += t1;
        *h = t1 + t2;
    }


    void Init() {
        // _sha256 初始hash
        _s[0] = 0x6a09e667ul;
        _s[1] = 0xbb67ae85ul;
        _s[2] = 0x3c6ef372ul;
        _s[3] = 0xa54ff53aul;
        _s[4] = 0x510e527ful;
        _s[5] = 0x9b05688cul;
        _s[6] = 0x1f83d9abul;
        _s[7] = 0x5be0cd19ul;
    }

    static inline void t(uint32_t* s, const uint8_t* chunk, size_t blocks) {
        while(blocks--) {
            uint32_t a = s[0], b = s[1], c = s[2], d = s[3], e = s[4], f = s[5], g = s[6], h = s[7];
            uint32_t w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10, w11, w12, w13, w14, w15;

            Round(a, b, c, &d, e, f, g, &h, 0x428a2f98, w0 = ReadBE32_EX(chunk + 0));
            Round(h, a, b, &c, d, e, f, &g, 0x71374491, w1 = ReadBE32_EX(chunk + 4));
            Round(g, h, a, &b, c, d, e, &f, 0xb5c0fbcf, w2 = ReadBE32_EX(chunk + 8));
            Round(f, g, h, &a, b, c, d, &e, 0xe9b5dba5, w3 = ReadBE32_EX(chunk + 12));
            Round(e, f, g, &h, a, b, c, &d, 0x3956c25b, w4 = ReadBE32_EX(chunk + 16));
            Round(d, e, f, &g, h, a, b, &c, 0x59f111f1, w5 = ReadBE32_EX(chunk + 20));
            Round(c, d, e, &f, g, h, a, &b, 0x923f82a4, w6 = ReadBE32_EX(chunk + 24));
            Round(b, c, d, &e, f, g, h, &a, 0xab1c5ed5, w7 = ReadBE32_EX(chunk + 28));
            Round(a, b, c, &d, e, f, g, &h, 0xd807aa98, w8 = ReadBE32_EX(chunk + 32));
            Round(h, a, b, &c, d, e, f, &g, 0x12835b01, w9 = ReadBE32_EX(chunk + 36));
            Round(g, h, a, &b, c, d, e, &f, 0x243185be, w10 = ReadBE32_EX(chunk + 40));
            Round(f, g, h, &a, b, c, d, &e, 0x550c7dc3, w11 = ReadBE32_EX(chunk + 44));
            Round(e, f, g, &h, a, b, c, &d, 0x72be5d74, w12 = ReadBE32_EX(chunk + 48));
            Round(d, e, f, &g, h, a, b, &c, 0x80deb1fe, w13 = ReadBE32_EX(chunk + 52));
            Round(c, d, e, &f, g, h, a, &b, 0x9bdc06a7, w14 = ReadBE32_EX(chunk + 56));
            Round(b, c, d, &e, f, g, h, &a, 0xc19bf174, w15 = ReadBE32_EX(chunk + 60));

            Round(a, b, c, &d, e, f, g, &h, 0xe49b69c1, w0 += sigma1(w14) + w9 + sigma0(w1));
            Round(h, a, b, &c, d, e, f, &g, 0xefbe4786, w1 += sigma1(w15) + w10 + sigma0(w2));
            Round(g, h, a, &b, c, d, e, &f, 0x0fc19dc6, w2 += sigma1(w0) + w11 + sigma0(w3));
            Round(f, g, h, &a, b, c, d, &e, 0x240ca1cc, w3 += sigma1(w1) + w12 + sigma0(w4));
            Round(e, f, g, &h, a, b, c, &d, 0x2de92c6f, w4 += sigma1(w2) + w13 + sigma0(w5));
            Round(d, e, f, &g, h, a, b, &c, 0x4a7484aa, w5 += sigma1(w3) + w14 + sigma0(w6));
            Round(c, d, e, &f, g, h, a, &b, 0x5cb0a9dc, w6 += sigma1(w4) + w15 + sigma0(w7));
            Round(b, c, d, &e, f, g, h, &a, 0x76f988da, w7 += sigma1(w5) + w0 + sigma0(w8));
            Round(a, b, c, &d, e, f, g, &h, 0x983e5152, w8 += sigma1(w6) + w1 + sigma0(w9));
            Round(h, a, b, &c, d, e, f, &g, 0xa831c66d, w9 += sigma1(w7) + w2 + sigma0(w10));
            Round(g, h, a, &b, c, d, e, &f, 0xb00327c8, w10 += sigma1(w8) + w3 + sigma0(w11));
            Round(f, g, h, &a, b, c, d, &e, 0xbf597fc7, w11 += sigma1(w9) + w4 + sigma0(w12));
            Round(e, f, g, &h, a, b, c, &d, 0xc6e00bf3, w12 += sigma1(w10) + w5 + sigma0(w13));
            Round(d, e, f, &g, h, a, b, &c, 0xd5a79147, w13 += sigma1(w11) + w6 + sigma0(w14));
            Round(c, d, e, &f, g, h, a, &b, 0x06ca6351, w14 += sigma1(w12) + w7 + sigma0(w15));
            Round(b, c, d, &e, f, g, h, &a, 0x14292967, w15 += sigma1(w13) + w8 + sigma0(w0));

            Round(a, b, c, &d, e, f, g, &h, 0x27b70a85, w0 += sigma1(w14) + w9 + sigma0(w1));
            Round(h, a, b, &c, d, e, f, &g, 0x2e1b2138, w1 += sigma1(w15) + w10 + sigma0(w2));
            Round(g, h, a, &b, c, d, e, &f, 0x4d2c6dfc, w2 += sigma1(w0) + w11 + sigma0(w3));
            Round(f, g, h, &a, b, c, d, &e, 0x53380d13, w3 += sigma1(w1) + w12 + sigma0(w4));
            Round(e, f, g, &h, a, b, c, &d, 0x650a7354, w4 += sigma1(w2) + w13 + sigma0(w5));
            Round(d, e, f, &g, h, a, b, &c, 0x766a0abb, w5 += sigma1(w3) + w14 + sigma0(w6));
            Round(c, d, e, &f, g, h, a, &b, 0x81c2c92e, w6 += sigma1(w4) + w15 + sigma0(w7));
            Round(b, c, d, &e, f, g, h, &a, 0x92722c85, w7 += sigma1(w5) + w0 + sigma0(w8));
            Round(a, b, c, &d, e, f, g, &h, 0xa2bfe8a1, w8 += sigma1(w6) + w1 + sigma0(w9));
            Round(h, a, b, &c, d, e, f, &g, 0xa81a664b, w9 += sigma1(w7) + w2 + sigma0(w10));
            Round(g, h, a, &b, c, d, e, &f, 0xc24b8b70, w10 += sigma1(w8) + w3 + sigma0(w11));
            Round(f, g, h, &a, b, c, d, &e, 0xc76c51a3, w11 += sigma1(w9) + w4 + sigma0(w12));
            Round(e, f, g, &h, a, b, c, &d, 0xd192e819, w12 += sigma1(w10) + w5 + sigma0(w13));
            Round(d, e, f, &g, h, a, b, &c, 0xd6990624, w13 += sigma1(w11) + w6 + sigma0(w14));
            Round(c, d, e, &f, g, h, a, &b, 0xf40e3585, w14 += sigma1(w12) + w7 + sigma0(w15));
            Round(b, c, d, &e, f, g, h, &a, 0x106aa070, w15 += sigma1(w13) + w8 + sigma0(w0));

            Round(a, b, c, &d, e, f, g, &h, 0x19a4c116, w0 += sigma1(w14) + w9 + sigma0(w1));
            Round(h, a, b, &c, d, e, f, &g, 0x1e376c08, w1 += sigma1(w15) + w10 + sigma0(w2));
            Round(g, h, a, &b, c, d, e, &f, 0x2748774c, w2 += sigma1(w0) + w11 + sigma0(w3));
            Round(f, g, h, &a, b, c, d, &e, 0x34b0bcb5, w3 += sigma1(w1) + w12 + sigma0(w4));
            Round(e, f, g, &h, a, b, c, &d, 0x391c0cb3, w4 += sigma1(w2) + w13 + sigma0(w5));
            Round(d, e, f, &g, h, a, b, &c, 0x4ed8aa4a, w5 += sigma1(w3) + w14 + sigma0(w6));
            Round(c, d, e, &f, g, h, a, &b, 0x5b9cca4f, w6 += sigma1(w4) + w15 + sigma0(w7));
            Round(b, c, d, &e, f, g, h, &a, 0x682e6ff3, w7 += sigma1(w5) + w0 + sigma0(w8));
            Round(a, b, c, &d, e, f, g, &h, 0x748f82ee, w8 += sigma1(w6) + w1 + sigma0(w9));
            Round(h, a, b, &c, d, e, f, &g, 0x78a5636f, w9 += sigma1(w7) + w2 + sigma0(w10));
            Round(g, h, a, &b, c, d, e, &f, 0x84c87814, w10 += sigma1(w8) + w3 + sigma0(w11));
            Round(f, g, h, &a, b, c, d, &e, 0x8cc70208, w11 += sigma1(w9) + w4 + sigma0(w12));
            Round(e, f, g, &h, a, b, c, &d, 0x90befffa, w12 += sigma1(w10) + w5 + sigma0(w13));
            Round(d, e, f, &g, h, a, b, &c, 0xa4506ceb, w13 += sigma1(w11) + w6 + sigma0(w14));
            Round(c, d, e, &f, g, h, a, &b, 0xbef9a3f7, w14 + sigma1(w12) + w7 + sigma0(w15));
            Round(b, c, d, &e, f, g, h, &a, 0xc67178f2, w15 + sigma1(w13) + w8 + sigma0(w0));

            s[0] += a;
            s[1] += b;
            s[2] += c;
            s[3] += d;
            s[4] += e;
            s[5] += f;
            s[6] += g;
            s[7] += h;
            chunk += 64;
        }
    }

    void HashTest(uint8_t* chunk, unsigned char* outHash) {

        // 第一轮
        Init();
        t(_s, chunk, 2);

        // 第二轮

        // 写入 Big ending hashBlock 结果
        uint8_t tmpResult[64];
        memset(tmpResult, 0, 64);
        WriteBE32_EX(tmpResult, _s[0]);
        WriteBE32_EX(tmpResult + 4, _s[1]);
        WriteBE32_EX(tmpResult + 8, _s[2]);
        WriteBE32_EX(tmpResult + 12, _s[3]);
        WriteBE32_EX(tmpResult + 16, _s[4]);
        WriteBE32_EX(tmpResult + 20, _s[5]);
        WriteBE32_EX(tmpResult + 24, _s[6]);
        WriteBE32_EX(tmpResult + 28, _s[7]);


        // 因为是双sha256，则此处是对第一轮产生的hash进行补位，为了进行下一轮的sha256计算
        tmpResult[32] = 0x80;
        uint8_t rawDataSize[8];
        WriteBE64(rawDataSize, 32 << 3); // 写入big endian (<<3的是乘以8)
        memcpy(tmpResult + 56, rawDataSize, 8);

        Init();
        t(_s, tmpResult, 1);

        WriteBE32_EX(outHash, _s[0]);
        WriteBE32_EX(outHash + 4, _s[1]);
        WriteBE32_EX(outHash + 8, _s[2]);
        WriteBE32_EX(outHash + 12, _s[3]);
        WriteBE32_EX(outHash + 16, _s[4]);
        WriteBE32_EX(outHash + 20, _s[5]);
        WriteBE32_EX(outHash + 24, _s[6]);
        WriteBE32_EX(outHash + 28, _s[7]);
    }
}






void BTC_SHA256_GPU::CPUPperformance() {
    auto block = Block::GetGenesisBlock();
    auto header = block.GetBlockHeader();

    TBlockHeaderBytes vData(PADDED_BLOCKHEADER_SIZE);
    NetMessageData s(SER_GETHASH); s << header;
    memcpy(&vData[0], s.Data(), s.DataSize());
    PadBlockHaderData(vData);


    uint256 hash;
     int c = 1000 * 40 * 512; // 20,480,000
//    int c = 4;
    int NONCE_OFFSET = 76;

    TimeUtils u;

    for (int i = 0; i < c; ++i) {
        memcpy(&vData[0] + NONCE_OFFSET, &i, 4);
        TT::HashTest(&vData[0], hash.begin());
//        AppLog::Debug("HASH: %s", hash.ToString().c_str());
    }


    u.End("CPU: ", c);
}

void BTC_SHA256_GPU::GPUPperformance() {
    TimeUtils u;

    int c = 1000 * 40 * 512;
    u.End("GPU: ", c);

}
