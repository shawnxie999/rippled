//------------------------------------------------------------------------------
/*
  This file is part of rippled: https://github.com/ripple/rippled
  Copyright (c) 2023 Ripple Labs Inc.

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

#include <ripple/app/tx/impl/CFTokenAuthorize.h>
#include <ripple/ledger/View.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/TxFlags.h>
#include <ripple/protocol/st.h>

namespace ripple {

NotTEC
CFTokenAuthorize::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureCFTokensV1))
        return temDISABLED;

    if (auto const ret = preflight1(ctx); !isTesSuccess(ret))
        return ret;

    if (ctx.tx.getFlags() & tfCFTokenAuthorizeMask)
        return temINVALID_FLAG;

    auto const accountID = ctx.tx[sfAccount];
    auto const holderID = ctx.tx[~sfCFTokenholder];
    if (holderID && accountID == holderID)
        return temMALFORMED;

    return preflight2(ctx);
}

TER
CFTokenAuthorize::preclaim(PreclaimContext const& ctx)
{
    auto const sleCftIssuance =
        ctx.view.read(keylet::cftIssuance(ctx.tx[sfCFTokenIssuanceID]));
    if (!sleCftIssuance)
        return tecOBJECT_NOT_FOUND;

    auto const accountID = ctx.tx[sfAccount];
    auto const txFlags = ctx.tx[sfFlags];
    auto const holderID = ctx.tx[~sfCFTokenholder]; 

    if (holderID && !(ctx.view.exists(keylet::account(holderID))))
        return tecNO_DST;
    
    std::uint32_t const cftIssuanceFlags = sleCftIssuance->getFieldU32(sfFlags);

    // TODO: locate the CFToken Object and check if it exists
    std::shared_ptr<SLE const> const sleCft;

    // If tx is submitted by issuer, they would either try to do the following for allowlisting
    // 1. authorize an account
    // 2. unauthorize an account
    if (accountID == (*sleCftIssuance)[sfIssuer]){

        // If tx is submitted by issuer, it only applies for CFT with lsfCFTRequireAuth set
        if (!(cftIssuanceFlags & lsfCFTRequireAuth))
            return tecNO_AUTH;
        
        if (!holderID)
            return temMALFORMED;

        sleCft = ctx.view.read(keylet::cftoken(ctx.tx[sfCFTokenIssuanceID], holderID));
        if (!sleCft)
            return tecNO_ENTRY;

        auto const sleCftFlags = (*sleCft)[sfFlags];

        // issuer wants to unauthorize the holder
        if (txFlags & tfCFTUnathorize){
            if (!(sleCftFlags & lsfCFTAuthorized))
                return temINVALID_FLAG;

        }
        // authorize a holder
        else {
            //make sure the holder is not already authorized
            if (sleCftFlags & lsfCFTAuthorized)
                return temMALFORMED;
        }
    }
    // if non-issuer account submits this tx, then they are trying either:
    // 1. Unauthorize/delete CFToken
    // 2. Use/create CFToken
    else{
        if(holderID)
            return temMALFORMED;

        sleCft = ctx.view.read(keylet::cftoken(ctx.tx[sfCFTokenIssuanceID], accountID));

        // if holder wants to delete/unauthorize a cft
        if (txFlags & tfCFTUnathorize){
            if (!sleCft)
                return tecNO_ENTRY;

            if((*sleCft)[sfCFTAmount] != 0)
                return tecHAS_OBLIGATIONS; 
        }
        // if holder wants to use and create a cft
        else{
            if (sleCft)
                return tecDUPLICATE;
        }
    }
    return tesSUCCESS;
}

TER
CFTokenAuthorize::doApply()
{
    auto const cftIssuanceID = ctx_.tx[sfCFTokenIssuanceID];
    auto const sleCftIssuance =
        view().read(keylet::cftIssuance(cftIssuanceID));
    if (!sleCftIssuance)
        return tecINTERNAL;


    auto const holderID = ctx_.tx[~sfCFTokenholder];
    auto const txFlags = ctx.tx[sfFlags];

    if (accountID == (*sleCftIssuance)[sfIssuer]){
        auto const sleHolder = 
    }

    else{
        // if holder wants to delete/unauthorize a cft
        if (txFlags & tfCFTUnathorize){
            if (!sleCft)
                return tecNO_ENTRY;

            if((*sleCft)[sfCFTAmount] != 0)
                return tecHAS_OBLIGATIONS; 
        }
        // user wants to create a cft
        else{
            auto const sleAcct = view().peek(keylet::account(account_));
            if (!acct)
                return tecINTERNAL;

            if (mPriorBalance < view().fees().accountReserve((*sleAcct)[sfOwnerCount] + 1))
                return tecINSUFFICIENT_RESERVE;      

            auto const cftokenID = keylet::cftoken(cftIssuanceID, account_);

            
            auto const ownerNode = view().dirInsert(
                keylet::ownerDir(account_), cftokenID, describeOwnerDir(account_));

            if (!ownerNode)
                return tecDIR_FULL;
            

            auto const offerNode = view().dirInsert(keylet::cft_dir(cftIssuanceID), cftokenID,     
                [&cftokenID](std::shared_ptr<SLE> const& sle) {
                    (*sle)[sfCFTokenIssuanceID] = cftIssuanceID;
            });
            
            auto cftoken = std::make_shared<SLE>(cftokenID);
            (*cftoken)[sfAccount] = account_;
            (*cftoken)[sfCFTokenIssuanceID] = cftIssuanceID;
            (*cftoken)[sfFlags] = ctx_.tx.getFlags() & ~tfUniversalMask;
            (*cftoken)[sfCFTAmount] = 0;
            
            view().insert(cftoken);


            // Update owner count.
            adjustOwnerCount(view(), acct, 1, j_);
        }

    }




    return tesSUCCESS;
}

}  // namespace ripple
