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
    std::vector<uint256> prevIDs;
    std::vector<uint256> finalIDs;

    // The owner is not necessarily the issuer, if using authorized minter
    // flow. Determine owner from the ledger object ID of the NFTokenPages
    // that were changed.
    std::optional<AccountID> owner;

    for (STObject const& node : transactionMeta.getNodes())
    {
        if (node.getFieldU16(sfLedgerEntryType) !=
            ltNFTOKEN_PAGE)
            continue;

        if (!owner)
            owner = AccountID::fromVoid(
                node.getFieldH256(sfLedgerIndex).data());

        if (node.getFName() == sfCreatedNode)
        {
            STArray const& toAddNFTs =
                node.peekAtField(sfNewFields)
                    .downcast<STObject>()
                    .getFieldArray(sfNFTokens);
            std::transform(
                toAddNFTs.begin(),
                toAddNFTs.end(),
                std::back_inserter(finalIDs),
                [](STObject const& nft) {
                    return nft.getFieldH256(sfNFTokenID);
                });
        }
        // Else it's modified, as there should never be a deleted NFToken page
        // as a result of a mint.
        else
        {
            // When a mint results in splitting an existing page,
            // it results in a created page and a modified node. Sometimes,
            // the created node needs to be linked to a third page, resulting
            // in modifying that third page's PreviousPageMin or NextPageMin
            // field changing, but no NFTs within that page changing. In this
            // case, there will be no previous NFTs and we need to skip.
            // However, there will always be NFTs listed in the final fields,
            // as rippled outputs all fields in final fields even if they were
            // not changed.
            STObject const& previousFields =
                node.peekAtField(sfPreviousFields)
                    .downcast<STObject>();
            if (!previousFields.isFieldPresent(sfNFTokens))
                continue;

            STArray const& toAddNFTs =
                previousFields.getFieldArray(sfNFTokens);
            std::transform(
                toAddNFTs.begin(),
                toAddNFTs.end(),
                std::back_inserter(prevIDs),
                [](STObject const& nft) {
                    return nft.getFieldH256(sfNFTokenID);
                });

            STArray const& toAddFinalNFTs =
                node.peekAtField(sfFinalFields)
                    .downcast<STObject>()
                    .getFieldArray(sfNFTokens);
            std::transform(
                toAddFinalNFTs.begin(),
                toAddFinalNFTs.end(),
                std::back_inserter(finalIDs),
                [](STObject const& nft) {
                    return nft.getFieldH256(sfNFTokenID);
                });
        }
    }

    std::sort(finalIDs.begin(), finalIDs.end());
    std::sort(prevIDs.begin(), prevIDs.end());
    std::vector<uint256> tokenIDResult;
    std::set_difference(
        finalIDs.begin(),
        finalIDs.end(),
        prevIDs.begin(),
        prevIDs.end(),
        std::inserter(tokenIDResult, tokenIDResult.begin()));
    if (tokenIDResult.size() == 1 && owner)
    {
        response[jss::nft_id] = to_string(tokenIDResult.front());
    }

}


}  // namespace RPC
}  // namespace ripple
