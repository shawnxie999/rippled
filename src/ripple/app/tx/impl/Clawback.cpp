//------------------------------------------------------------------------------
/*
  This file is part of rippled: https://github.com/ripple/rippled
  Copyright (c) 2021 Ripple Labs Inc.

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

#include <ripple/app/tx/impl/Clawback.h>
#include <ripple/ledger/Directory.h>
#include <ripple/ledger/View.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/Protocol.h>
#include <ripple/protocol/TxFlags.h>
#include <ripple/protocol/st.h>
#include <boost/endian/conversion.hpp>
#include <array>

namespace ripple {

NotTEC
Clawback::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureClawback))
        return temDISABLED;

    AccountID const issuer = ctx.tx.getAccountID(sfAccount);
    STAmount const clawAmount(ctx.tx.getFieldAmount(sfAmount));

    // The issuer field is used for the token holder instead
    AccountID const holder = clawAmount.getIssuer();

    if(issuer == holder)
        return temBAD_AMOUNT;             

    return preflight2(ctx);
}

TER
Clawback::preclaim(PreclaimContext const& ctx)
{
    AccountID const issuer = ctx.tx.getAccountID(sfAccount);
    STAmount const clawAmount(ctx.tx.getFieldAmount(sfAmount));
    AccountID const holder = clawAmount.getIssuer();

    auto const sleIssuer = ctx.view.read(keylet::account(issuer));
    auto const sleHolder = ctx.view.read(keylet::account(holder));
    if (!sleIssuer || !sleHolder)
        return terNO_ACCOUNT;

    std::uint32_t const issuerFlagsIn = sleIssuer->getFieldU32(sfFlags);

    // If AllowClawback is not set or NoFreeze is set, return no permission
    if (!(issuerFlagsIn & asfAllowClawback) || (issuerFlagsIn & lsfNoFreeze))
        return tecNO_PERMISSION;
    
    // Trustline must exist and balance is non-zero
    if (!accountHolds(ctx.view,
                    holder,
                    clawAmount.getCurrency(),
                    issuer,
                    fhIGNORE_FREEZE,
                    ctx.j))
        return tecNO_LINE;

    // The account of the tx must be the issuer of the currecy
    auto sleRippleState = ctx.view.read(keylet::line(holder, issuer, clawAmount.getCurrency()));
    bool const bHigh = issuer > holder;
    AccountID const& uLowAccountID = !bHigh ? issuer : holder;
    AccountID const& highAccountID = bHigh ? issuer : holder;
    STAmount const balance = sleRippleState->getFieldAmount(sfBalance);
    if (balance > beast::zero && highAccountID != issuer)
        return tecNO_PERMISSION;
    if (balance < beast::zero && uLowAccountID != issuer)
        return tecNO_PERMISSION;

    return tesSUCCESS;
}

TER
Clawback::clawback(
    AccountID const& issuer,
    AccountID const& holder,
    STAmount const& amount)
{
    // This should never happen, but it's easy and quick to check.
    if (amount < beast::zero)
        return tecINTERNAL;

    if (amount == beast::zero)
        return tesSUCCESS;

    auto const result = accountSend(view(), holder, issuer, amount, j_);

    //todo check balance sign
    return tesSUCCESS;
}

TER
Clawback::changeRippleStateFreeze(AccountID const& issuer, AccountID const& holder, Currency const& currency){
    std::uint32_t const uTxFlags = ctx_.tx.getFlags();

    auto const sleAcct = view().peek(keylet::account(issuer));

    auto sleRippleState =
        view().peek(keylet::line(issuer, holder, currency));

    if (!sleAcct || !sleRippleState)
        return tecINTERNAL;

    std::uint32_t const uFlagsIn(sleRippleState->getFieldU32(sfFlags));
    std::uint32_t uFlagsOut(uFlagsIn);
        
    bool const bSetFreeze = (uTxFlags & tfSetFreeze);
    bool const bClearFreeze = (uTxFlags & tfClearFreeze);
    bool const bHigh = issuer > holder;

    // same logic as SetTrust transactor
    if (bSetFreeze && !bClearFreeze && !sleAcct->isFlag(lsfNoFreeze))
    {
        uFlagsOut |= (bHigh ? lsfHighFreeze : lsfLowFreeze);
    }
    else if (bClearFreeze && !bSetFreeze)
    {
        uFlagsOut &= ~(bHigh ? lsfHighFreeze : lsfLowFreeze);
    }

    if (uFlagsIn != uFlagsOut)
        sleRippleState->setFieldU32(sfFlags, uFlagsOut);
        view().update(sleRippleState);

    return tesSUCCESS;
}

TER
Clawback::doApply()
{
    AccountID const issuer = ctx_.tx.getAccountID(sfAccount);
    STAmount clawAmount(ctx_.tx.getFieldAmount(sfAmount));
    AccountID const holder = clawAmount.getIssuer();

    // issuer field was holder's address in request, needs to change 
    clawAmount.setIssuer(issuer);
 
    if( auto const ret = changeRippleStateFreeze( issuer, holder, clawAmount.getCurrency()); !isTesSuccess(ret))
        return ret;

    // Get the amount of spendable IOU that the holder has
    STAmount const spendableAmount = accountHolds(
                                            view(),
                                            holder,
                                            clawAmount.getCurrency(),
                                            clawAmount.getIssuer(),
                                            fhIGNORE_FREEZE,
                                            j_);

    if (spendableAmount > clawAmount)
        return clawback(issuer, holder, clawAmount);
    else
        return clawback(issuer, holder, spendableAmount);

    return tecINTERNAL;
}

}  // namespace ripple
