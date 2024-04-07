//
// Created by fly on 2020/6/12.
//

#ifndef BITCOIN_GPU_MINER_SHA256_CPU_H
#define BITCOIN_GPU_MINER_SHA256_CPU_H

#include <cstddef>
#include <string>
#include "uint256.h"

namespace sha256 {
    class SHA256_CPU {

    private:
        uint32_t s[8];
        unsigned char buf[64];
        uint64_t bytes;


    public:
        static const size_t OUTPUT_SIZE = 32;

        SHA256_CPU();

        void SelfTest();

        SHA256_CPU &Reset();

        SHA256_CPU &Write(const unsigned char *data, size_t dataSize);

        void Finalize(unsigned char outHash[OUTPUT_SIZE]);
    };


}

#endif //BITCOIN_GPU_MINER_SHA256_CPU_H
