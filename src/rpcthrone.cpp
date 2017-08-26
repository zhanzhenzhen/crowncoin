// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "db.h"
#include "init.h"
#include "activethrone.h"
#include "throneman.h"
#include "throne-payments.h"
#include "throne-budget.h"
#include "throneconfig.h"
#include "rpcserver.h"
#include "utilmoneystr.h"

#include <fstream>
using namespace json_spirit;

void SendMoney(const CTxDestination &address, CAmount nValue, CWalletTx& wtxNew, AvailableCoinsType coin_type=ALL_COINS)
{
    // Check amount
    if (nValue <= 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid amount");

    if (nValue > pwalletMain->GetBalance())
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds");

    string strError;
    if (pwalletMain->IsLocked())
    {
        strError = "Error: Wallet locked, unable to create transaction!";
        LogPrintf("SendMoney() : %s", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    // Parse Crown address
    CScript scriptPubKey = GetScriptForDestination(address);

    // Create and send the transaction
    CReserveKey reservekey(pwalletMain);
    CAmount nFeeRequired;
    if (!pwalletMain->CreateTransaction(scriptPubKey, nValue, wtxNew, reservekey, nFeeRequired, strError, NULL, coin_type))
    {
        if (nValue + nFeeRequired > pwalletMain->GetBalance())
            strError = strprintf("Error: This transaction requires a transaction fee of at least %s because of its amount, complexity, or use of recently received funds!", FormatMoney(nFeeRequired));
        LogPrintf("SendMoney() : %s\n", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    if (!pwalletMain->CommitTransaction(wtxNew, reservekey))
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: The transaction was rejected! This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");
}


Value getpoolinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getpoolinfo\n"
            "Returns an object containing anonymous pool-related information.");

    Object obj;
    obj.push_back(Pair("current_throne",        mnodeman.GetCurrentThroNe()->addr.ToString()));
    return obj;
}


Value throne(const Array& params, bool fHelp)
{
    string strCommand;
    if (params.size() >= 1)
        strCommand = params[0].get_str();

    if (fHelp  ||
        (strCommand != "start" && strCommand != "start-alias" && strCommand != "start-many" && strCommand != "start-all" && strCommand != "start-missing" &&
         strCommand != "start-disabled" && strCommand != "list" && strCommand != "list-conf" && strCommand != "count"  && strCommand != "enforce" &&
        strCommand != "debug" && strCommand != "current" && strCommand != "winners" && strCommand != "genkey" && strCommand != "connect" &&
        strCommand != "outputs" && strCommand != "status" && strCommand != "calcscore"))
        throw runtime_error(
                "throne \"command\"... ( \"passphrase\" )\n"
                "Set of commands to execute throne related actions\n"
                "\nArguments:\n"
                "1. \"command\"        (string or set of strings, required) The command to execute\n"
                "2. \"passphrase\"     (string, optional) The wallet passphrase\n"
                "\nAvailable commands:\n"
                "  count        - Print number of all known thrones (optional: 'ds', 'enabled', 'all', 'qualify')\n"
                "  current      - Print info on current throne winner\n"
                "  debug        - Print throne status\n"
                "  genkey       - Generate new throneprivkey\n"
                "  enforce      - Enforce throne payments\n"
                "  outputs      - Print throne compatible outputs\n"
                "  start        - Start throne configured in crown.conf\n"
                "  start-alias  - Start single throne by assigned alias configured in throne.conf\n"
                "  start-<mode> - Start thrones configured in throne.conf (<mode>: 'all', 'missing', 'disabled')\n"
                "  status       - Print throne status information\n"
                "  list         - Print list of all known thrones (see thronelist for more info)\n"
                "  list-conf    - Print throne.conf in JSON format\n"
                "  winners      - Print list of throne winners\n"
                );

    if (strCommand == "list")
    {
        Array newParams(params.size() - 1);
        std::copy(params.begin() + 1, params.end(), newParams.begin());
        return thronelist(newParams, fHelp);
    }

    if (strCommand == "budget")
    {
        return "Show budgets";
    }

    if(strCommand == "connect")
    {
        std::string strAddress = "";
        if (params.size() == 2){
            strAddress = params[1].get_str();
        } else {
            throw runtime_error("Throne address required\n");
        }

        CService addr = CService(strAddress);

        CNode *pnode = ConnectNode((CAddress)addr, NULL, false);
        if(pnode){
            pnode->Release();
            return "successfully connected";
        } else {
            throw runtime_error("error connecting\n");
        }
    }

    if (strCommand == "count")
    {
        if (params.size() > 2){
            throw runtime_error("too many parameters\n");
        }
        if (params.size() == 2)
        {
            int nCount = 0;

            if(chainActive.Tip())
                mnodeman.GetNextThroneInQueueForPayment(chainActive.Tip()->nHeight, true, nCount);

            if(params[1] == "ds") return mnodeman.CountEnabled(MIN_POOL_PEER_PROTO_VERSION);
            if(params[1] == "enabled") return mnodeman.CountEnabled();
            if(params[1] == "qualify") return nCount;
            if(params[1] == "all") return strprintf("Total: %d (DS Compatible: %d / Enabled: %d / Qualify: %d)",
                                                    mnodeman.size(),
                                                    mnodeman.CountEnabled(MIN_POOL_PEER_PROTO_VERSION),
                                                    mnodeman.CountEnabled(),
                                                    nCount);
        }
        return mnodeman.size();
    }

    if (strCommand == "current")
    {
        CThrone* winner = mnodeman.GetCurrentThroNe(1);
        if(winner) {
            Object obj;

            obj.push_back(Pair("IP:port",       winner->addr.ToString()));
            obj.push_back(Pair("protocol",      (int64_t)winner->protocolVersion));
            obj.push_back(Pair("vin",           winner->vin.prevout.hash.ToString()));
            obj.push_back(Pair("pubkey",        CBitcoinAddress(winner->pubkey.GetID()).ToString()));
            obj.push_back(Pair("lastseen",      (winner->lastPing == CThronePing()) ? winner->sigTime :
                                                        (int64_t)winner->lastPing.sigTime));
            obj.push_back(Pair("activeseconds", (winner->lastPing == CThronePing()) ? 0 :
                                                        (int64_t)(winner->lastPing.sigTime - winner->sigTime)));
            return obj;
        }

        return "unknown";
    }

    if (strCommand == "debug")
    {
        if(activeThrone.status != ACTIVE_THRONE_INITIAL || !throneSync.IsSynced())
            return activeThrone.GetStatus();

        CTxIn vin = CTxIn();
        CPubKey pubkey = CScript();
        CKey key;
        if(!pwalletMain || !pwalletMain->GetThroneVinAndKeys(vin, pubkey, key))
            throw runtime_error("Missing throne input, please look at the documentation for instructions on throne creation\n");
        return activeThrone.GetStatus();
    }

    if(strCommand == "enforce")
    {
        return (uint64_t)enforceThronePaymentsTime;
    }

    if (strCommand == "start")
    {
        if(!fThroNe) throw runtime_error("you must set throne=1 in the configuration\n");

        {
            LOCK(pwalletMain->cs_wallet);
            EnsureWalletIsUnlocked();
        }

        if(activeThrone.status != ACTIVE_THRONE_STARTED){
            activeThrone.status = ACTIVE_THRONE_INITIAL; // TODO: consider better way
            activeThrone.ManageStatus();
        }

        return activeThrone.GetStatus();
    }

    if (strCommand == "start-alias")
    {
        if (params.size() < 2){
            throw runtime_error("command needs at least 2 parameters\n");
        }

        {
            LOCK(pwalletMain->cs_wallet);
            EnsureWalletIsUnlocked();
        }

        std::string alias = params[1].get_str();

        bool found = false;

        Object statusObj;
        statusObj.push_back(Pair("alias", alias));

        BOOST_FOREACH(CThroneConfig::CThroneEntry mne, throneConfig.getEntries()) {
            if(mne.getAlias() == alias) {
                found = true;
                std::string errorMessage;
                CThroneBroadcast mnb;

                bool result = CThroneBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), errorMessage, mnb);

                statusObj.push_back(Pair("result", result ? "successful" : "failed"));
                if(result) {
                    mnodeman.UpdateThroneList(mnb);
                    mnb.Relay();
                } else {
                    statusObj.push_back(Pair("errorMessage", errorMessage));
                }
                break;
            }
        }

        if(!found) {
            statusObj.push_back(Pair("result", "failed"));
            statusObj.push_back(Pair("errorMessage", "could not find alias in config. Verify with list-conf."));
        }

        return statusObj;

    }

    if (strCommand == "start-many" || strCommand == "start-all" || strCommand == "start-missing" || strCommand == "start-disabled")
    {

        {
            LOCK(pwalletMain->cs_wallet);
            EnsureWalletIsUnlocked();
        }

        if((strCommand == "start-missing" || strCommand == "start-disabled") &&
         (throneSync.RequestedThroneAssets <= THRONE_SYNC_LIST ||
          throneSync.RequestedThroneAssets == THRONE_SYNC_FAILED)) {
            throw runtime_error("You can't use this command until throne list is synced\n");
        }

        std::vector<CThroneConfig::CThroneEntry> mnEntries;
        mnEntries = throneConfig.getEntries();

        int successful = 0;
        int failed = 0;

        Object resultsObj;

        BOOST_FOREACH(CThroneConfig::CThroneEntry mne, throneConfig.getEntries()) {
            std::string errorMessage;

            CTxIn vin = CTxIn(uint256S(mne.getTxHash()), uint32_t(atoi(mne.getOutputIndex().c_str())));
            CThrone *pmn = mnodeman.Find(vin);
            CThroneBroadcast mnb;

            if(strCommand == "start-missing" && pmn) continue;
            if(strCommand == "start-disabled" && pmn && pmn->IsEnabled()) continue;

            bool result = CThroneBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), errorMessage, mnb);

            Object statusObj;
            statusObj.push_back(Pair("alias", mne.getAlias()));
            statusObj.push_back(Pair("result", result ? "successful" : "failed"));

            if(result) {
                successful++;
                mnodeman.UpdateThroneList(mnb);
                mnb.Relay();
            } else {
                failed++;
                statusObj.push_back(Pair("errorMessage", errorMessage));
            }

            resultsObj.push_back(Pair("status", statusObj));
        }

        Object returnObj;
        returnObj.push_back(Pair("overall", strprintf("Successfully started %d thrones, failed to start %d, total %d", successful, failed, successful + failed)));
        returnObj.push_back(Pair("detail", resultsObj));

        return returnObj;
    }

    if (strCommand == "create")
    {

        throw runtime_error("Not implemented yet, please look at the documentation for instructions on throne creation\n");
    }

    if (strCommand == "genkey")
    {
        CKey secret;
        secret.MakeNewKey(false);

        return CBitcoinSecret(secret).ToString();
    }

    if(strCommand == "list-conf")
    {
        std::vector<CThroneConfig::CThroneEntry> mnEntries;
        mnEntries = throneConfig.getEntries();

        Object resultObj;

        BOOST_FOREACH(CThroneConfig::CThroneEntry mne, throneConfig.getEntries()) {
            CTxIn vin = CTxIn(uint256S(mne.getTxHash()), uint32_t(atoi(mne.getOutputIndex().c_str())));
            CThrone *pmn = mnodeman.Find(vin);

            std::string strStatus = pmn ? pmn->Status() : "MISSING";

            Object mnObj;
            mnObj.push_back(Pair("alias", mne.getAlias()));
            mnObj.push_back(Pair("address", mne.getIp()));
            mnObj.push_back(Pair("privateKey", mne.getPrivKey()));
            mnObj.push_back(Pair("txHash", mne.getTxHash()));
            mnObj.push_back(Pair("outputIndex", mne.getOutputIndex()));
            mnObj.push_back(Pair("status", strStatus));
            resultObj.push_back(Pair("throne", mnObj));
        }

        return resultObj;
    }

    if (strCommand == "outputs"){
        // Find possible candidates
        std::vector<COutput> vPossibleCoins;
        pwalletMain->AvailableCoins(vPossibleCoins, true, NULL, ONLY_10000);

        Object obj;
        BOOST_FOREACH(COutput& out, vPossibleCoins)
            obj.push_back(Pair(out.tx->GetHash().ToString(), strprintf("%d", out.i)));

        return obj;

    }

    if(strCommand == "status")
    {
        if(!fThroNe) throw runtime_error("This is not a throne\n");

        Object mnObj;
        CThrone *pmn = mnodeman.Find(activeThrone.vin);

        mnObj.push_back(Pair("vin", activeThrone.vin.ToString()));
        mnObj.push_back(Pair("service", activeThrone.service.ToString()));
        if (pmn) mnObj.push_back(Pair("pubkey", CBitcoinAddress(pmn->pubkey.GetID()).ToString()));
        mnObj.push_back(Pair("status", activeThrone.GetStatus()));
        return mnObj;
    }

    if (strCommand == "winners")
    {
        int nLast = 10;

        if (params.size() >= 2){
            nLast = atoi(params[1].get_str());
        }

        Object obj;

        for(int nHeight = chainActive.Tip()->nHeight-nLast; nHeight < chainActive.Tip()->nHeight+20; nHeight++)
        {
            obj.push_back(Pair(strprintf("%d", nHeight), GetRequiredPaymentsString(nHeight)));
        }

        return obj;
    }

    /*
        Shows which throne wins by score each block
    */
    if (strCommand == "calcscore")
    {

        int nLast = 10;

        if (params.size() >= 2){
            nLast = atoi(params[1].get_str());
        }
        Object obj;

        std::vector<CThrone> vThrones = mnodeman.GetFullThroneVector();
        for(int nHeight = chainActive.Tip()->nHeight-nLast; nHeight < chainActive.Tip()->nHeight+20; nHeight++){
            arith_uint256  nHigh = 0;
            CThrone *pBestThrone = NULL;
            BOOST_FOREACH(CThrone& mn, vThrones) {
                arith_uint256  n = UintToArith256(mn.CalculateScore(1, nHeight-100));
                if(n > nHigh){
                    nHigh = n;
                    pBestThrone = &mn;
                }
            }
            if(pBestThrone)
                obj.push_back(Pair(strprintf("%d", nHeight), pBestThrone->vin.prevout.ToStringShort().c_str()));
        }

        return obj;
    }

    return Value::null;
}

