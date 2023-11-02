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
CFTokenIssuanceCreate::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureCFTokensV1))
        return temDISABLED;

    if (auto const ret = preflight1(ctx); !isTesSuccess(ret))
        return ret;

    if (ctx.tx.getFlags() & tfCFTokenAuthorizeMask)
        return temINVALID_FLAG;

    auto const accountID = ctx.tx[sfAccount];
    auto const holder = ctx.tx[~sfCFTokenHolder];
    if (holder && accountID == holder)
        return temMALFORMED;

    return preflight2(ctx);
}

TER
CFTokenIssuanceCreate::preclaim(PreclaimContext const& ctx)
{
    auto const sleCFT =
        ctx.view.read(keylet::cftIssuance(ctx.tx[sfCFTokenIssuanceID]));
    if (!sleCFT)
        return tecOBJECT_NOT_FOUND;

    auto const accountID = ctx.tx[sfAccount];
    auto const txFlags = ctx.tx[sfFlags];
    auto const holder = ctx.tx[~sfCFTokenHolder]; 
    std::uint32_t const cftIssuanceFlags = sleCFT->getFieldU32(sfFlags);

    // TODO: locate the CFToken Object and check if it exists
    auto const sleCft = ;

    // If tx is submitted by issuer, they would either try to do the following for allowlisting
    // 1. authorize an account
    // 2. unauthorize an account
    if (accountID == (*sleCFT)[sfIssuer]){
        // If tx is submitted by issuer, it only applies for CFT with lsfCFTRequireAuth set
        if (!(cftIssuanceFlags & lsfCFTRequireAuth))
            return tecNO_PERMISSION;
        
        if (!holder)
            return no holder specfied;

        if (!sleCft)
            return no object;

        // issuer wants to authorize the holder
        if (!(txFlags & tfCFTUnathorize)){
            //make sure the holder is not already authorized
            if ((*sleCft)[sfFlags] & lsfCFTAuthorized)
                return flag already set;
            

        }
        // unathorize a holder
        else {
            if (!(*sleCft)[sfFlags] & lsfCFTAuthorized)
                return cft is not authorized;
        }


    }
    else{
        if(holder)
            return temMALFORMED;

        // if holder wants to hold a cft
        if (!(txFlags & tfCFTUnathorize)){

            if (sleCft)
                return already holds object;


        }
        // if holder no longer wants to use cft
        else{
           if((*sleCft)[sfCFTAmount] != 0)
            return tecHAS_OBLIGATIONS; 
        }
    }
    return tesSUCCESS;
}

TER
CFTokenIssuanceCreate::doApply()
{

    return tesSUCCESS;
}

}  // namespace ripple
