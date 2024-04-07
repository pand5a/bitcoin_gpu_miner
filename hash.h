//
// Created by fly on 2020/8/15.
//

#ifndef BITCOIN_GPU_MINER_HASH_H
#define BITCOIN_GPU_MINER_HASH_H
#include "common.h"
#include "sha256_cpu.h"
#include "uint256.h"
#include "ripemd160.h"
#include "prevector.h"
#include "serialize.h"
#include "version.h"

#include <vector>

using namespace sha256;
namespace Hash {

    typedef uint256 ChainCode;

    // From Bitcoin core sources
    /** SipHash-2-4 */
    class SipHasher {
    private:
        uint64_t v[4];
        uint64_t tmp;
        int count;

    public:
        /** Construct a SipHash calculator initialized with 128-bit key (k0, k1) */
        SipHasher(uint64_t k0, uint64_t k1);

        /** Hash a 64-bit integer worth of data
         *  It is treated as if this was the little-endian interpretation of 8 bytes.
         *  This function can only be used when a multiple of 8 bytes have been written so far.
         */
        SipHasher &Write(uint64_t data);

        /** Hash arbitrary bytes. */
        SipHasher &Write(const unsigned char *data, size_t size);

        /** Compute the 64-bit SipHash-2-4 of the data written so far. The object remains untouched. */
        uint64_t Finalize() const;
    };


    /** A hasher class for Bitcoin's 256-bit hashBlock (double SHA-256). */
    class DoubleHash256 {
    private:
        SHA256_CPU sha;
    public:
        static const size_t OUTPUT_SIZE = SHA256_CPU::OUTPUT_SIZE;

        void Finalize(uint8_t output[OUTPUT_SIZE]) {
            unsigned char buf[SHA256_CPU::OUTPUT_SIZE];
            sha.Finalize(buf);
            sha.Reset().Write(buf, SHA256_CPU::OUTPUT_SIZE).Finalize(output);
        }

        DoubleHash256& Write(const uint8_t* input, size_t size) {
            sha.Write(input, size);
            return *this;
        }

        DoubleHash256& Reset() {
            sha.Reset();
            return *this;
        }
    };

    /** Compute the 256-bit hashBlock of an object. */
    template<typename T1>
    inline uint256 Hash256(const T1 pbegin, const T1 pend)
    {
        static const unsigned char pblank[1] = {};
        uint256 result;
        DoubleHash256().Write(pbegin == pend ? pblank : (const unsigned char*)&pbegin[0], (pend - pbegin) * sizeof(pbegin[0]))
                .Finalize((unsigned char*)&result);
        return result;
    }



/** A hasher class for Bitcoin's 160-bit hashBlock (SHA-256 + RIPEMD-160). */
    class CHash160 {
    private:
        SHA256_CPU sha;
    public:
        static const size_t OUTPUT_SIZE = CRIPEMD160::OUTPUT_SIZE;

        void Finalize(unsigned char hash[OUTPUT_SIZE]) {
            unsigned char buf[SHA256_CPU::OUTPUT_SIZE];
            sha.Finalize(buf);
            CRIPEMD160().Write(buf, SHA256_CPU::OUTPUT_SIZE).Finalize(hash);
        }

        CHash160& Write(const unsigned char *data, size_t len) {
            sha.Write(data, len);
            return *this;
        }

        CHash160& Reset() {
            sha.Reset();
            return *this;
        }
    };

/** Compute the 256-bit hashBlock of an object. */
    template<typename T1>
    inline uint256 Hash(const T1 pbegin, const T1 pend)
    {
        static const unsigned char pblank[1] = {};
        uint256 result;
        SHA256_CPU().Write(pbegin == pend ? pblank : (const unsigned char*)&pbegin[0], (pend - pbegin) * sizeof(pbegin[0]))
                .Finalize((unsigned char*)&result);
        return result;
    }

/** Compute the 256-bit hashBlock of the concatenation of two objects. */
    template<typename T1, typename T2>
    inline uint256 Hash(const T1 p1begin, const T1 p1end,
                        const T2 p2begin, const T2 p2end) {
        static const unsigned char pblank[1] = {};
        uint256 result;
        SHA256_CPU().Write(p1begin == p1end ? pblank : (const unsigned char*)&p1begin[0], (p1end - p1begin) * sizeof(p1begin[0]))
                .Write(p2begin == p2end ? pblank : (const unsigned char*)&p2begin[0], (p2end - p2begin) * sizeof(p2begin[0]))
                .Finalize((unsigned char*)&result);
        return result;
    }

/** Compute the 160-bit hashBlock an object. */
    template<typename T1>
    inline uint160 Hash160(const T1 pbegin, const T1 pend)
    {
        static unsigned char pblank[1] = {};
        uint160 result;
        CHash160().Write(pbegin == pend ? pblank : (const unsigned char*)&pbegin[0], (pend - pbegin) * sizeof(pbegin[0]))
                .Finalize((unsigned char*)&result);
        return result;
    }

/** Compute the 160-bit hashBlock of a vector. */
    inline uint160 Hash160(const std::vector<unsigned char>& vch)
    {
        return Hash160(vch.begin(), vch.end());
    }

/** Compute the 160-bit hashBlock of a vector. */
    template<unsigned int N>
    inline uint160 Hash160(const prevector<N, unsigned char>& vch)
    {
        return Hash160(vch.begin(), vch.end());
    }


