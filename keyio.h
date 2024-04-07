//
// Created by fly on 2020/11/4.
//

#ifndef BITCOIN_GPU_MINER_KEYIO_H
#define BITCOIN_GPU_MINER_KEYIO_H
#include "common.h"
#include "script/standard.h"
#include "params.h"



class Keyio {


public:
    DEF_CLASS_SINGLETON(Keyio);

    CTxDestination DecodeDestination(const std::string& str);

};


#endif //BITCOIN_GPU_MINER_KEYIO_H
