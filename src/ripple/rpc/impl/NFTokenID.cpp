//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2019 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include <ripple/rpc/NFTokenID.h>

#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/ledger/OpenLedger.h>
#include <ripple/app/misc/Transaction.h>
#include <ripple/ledger/View.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/Feature.h>
#include <ripple/rpc/Context.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <boost/algorithm/string/case_conv.hpp>

namespace ripple {
namespace RPC {

bool
canHaveNFTokenID(
    std::shared_ptr<STTx const> const& serializedTx,
    TxMeta const& transactionMeta)
{
    if (!serializedTx)
        return false;

    {
        TxType const tt{serializedTx->getTxnType()};
        if (tt != ttNFTOKEN_MINT && tt != ttNFTOKEN_ACCEPT_OFFER && tt != ttNFTOKEN_CANCEL_OFFER)
            return false;
    }

    // if the transaction failed nothing could have been delivered.
    if (transactionMeta.getResultTER() != tesSUCCESS)
        return false;

    return true;
}

void
insertNFTokenID(
    Json::Value& response,
    RPC::JsonContext const& context,
    std::shared_ptr<Transaction> const& transaction,
    TxMeta const& transactionMeta)
{
    if(!canHaveNFTokenID(transaction->getSTransaction(), transactionMeta))
        return;

    auto affectedNodes = transactionMeta.getAsObject().getFieldArray(sfAffectedNodes);

    std::optional<uint256> newNFTokenID;
    for(auto & node: affectedNodes){    
        if(node.getFieldU16(sfLedgerEntryType) == ltNFTOKEN_PAGE && node.isFieldPresent(sfPreviousFields)){
            auto const& previousFields = node.getFieldObject(sfPreviousFields);
            auto const& finalFields = node.getFieldObject(sfFinalFields);

            if(!previousFields.isFieldPresent(sfNFTokens) || !finalFields.isFieldPresent(sfNFTokens))
                continue;
            
            auto const& prevTokens = previousFields.getFieldArray(sfNFTokens);
            std::vector<uint256> prevIDs;

            for(auto const& nftoken: prevTokens)
                prevIDs.emplace_back(nftoken.getFieldH256(sfNFTokenID));
            
            auto const& finalTokens = finalFields.getFieldArray(sfNFTokens);
            std::vector<uint256> finalIDs;

            for(auto const& nftoken: finalTokens)
                finalIDs.emplace_back(nftoken.getFieldH256(sfNFTokenID));

            std::vector<uint256> diff;
            std::sort(prevIDs.begin(), prevIDs.end());
            std::sort(finalIDs.begin(), finalIDs.end());
            
            std::set_difference(prevIDs.begin(), prevIDs.end(), finalIDs.begin(), finalIDs.end(), std::back_inserter(diff));
            if(diff.size() <= 0)
                continue;

            newNFTokenID = diff[0];
            break;

        }
        else if( node.getFieldU16(sfLedgerEntryType) == ltNFTOKEN_PAGE && node.isFieldPresent(sfNewFields) ){
            auto const& newFields = node.getFieldObject(sfNewFields);

            if(!newFields.isFieldPresent(sfNFTokens))
                continue;
        
            auto const& nftokens = newFields.getFieldArray(sfNFTokens);

            if(nftokens.size() <= 0)
                continue;
        
            newNFTokenID = nftokens[0].getFieldH256(sfNFTokenID);
            break;   
            
        }
    }

    if(newNFTokenID)
        response[jss::nft_id] = to_string(newNFTokenID.value());

}


}  // namespace RPC
}  // namespace ripple
