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

#include <ripple/app/tx/impl/CFTokenIssuanceSet.h>
#include <ripple/ledger/View.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/st.h>
#include <ripple/protocol/TxFlags.h>

namespace ripple {

NotTEC
CFTokenIssuanceSet::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureCFTokensV1))
        return temDISABLED;

    //check flags
    if (auto const ret = preflight1(ctx); !isTesSuccess(ret))
        return ret;

    auto const txFlags = ctx.tx.getFlags();

    if (txFlags & tfCFTokenIssuanceSetMask)
        return temINVALID_FLAG;
    // fails if both flags are set
    else if ((txFlags & tfCFTLock) && (txFlags & tfCFTUnlock))
        return temINVALID_FLAG;
    // if no flag is set
    else if (!(txFlags & tfCFTLock & tfCFTUnlock))
        return temINVALID_FLAG;
        
    return preflight2(ctx);
}

TER
CFTokenIssuanceSet::preclaim(PreclaimContext const& ctx)
{
    // ensure that issuance exists
    auto const sleCFT =
        ctx.view.read(keylet::cftIssuance(ctx.tx[sfCFTokenIssuanceID]));
    if (!sleCFT)
        return tecOBJECT_NOT_FOUND;


    return tesSUCCESS;
}

TER
CFTokenIssuanceSet::doApply()
{


    return tesSUCCESS;
}

}  // namespace ripple