//
// Created by fly on 2020/10/21.
//

#include "params.h"
//ParamsBase s_paramsInstance;
ParamsBase* ParamsBase::Instance() {
    static auto i = RegTestParams();
    return &i;
}

//void Initialize() {
//    s_paramsInstance = RegTestParams();
//}