    void BIP32Hash(const ChainCode &chainCode, unsigned int nChild, unsigned char header, const unsigned char data[32], unsigned char output[64]);



/** A writer stream (for serialization) that computes a 256-bit hashBlock. */
    class CHashWriter
    {
//        std::vector<byte> _buf;
    private:
        DoubleHash256 ctx;

        const int nType;
        const int nVersion;
    public:

        CHashWriter(int nTypeIn, int nVersionIn) : nType(nTypeIn), nVersion(nVersionIn) {}

        int GetType() const { return nType; }
        int GetVersion() const { return nVersion; }

        void write(const char *pch, size_t size) {
//            _buf.push_back(pch, pch+size);
//            _buf.insert(_buf.end(), pch, pch+size);
            ctx.Write((const unsigned char*)pch, size);
        }

        void read(byte* pBuf, size_t size) {
            // 占位
        }

        // invalidates the object
        uint256 GetHash() {

//            printf("\n");
//            printf("CHashWriter Data: --------------------------------------\n");
//            for(auto i : _buf) {
//                printf("%02x", i);
//            }

            uint256 result;
            ctx.Finalize((unsigned char*)&result);
            return result;
        }

        /**
         * Returns the first 64 bits from the resulting hashBlock.
         */
        inline uint64_t GetCheapHash() {
            unsigned char result[DoubleHash256::OUTPUT_SIZE];
            ctx.Finalize(result);
            return ReadLE64(result);
        }

        template<typename T>
        CHashWriter& operator<<(const T& obj) {
            // Serialize to this stream
            ::Serialize(*this, obj);
            return (*this);
        }

        // 占位 for seriallize
        template<typename T>
        CHashWriter& operator >> (const T& obj) {
            // Serialize to this stream
            // ::Serialize(*this, obj);
            return (*this);
        }
    };

/** Reads data from an underlying stream, while hashing the read data. */
    template<typename Source>
    class CHashVerifier : public CHashWriter
    {
    private:
        Source* source;

    public:
        explicit CHashVerifier(Source* source_) : CHashWriter(source_->GetType(), source_->GetVersion()), source(source_) {}

        void read(char* pch, size_t nSize)
        {
            source->read(pch, nSize);
            this->write(pch, nSize);
        }

        void ignore(size_t nSize)
        {
            char data[1024];
            while (nSize > 0) {
                size_t now = std::min<size_t>(nSize, 1024);
                read(data, now);
                nSize -= now;
            }
        }

        template<typename T>
        CHashVerifier<Source>& operator>>(T&& obj)
        {
            // Unserialize from this stream
            ::Unserialize(*this, obj);
            return (*this);
        }

        // 占位 for seriallize
        template<typename T>
        CHashVerifier<Source>& operator<<(T&& obj)
        {
            // Unserialize from this stream
            // ::Unserialize(*this, obj);
            return (*this);
        }
    };

/** Compute the 256-bit hashBlock of an object's serialization. */
    template<typename T>
    uint256 SerializeHash(const T& obj, int nType=SER_GETHASH, int nVersion=PROTOCOL_VERSION)
    {
        CHashWriter ss(nType, nVersion);
        ss << obj;


        return ss.GetHash();
    }

    unsigned int MurmurHash3(unsigned int nHashSeed, const std::vector<unsigned char>& vDataToHash);

}

#endif //BITCOIN_GPU_MINER_HASH_H
