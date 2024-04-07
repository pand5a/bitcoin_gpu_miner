/**
 * for(int i = 0 ; i < 8; ++i) {
        threadPool.Enqueue([i] {
            {
               std::cout << "Hello" << "(" << i << ")" << std::endl;
            }

        });
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));
 */

//
// Created by fly on 2020/7/11.
//
// 来源: https://segmentfault.com/a/1190000006691692?utm_source=tag-newest
// 另外一个实现: https://blog.csdn.net/qq_41105995/article/details/105986999

#ifndef BITCOIN_GPU_MINER_THREAD_POOL_H
#define BITCOIN_GPU_MINER_THREAD_POOL_H

#include <functional>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include <boost/asio.hpp>

class ThreadPool {
public:
    explicit ThreadPool(std::size_t size)
    :  _work_guard(boost::asio::make_work_guard(_ioc)) {

        _workers.reserve(size);

        for (std::size_t i = 0; i < size; ++i) {
            _workers.emplace_back(&boost::asio::io_context::run , &_ioc);
        }
    }

    ~ThreadPool() {
        _ioc.stop();
        for( auto& v : _workers) {
            v.join();
        }

    }

    template<class F>
    void Enqueue(F f) {
        boost::asio::post(_ioc, f);
    }

private:
    std::vector<std::thread>    _workers;
    boost::asio::io_context     _ioc;


    typedef boost::asio::io_context::executor_type ExecutorType;
    boost::asio::executor_work_guard<ExecutorType> _work_guard;
};


#endif //BITCOIN_GPU_MINER_THREAD_POOL_H
