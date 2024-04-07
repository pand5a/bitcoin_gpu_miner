//
// Created by fly on 2020/10/10.
//

#include "tx.h"
#include "blockchain.h"

CAmount NetMessageTransaction::GetFees() const {
    CAmount inValue = 0;
    DBTransactionIndexWrapper::TxIdx idx;

    for(const auto& in : _vin) {
        if (!DBTransactionIndexWrapper::Instance()->Get(in._previousOutput._hashTransaction, idx)) {
            return 0;
        }

        if (in._previousOutput._outPutIdx >=  idx.tx._vout.size()) {
            return 0;
        }

        inValue += idx.tx._vout[in._previousOutput._outPutIdx]._nValue;
    }

    CAmount outValue = GetValueOut();
    CAmount ret = inValue - outValue;

    return ret > 0 ? ret : 0;
}