Value thronelist(const Array& params, bool fHelp)
{
    std::string strMode = "status";
    std::string strFilter = "";

    if (params.size() >= 1) strMode = params[0].get_str();
    if (params.size() == 2) strFilter = params[1].get_str();

    if (fHelp ||
            (strMode != "status" && strMode != "vin" && strMode != "pubkey" && strMode != "lastseen" && strMode != "activeseconds" && strMode != "rank" && strMode != "addr"
                && strMode != "protocol" && strMode != "full" && strMode != "lastpaid"))
    {
        throw runtime_error(
                "thronelist ( \"mode\" \"filter\" )\n"
                "Get a list of thrones in different modes\n"
                "\nArguments:\n"
                "1. \"mode\"      (string, optional/required to use filter, defaults = status) The mode to run list in\n"
                "2. \"filter\"    (string, optional) Filter results. Partial match by IP by default in all modes,\n"
                "                                    additional matches in some modes are also available\n"
                "\nAvailable modes:\n"
                "  activeseconds  - Print number of seconds throne recognized by the network as enabled\n"
                "                   (since latest issued \"throne start/start-many/start-alias\")\n"
                "  addr           - Print ip address associated with a throne (can be additionally filtered, partial match)\n"
                "  full           - Print info in format 'status protocol pubkey IP lastseen activeseconds lastpaid'\n"
                "                   (can be additionally filtered, partial match)\n"
                "  lastseen       - Print timestamp of when a throne was last seen on the network\n"
                "  lastpaid       - The last time a node was paid on the network\n"
                "  protocol       - Print protocol of a throne (can be additionally filtered, exact match))\n"
                "  pubkey         - Print public key associated with a throne (can be additionally filtered,\n"
                "                   partial match)\n"
                "  rank           - Print rank of a throne based on current block\n"
                "  status         - Print throne status: ENABLED / EXPIRED / VIN_SPENT / REMOVE / POS_ERROR\n"
                "                   (can be additionally filtered, partial match)\n"
                );
    }

    Object obj;
    if (strMode == "rank") {
        std::vector<pair<int, CThrone> > vThroneRanks = mnodeman.GetThroneRanks(chainActive.Tip()->nHeight);
        BOOST_FOREACH(PAIRTYPE(int, CThrone)& s, vThroneRanks) {
            std::string strVin = s.second.vin.prevout.ToStringShort();
            if(strFilter !="" && strVin.find(strFilter) == string::npos) continue;
            obj.push_back(Pair(strVin,       s.first));
        }
    } else {
        std::vector<CThrone> vThrones = mnodeman.GetFullThroneVector();
        BOOST_FOREACH(CThrone& mn, vThrones) {
            std::string strVin = mn.vin.prevout.ToStringShort();
            if (strMode == "activeseconds") {
                if(strFilter !="" && strVin.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(strVin,       (int64_t)(mn.lastPing.sigTime - mn.sigTime)));
            } else if (strMode == "addr") {
                if(strFilter !="" && mn.vin.prevout.hash.ToString().find(strFilter) == string::npos &&
                    strVin.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(strVin,       mn.addr.ToString()));
            } else if (strMode == "full") {
                std::ostringstream addrStream;
                addrStream << setw(21) << strVin;

                std::ostringstream stringStream;
                stringStream << setw(9) <<
                               mn.Status() << " " <<
                               mn.protocolVersion << " " <<
                               CBitcoinAddress(mn.pubkey.GetID()).ToString() << " " << setw(21) <<
                               mn.addr.ToString() << " " <<
                               (int64_t)mn.lastPing.sigTime << " " << setw(8) <<
                               (int64_t)(mn.lastPing.sigTime - mn.sigTime) << " " <<
                               (int64_t)mn.GetLastPaid();
                std::string output = stringStream.str();
                stringStream << " " << strVin;
                if(strFilter !="" && stringStream.str().find(strFilter) == string::npos &&
                        strVin.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(addrStream.str(), output));
            } else if (strMode == "lastseen") {
                if(strFilter !="" && strVin.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(strVin,       (int64_t)mn.lastPing.sigTime));
            } else if (strMode == "lastpaid"){
                if(strFilter !="" && mn.vin.prevout.hash.ToString().find(strFilter) == string::npos &&
                    strVin.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(strVin,      (int64_t)mn.GetLastPaid()));
            } else if (strMode == "protocol") {
                if(strFilter !="" && strFilter != strprintf("%d", mn.protocolVersion) &&
                    strVin.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(strVin,       (int64_t)mn.protocolVersion));
            } else if (strMode == "pubkey") {
                CBitcoinAddress address(mn.pubkey.GetID());

                if(strFilter !="" && address.ToString().find(strFilter) == string::npos &&
                    strVin.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(strVin,       address.ToString()));
            } else if(strMode == "status") {
                std::string strStatus = mn.Status();
                if(strFilter !="" && strVin.find(strFilter) == string::npos && strStatus.find(strFilter) == string::npos) continue;
                obj.push_back(Pair(strVin,       strStatus));
            }
        }
    }
    return obj;

}

