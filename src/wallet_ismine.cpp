// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2016-2017 The PIVX developers
// Copyright (c) 2019 The DreamTeam3 developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet_ismine.h"

#include "key.h"
#include "keystore.h"
#include "script/script.h"
#include "script/standard.h"
#include "util.h"

#include <boost/foreach.hpp>

using namespace std;

typedef vector<unsigned char> valtype;

unsigned int HaveKeys(const vector<valtype>& pubkeys, const CKeyStore& keystore)
{
    unsigned int nResult = 0;
    BOOST_FOREACH (const valtype& pubkey, pubkeys) {
        CKeyID keyID = CPubKey(pubkey).GetID();
        if(keystore.HaveKey(keyID))
            ++nResult;
    }
    return nResult;
}

isminetype IsMine(const CKeyStore& keystore, const CTxDestination& dest)
{
    CScript script = GetScriptForDestination(dest);
    return IsMine(keystore, script);
}

isminetype IsMine(const CKeyStore& keystore, const CScript& scriptPubKey)
{
    if(keystore.HaveWatchOnly(scriptPubKey))
        return ISMINE_WATCH_ONLY;
    if(keystore.HaveMultiSig(scriptPubKey))
        return ISMINE_MULTISIG;

    vector<valtype> vSolutions;
    txnouttype whichType;
    if(!Solver(scriptPubKey, whichType, vSolutions)) {
        if(keystore.HaveWatchOnly(scriptPubKey))
            return ISMINE_WATCH_ONLY;
        if(keystore.HaveMultiSig(scriptPubKey))
            return ISMINE_MULTISIG;

        return ISMINE_NO;
    }

    CKeyID keyID;
    switch (whichType) {
    case TX_NONSTANDARD:
    case TX_NULL_DATA:
        break;
    case TX_ZEROCOINMINT:
    case TX_PUBKEY:
        keyID = CPubKey(vSolutions[0]).GetID();
        if(keystore.HaveKey(keyID))
            return ISMINE_SPENDABLE;
        break;
    case TX_PUBKEYHASH:
        keyID = CKeyID(uint160(vSolutions[0]));
        if(keystore.HaveKey(keyID))
            return ISMINE_SPENDABLE;
        break;
    case TX_SCRIPTHASH: {
        CScriptID scriptID = CScriptID(uint160(vSolutions[0]));
        CScript subscript;
        if(keystore.GetCScript(scriptID, subscript)) {
            isminetype ret = IsMine(keystore, subscript);
            if(ret != ISMINE_NO)
                return ret;
        }
        break;
    }
    case TX_MULTISIG: {
        // Only consider transactions "mine" if we own ALL the
        // keys involved. multi-signature transactions that are
        // partially owned (somebody else has a key that can spend
        // them) enable spend-out-from-under-you attacks, especially
        // in shared-wallet situations.
        vector<valtype> keys(vSolutions.begin() + 1, vSolutions.begin() + vSolutions.size() - 1);
        if(HaveKeys(keys, keystore) == keys.size())
            return ISMINE_SPENDABLE;
        break;
    }
    }

    if(keystore.HaveWatchOnly(scriptPubKey))
        return ISMINE_WATCH_ONLY;
    if(keystore.HaveMultiSig(scriptPubKey))
        return ISMINE_MULTISIG;

    return ISMINE_NO;
}
