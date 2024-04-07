cmake_minimum_required(VERSION 3.16)
project(bitcoin_gpu_miner)

set(CMAKE_CXX_COMPILER "g++")
set(CMAKE_CXX_STANDARD 14)

IF (WIN32)
    MESSAGE(STATUS "Now is windows")
ELSEIF (APPLE)
    MESSAGE(STATUS "Now is Apple")
    add_definitions(-D APPLE)

    # 设置链接参数
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework OpenCL")

    # 拷贝cl文件,到binary目录内
    configure_file (
            ${CMAKE_CURRENT_SOURCE_DIR}/btc_sha256_gpu.cl
            ${CMAKE_BINARY_DIR}/btc_sha256_gpu.cl
            COPYONLY
    )

    include_directories(/usr/local/include)
    include_directories(/usr/local/opt/openssl/include)

    link_directories(/usr/local/opt/openssl/lib)
    link_directories(${CMAKE_CURRENT_SOURCE_DIR}/leveldb/build/macos)

ELSEIF (UNIX)
    MESSAGE(STATUS "Now is UNIX-like OS's.")
    link_directories(/usr/lib/x86_64-linux-gnu)
    link_directories(${CMAKE_CURRENT_SOURCE_DIR}/leveldb/build/linux)
ENDIF ()

add_definitions(-D BOOST_ASIO_NO_DEPRECATED)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/secp256k1/.libs)

# leveldb
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/leveldb/include)

#link_libraries(libsecp256k1.a)
add_executable(
        bitcoin_gpu_miner
        main.cpp
        sha256_cpu.cpp
        sha256_cpu.h
        arith_uint256.cpp
        arith_uint256.h
        uint256.cpp
        uint256.h
        common.h
        utilstrencodings.cpp
        utilstrencodings.h
        tinyformat.h
        endian.h
        byteswap.h
#        btc_sha256.cpp
#        btc_sha256.h
        ease_log.cpp
        ease_log.h
        time_utils.cpp
        time_utils.h
        btc_sha256_gpu.cpp
        btc_sha256_gpu.h
        cmdline.h
        net.cpp
        net.h
        queue.h
        thread_pool.h
        peer_logic_man.cpp
        peer_logic_man.h
        serialize.h
        prevector.h
        block.h
        block.cpp
        hash.h
        hash.cpp
        script/script.h
        script/script.cpp
        ripemd160.h
        ripemd160.cpp
        pubkey.h
        pubkey.cpp
        sha512.h
        sha512.cpp
        hmac_sha512.h
        hmac_sha512.cpp
        key.h
        key.cpp
        chacha20.h
        chacha20.cpp
        random.h
        random.cpp
        cleanse.h
        cleanse.cpp
        script/interpreter.h
        script/interpreter.cpp
        script/sigcache.h
        script/sigcache.cpp
        script/standard.h
        script/standard.cpp
        sha1.h
        sha1.cpp
        blockchain.cpp
        blockchain.h
        tx.cpp
        tx.h
        params.cpp
        params.h
        keyio.cpp
        keyio.h)

set_target_properties(bitcoin_gpu_miner PROPERTIES
        COMPILE_DEFINITIONS "DB_BASE_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}\""
        )

IF (APPLE)
    target_link_libraries(bitcoin_gpu_miner crypto ssl leveldb secp256k1 -rdynamic -Wl,-headerpad_max_install_names -Wl,-dead_strip -Wl,-dead_strip_dylibs)
ELSEIF (UNIX)
    target_link_libraries(bitcoin_gpu_miner crypto ssl boost_system boost_thread pthread leveldb libsecp256k1.a)
ENDIF ()

# 测试程序
#add_executable(
#        bitcoin_gpu_miner_test
#        main_test.cpp
#        sha256_cpu.cpp
#        sha256_cpu.h
#        arith_uint256.cpp
#        arith_uint256.h
#        uint256.cpp
#        uint256.h
#        common.h
#        utilstrencodings.cpp
#        utilstrencodings.h
#        tinyformat.h
#        endian.h
#        byteswap.h
#        btc_sha256.cpp
#        btc_sha256.h
#        ease_log.cpp
#        ease_log.h
#        time_utils.cpp
#        time_utils.h
#        btc_sha256_gpu.cpp
#        btc_sha256_gpu.h
#        cmdline.h
#        net.cpp
#        net.h
#        queue.h
#        thread_pool.h
#        peer_logic_man.cpp
#        peer_logic_man.h
#        serialize.h
#        prevector.h
#        block.h
#        block.cpp
#        hashBlock.h
#        hashBlock.cpp
#)
#
#add_executable(client test/client.cpp)
#add_executable(server test/server.cpp)