bool DecodeHexVecMnb(std::vector<CThroneBroadcast>& vecMnb, std::string strHexMnb) {

    if (!IsHex(strHexMnb))
        return false;

    vector<unsigned char> mnbData(ParseHex(strHexMnb));
    CDataStream ssData(mnbData, SER_NETWORK, PROTOCOL_VERSION);
    try {
        ssData >> vecMnb;
    }
    catch (const std::exception&) {
        return false;
    }

    return true;
}

Value thronebroadcast(const Array& params, bool fHelp)
{
    string strCommand;
    if (params.size() >= 1)
        strCommand = params[0].get_str();

    if (fHelp  ||
        (strCommand != "create-alias" && strCommand != "create-all" && strCommand != "decode" && strCommand != "relay"))
        throw runtime_error(
                "thronebroadcast \"command\"... ( \"passphrase\" )\n"
                "Set of commands to create and relay throne broadcast messages\n"
                "\nArguments:\n"
                "1. \"command\"        (string or set of strings, required) The command to execute\n"
                "2. \"passphrase\"     (string, optional) The wallet passphrase\n"
                "\nAvailable commands:\n"
                "  create-alias  - Create single remote throne broadcast message by assigned alias configured in throne.conf\n"
                "  create-all    - Create remote throne broadcast messages for all thrones configured in throne.conf\n"
                "  decode        - Decode throne broadcast message\n"
                "  relay         - Relay throne broadcast message to the network\n"
                + HelpRequiringPassphrase());

    if (strCommand == "create-alias")
    {
        // wait for reindex and/or import to finish
        if (fImporting || fReindex)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Wait for reindex and/or import to finish");

        if (params.size() < 2)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Command needs at least 2 parameters");

        {
            LOCK(pwalletMain->cs_wallet);
            EnsureWalletIsUnlocked();
        }

        std::string alias = params[1].get_str();

        bool found = false;

        Object statusObj;
        std::vector<CThroneBroadcast> vecMnb;

        statusObj.push_back(Pair("alias", alias));

        BOOST_FOREACH(CThroneConfig::CThroneEntry mne, throneConfig.getEntries()) {
            if(mne.getAlias() == alias) {
                found = true;
                std::string errorMessage;
                CThroneBroadcast mnb;

                bool result = CThroneBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), errorMessage, mnb, true);

                statusObj.push_back(Pair("result", result ? "successful" : "failed"));
                if(result) {
                    vecMnb.push_back(mnb);
                    CDataStream ssVecMnb(SER_NETWORK, PROTOCOL_VERSION);
                    ssVecMnb << vecMnb;
                    statusObj.push_back(Pair("hex", HexStr(ssVecMnb.begin(), ssVecMnb.end())));
                } else {
                    statusObj.push_back(Pair("errorMessage", errorMessage));
                }
                break;
            }
        }

        if(!found) {
            statusObj.push_back(Pair("result", "not found"));
            statusObj.push_back(Pair("errorMessage", "Could not find alias in config. Verify with list-conf."));
        }

        return statusObj;

    }

    if (strCommand == "create-all")
    {
        // wait for reindex and/or import to finish
        if (fImporting || fReindex)
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Wait for reindex and/or import to finish");

        {
            LOCK(pwalletMain->cs_wallet);
            EnsureWalletIsUnlocked();
        }

        std::vector<CThroneConfig::CThroneEntry> mnEntries;
        mnEntries = throneConfig.getEntries();

        int successful = 0;
        int failed = 0;

        Object resultsObj;
        std::vector<CThroneBroadcast> vecMnb;

        BOOST_FOREACH(CThroneConfig::CThroneEntry mne, throneConfig.getEntries()) {
            std::string errorMessage;

            CTxIn vin = CTxIn(uint256S(mne.getTxHash()), uint32_t(atoi(mne.getOutputIndex().c_str())));
            CThroneBroadcast mnb;

            bool result = CThroneBroadcast::Create(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), errorMessage, mnb, true);

            Object statusObj;
            statusObj.push_back(Pair("alias", mne.getAlias()));
            statusObj.push_back(Pair("result", result ? "successful" : "failed"));

            if(result) {
                successful++;
                vecMnb.push_back(mnb);
            } else {
                failed++;
                statusObj.push_back(Pair("errorMessage", errorMessage));
            }

            resultsObj.push_back(Pair("status", statusObj));
        }

        CDataStream ssVecMnb(SER_NETWORK, PROTOCOL_VERSION);
        ssVecMnb << vecMnb;
        Object returnObj;
        returnObj.push_back(Pair("overall", strprintf("Successfully created broadcast messages for %d thrones, failed to create %d, total %d", successful, failed, successful + failed)));
        returnObj.push_back(Pair("detail", resultsObj));
        returnObj.push_back(Pair("hex", HexStr(ssVecMnb.begin(), ssVecMnb.end())));

        return returnObj;
    }

    if (strCommand == "decode")
    {
        if (params.size() != 2)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Correct usage is 'thronebroadcast decode \"hexstring\"'");

        int successful = 0;
        int failed = 0;

        std::vector<CThroneBroadcast> vecMnb;
        Object returnObj;

        if (!DecodeHexVecMnb(vecMnb, params[1].get_str()))
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Throne broadcast message decode failed");

        BOOST_FOREACH(CThroneBroadcast& mnb, vecMnb) {
            Object resultObj;

            if(mnb.VerifySignature()) {
                successful++;
                resultObj.push_back(Pair("vin", mnb.vin.ToString()));
                resultObj.push_back(Pair("addr", mnb.addr.ToString()));
                resultObj.push_back(Pair("pubkey", CBitcoinAddress(mnb.pubkey.GetID()).ToString()));
                resultObj.push_back(Pair("pubkey2", CBitcoinAddress(mnb.pubkey2.GetID()).ToString()));
                resultObj.push_back(Pair("vchSig", EncodeBase64(&mnb.sig[0], mnb.sig.size())));
                resultObj.push_back(Pair("sigTime", mnb.sigTime));
                resultObj.push_back(Pair("protocolVersion", mnb.protocolVersion));
                resultObj.push_back(Pair("nLastDsq", mnb.nLastDsq));

                Object lastPingObj;
                lastPingObj.push_back(Pair("vin", mnb.lastPing.vin.ToString()));
                lastPingObj.push_back(Pair("blockHash", mnb.lastPing.blockHash.ToString()));
                lastPingObj.push_back(Pair("sigTime", mnb.lastPing.sigTime));
                lastPingObj.push_back(Pair("vchSig", EncodeBase64(&mnb.lastPing.vchSig[0], mnb.lastPing.vchSig.size())));

                resultObj.push_back(Pair("lastPing", lastPingObj));
            } else {
                failed++;
                resultObj.push_back(Pair("errorMessage", "Throne broadcast signature verification failed"));
            }

            returnObj.push_back(Pair(mnb.GetHash().ToString(), resultObj));
        }

        returnObj.push_back(Pair("overall", strprintf("Successfully decoded broadcast messages for %d thrones, failed to decode %d, total %d", successful, failed, successful + failed)));

        return returnObj;
    }

    if (strCommand == "relay")
    {
        if (params.size() < 2 || params.size() > 3)
            throw JSONRPCError(RPC_INVALID_PARAMETER,   "thronebroadcast relay \"hexstring\" ( fast )\n"
                                                        "\nArguments:\n"
                                                        "1. \"hex\"      (string, required) Broadcast messages hex string\n"
                                                        "2. fast       (string, optional) If none, using safe method\n");

        int successful = 0;
        int failed = 0;
        bool fSafe = params.size() == 2;

        std::vector<CThroneBroadcast> vecMnb;
        Object returnObj;

        if (!DecodeHexVecMnb(vecMnb, params[1].get_str()))
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Throne broadcast message decode failed");

        // verify all signatures first, bailout if any of them broken
        BOOST_FOREACH(CThroneBroadcast& mnb, vecMnb) {
            Object resultObj;

            resultObj.push_back(Pair("vin", mnb.vin.ToString()));
            resultObj.push_back(Pair("addr", mnb.addr.ToString()));

            int nDos = 0;
            bool fResult;
            if (mnb.VerifySignature()) {
                if (fSafe) {
                    fResult = mnodeman.CheckMnbAndUpdateThroneList(mnb, nDos);
                } else {
                    mnodeman.UpdateThroneList(mnb);
                    mnb.Relay();
                    fResult = true;
                }
            } else fResult = false;

            if(fResult) {
                successful++;
                mnodeman.UpdateThroneList(mnb);
                mnb.Relay();
                resultObj.push_back(Pair(mnb.GetHash().ToString(), "successful"));
            } else {
                failed++;
                resultObj.push_back(Pair("errorMessage", "Throne broadcast signature verification failed"));
            }

            returnObj.push_back(Pair(mnb.GetHash().ToString(), resultObj));
        }

        returnObj.push_back(Pair("overall", strprintf("Successfully relayed broadcast messages for %d thrones, failed to relay %d, total %d", successful, failed, successful + failed)));

        return returnObj;
    }

    return Value::null;
}
