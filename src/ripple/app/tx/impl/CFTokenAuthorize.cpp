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
    std::uint32_t const cftIssuanceFlags = sleCFT->getFieldU32(sfFlags);

    // If tx is submitted by issuer, they would either try to do the following for allowlisting
    // 1. authorize an account
    // 2. unauthorize an account
    if (accountID == (*sleCFT)[sfIssuer]){
        // If tx is submitted by issuer, it only applies for CFT with lsfCFTRequireAuth set
        if (!(cftIssuanceFlags & lsfCFTRequireAuth))
            return tecNO_PERMISSION;
        
        if()
    }
    else()
    return tesSUCCESS;
}

TER
CFTokenIssuanceCreate::doApply()
{

    return tesSUCCESS;
}

}  // namespace ripple
