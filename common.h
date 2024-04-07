// Copyright (c) 2014-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CRYPTO_COMMON_H
#define BITCOIN_CRYPTO_COMMON_H

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <stdint.h>
#include <string.h>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>
#include <iostream>

#include "endian.h"

#define X_STR_SWITCH(left_c) const std::string& __x__condition = left_c;
#define X_CASE(right_c) if (__x__condition == right_c)

#ifdef APPLE
// Define C99 equivalent types.
typedef signed char           int8_t;
typedef signed short          int16_t;
typedef signed int            int32_t;
typedef signed long long      int64_t;
typedef unsigned char         uint8_t;
typedef unsigned short        uint16_t;
typedef unsigned int          uint32_t;
typedef unsigned long long    uint64_t;
#endif

// Define c++11
using byte = char;


uint16_t static inline ReadLE16(const unsigned char* ptr)
{
    uint16_t x;
    memcpy((char*)&x, ptr, 2);
    return le16toh(x);
}

uint32_t static inline ReadLE32(const unsigned char* ptr)
{
    uint32_t x;
    memcpy((char*)&x, ptr, 4);
    return le32toh(x);
}

uint64_t static inline ReadLE64(const unsigned char* ptr)
{
    uint64_t x;
    memcpy((char*)&x, ptr, 8);
    return le64toh(x);
}

void static inline WriteLE16(unsigned char* ptr, uint16_t x)
{
    uint16_t v = htole16(x);
    memcpy(ptr, (char*)&v, 2);
}

void static inline WriteLE32(unsigned char* ptr, uint32_t x)
{
    uint32_t v = htole32(x);
    memcpy(ptr, (char*)&v, 4);
}

void static inline WriteLE64(unsigned char* ptr, uint64_t x)
{
    uint64_t v = htole64(x);
    memcpy(ptr, (char*)&v, 8);
}

uint32_t static inline ReadBE32(const unsigned char* ptr)
{
    uint32_t x;
    memcpy((char*)&x, ptr, 4);
    return be32toh(x);
}

uint64_t static inline ReadBE64(const unsigned char* ptr)
{
    uint64_t x;
    memcpy((char*)&x, ptr, 8);
    return be64toh(x);
}

void static inline WriteBE32(unsigned char* ptr, uint32_t x)
{
    uint32_t v = htobe32(x);
    memcpy(ptr, (char*)&v, 4);
}

void static inline WriteBE64(unsigned char* ptr, uint64_t x)
{
    uint64_t v = htobe64(x);
    memcpy(ptr, (char*)&v, 8);
}

/** Return the smallest number n such that (x >> n) == 0 (or 64 if the highest bit in x is set. */
uint64_t static inline CountBits(uint64_t x)
{
#if HAVE_DECL___BUILTIN_CLZL
    if (sizeof(unsigned long) >= sizeof(uint64_t)) {
        return x ? 8 * sizeof(unsigned long) - __builtin_clzl(x) : 0;
    }
#endif
#if HAVE_DECL___BUILTIN_CLZLL
    if (sizeof(unsigned long long) >= sizeof(uint64_t)) {
        return x ? 8 * sizeof(unsigned long long) - __builtin_clzll(x) : 0;
    }
#endif
    int ret = 0;
    while (x) {
        x >>= 1;
        ++ret;
    }
    return ret;
}

#define GPU_MINER_LOCK(_mutex) std::lock_guard<std::mutex> \
        guard(_mutex)

#define DEF_CLASS_SINGLETON(class_x) static class_x* Instance()

#define IMP_CLASS_SINGLETON(class_x) \
    class_x* class_x::Instance() { \
        static class_x instance; \
        return &instance; \
    }

static inline const std::string CurrentTime() {
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::stringstream ss;
    ss << std::put_time(std::localtime(&t), "%F %T");
    return ss.str();
}

static inline unsigned long GetThreadId()
{
    std::string threadId=boost::lexical_cast<std::string>(boost::this_thread::get_id());
    unsigned long  threadNumber=0;
    threadNumber =std::stoul(threadId,nullptr,16);
    return threadNumber;
}

template<typename ... Args>
static std::string str_format(const std::string &format, Args ... args) {


    // auto size_buf = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1;
    // std::unique_ptr<char[]> buf(new(std::nothrow) char[size_buf]);
    char buf[4096] = {0};
    std::snprintf(buf, 4096, format.c_str(), args ...);
    return std::string(&buf[0], buf + strlen(buf));
}

//std::string FormatMoney(const CAmount& n) {
//    // Note: not using straight sprintf here because we do NOT want
//    // localized number formatting.
//    int64_t n_abs = (n > 0 ? n : -n);
//    int64_t quotient = n_abs/COIN;
//    int64_t remainder = n_abs%COIN;
//    std::string str = strprintf("%d.%08d", quotient, remainder);
//
//    // Right-trim excess zeros before the decimal point:
//    int nTrim = 0;
//    for (int i = str.size()-1; (str[i] == '0' && IsDigit(str[i-2])); --i)
//        ++nTrim;
//    if (nTrim)
//        str.erase(str.size()-nTrim, nTrim);
//
//    if (n < 0)
//        str.insert((unsigned int)0, 1, '-');
//    return str;
//}


/**
 * 学习测试用
 */
/*
namespace XX_Serialize {



    struct SerActionSerialize {};
    struct SerActionUnserialize {};

    #define READWRITE(field)      (::SerReadWrite(s, serAction, field))

    #define ADD_SERIALIZE_METHODS                                         \
        template<typename Stream>                                             \
        void Serialize(Stream& s) {                                       \
            SerializationOp(s, CSerActionSerialize());                    \
        }                                                                 \
        template<typename Stream>                                         \
        void Unserialize(Stream& s) {                                     \
            SerializationOp(s, CSerActionUnserialize());                  \
        }

        template<typename Stream, typename T>
        void SerReadWrite(Stream& s, SerActionSerialize serAction, const T& field) {
        }

        template<typename Stream, typename T>
        void SerReadWrite(Stream& s, SerActionUnserialize serAction, T& field) {
        }


    // 整数类型的序列化/反序列化
    template<typename Stream, typename T, std::enable_if_t<std::is_integral<T>::value, int> = 0>
    void Serialize(Stream& s, const T& field) {
        std::cout << "type: " << typeid(field).name() << ", sizeof: " << sizeof(field) << std::endl;
    }

    template<typename Stream, typename T, std::enable_if_t<std::is_integral<T>::value, int> = 0>
    void Unserialize(Stream& s, T& field) {
        std::cout << "type: " << typeid(field).name() << ", sizeof: " << sizeof(field) << std::endl;
    }


    // Byte 数组的序列化/反序列化
    template<typename Stream, int N>
    void Serialize(Stream& s, byte (&a)[N]) {
        std::cout << "type: " << typeid(a).name() << ", sizeof: " << N << std::endl;
    }

    template<typename Stream, int N>
    void Unserialize(Stream& s, byte (&a)[N]) {
        std::cout << "type: " << typeid(a).name() << ", sizeof: " << N << std::endl;
        memcpy(s, a, N);
    }
}*/ // end namespace XX_Serialize

#endif // BITCOIN_CRYPTO_COMMON_H
