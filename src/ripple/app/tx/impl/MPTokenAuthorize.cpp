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

#include <ripple/app/tx/impl/MPTokenAuthorize.h>
#include <ripple/ledger/View.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/TxFlags.h>
#include <ripple/protocol/st.h>

namespace ripple {

NotTEC
MPTokenAuthorize::preflight(PreflightContext const& ctx)
{
    if (!ctx.rules.enabled(featureMPTokensV1))
        return temDISABLED;

    if (auto const ret = preflight1(ctx); !isTesSuccess(ret))
        return ret;

    if (ctx.tx.getFlags() & tfMPTokenAuthorizeMask)
        return temINVALID_FLAG;

    auto const accountID = ctx.tx[sfAccount];
    auto const holderID = ctx.tx[~sfMPTokenHolder];
    if (holderID && accountID == holderID)
        return temMALFORMED;

    return preflight2(ctx);
}

TER
MPTokenAuthorize::preclaim(PreclaimContext const& ctx)
{
    auto const sleMptIssuance =
        ctx.view.read(keylet::mptIssuance(ctx.tx[sfMPTokenIssuanceID]));
    if (!sleMptIssuance)
        return tecOBJECT_NOT_FOUND;

    auto const accountID = ctx.tx[sfAccount];
    auto const txFlags = ctx.tx.getFlags();
    auto const holderID = ctx.tx[~sfMPTokenHolder];

    if (holderID && !(ctx.view.exists(keylet::account(*holderID))))
        return tecNO_DST;

    std::uint32_t const mptIssuanceFlags = sleMptIssuance->getFieldU32(sfFlags);

    std::shared_ptr<SLE const> sleMpt;

    // If tx is submitted by issuer, they would either try to do the following
    // for allowlisting:
    // 1. authorize an account
    // 2. unauthorize an account
    //
    // Note: `accountID` is issuer's account
    //       `holderID` is holder's account
    if (accountID == (*sleMptIssuance)[sfIssuer])
    {
        // If tx is submitted by issuer, it only applies for MPT with
        // lsfMPTRequireAuth set
        if (!(mptIssuanceFlags & lsfMPTRequireAuth))
            return tecNO_AUTH;

        if (!holderID)
            return temMALFORMED;

        if (!ctx.view.exists(
                keylet::mptoken(ctx.tx[sfMPTokenIssuanceID], *holderID)))
            return tecNO_ENTRY;

        return tesSUCCESS;
    }

    // if non-issuer account submits this tx, then they are trying either:
    // 1. Unauthorize/delete MPToken
    // 2. Use/create MPToken
    //
    // Note: `accountID` is holder's account
    //       `holderID` is NOT used
    if (holderID)
        return temMALFORMED;

    sleMpt =
        ctx.view.read(keylet::mptoken(ctx.tx[sfMPTokenIssuanceID], accountID));

    // if holder wants to delete/unauthorize a mpt
    if (txFlags & tfMPTUnauthorize)
    {
        if (!sleMpt)
            return tecNO_ENTRY;

        if ((*sleMpt)[sfMPTAmount] != 0)
            return tecHAS_OBLIGATIONS;
    }
    // if holder wants to use and create a mpt
    else
    {
        if (sleMpt)
            return tecMPTOKEN_EXISTS;
    }

    return tesSUCCESS;
}

TER
MPTokenAuthorize::doApply()
{
    auto const mptIssuanceID = ctx_.tx[sfMPTokenIssuanceID];
    auto const sleMptIssuance = view().read(keylet::mptIssuance(mptIssuanceID));
    if (!sleMptIssuance)
        return tecINTERNAL;

    auto const sleAcct = view().peek(keylet::account(account_));
    if (!sleAcct)
        return tecINTERNAL;

    auto const holderID = ctx_.tx[~sfMPTokenHolder];
    auto const txFlags = ctx_.tx.getFlags();

    // If the account that submitted this tx is the issuer of the MPT
    // Note: `account_` is issuer's account
    //       `holderID` is holder's account
    if (account_ == (*sleMptIssuance)[sfIssuer])
    {
        if (!holderID)
            return tecINTERNAL;

        auto const sleMpt =
            view().peek(keylet::mptoken(mptIssuanceID, *holderID));
        if (!sleMpt)
            return tecINTERNAL;

        std::uint32_t const flagsIn = sleMpt->getFieldU32(sfFlags);
        std::uint32_t flagsOut = flagsIn;

        // Issuer wants to unauthorize the holder, unset lsfMPTAuthorized on
        // their MPToken
        if (txFlags & tfMPTUnauthorize)
            flagsOut &= ~lsfMPTAuthorized;
        // Issuer wants to authorize a holder, set lsfMPTAuthorized on their
        // MPToken
        else
            flagsOut |= lsfMPTAuthorized;

        if (flagsIn != flagsOut)
            sleMpt->setFieldU32(sfFlags, flagsOut);

        view().update(sleMpt);
        return tesSUCCESS;
    }

    // If the account that submitted the tx is a holder
    // Note: `account_` is holder's account
    //       `holderID` is NOT used
    if (holderID)
        return tecINTERNAL;

    // When a holder wants to unauthorize/delete a MPT, the ledger must
    //      - delete mptokenKey from both owner and mpt directories
    //      - delete the MPToken
    if (txFlags & tfMPTUnauthorize)
    {
        auto const mptokenKey = keylet::mptoken(mptIssuanceID, account_);
        auto const sleMpt = view().peek(mptokenKey);
        if (!sleMpt)
            return tecINTERNAL;

        if (!view().dirRemove(
                keylet::ownerDir(account_),
                (*sleMpt)[sfOwnerNode],
                sleMpt->key(),
                false))
            return tecINTERNAL;

        if (!view().dirRemove(
                keylet::mpt_dir(mptIssuanceID),
                (*sleMpt)[sfMPTokenNode],
                sleMpt->key(),
                false))
            return tecINTERNAL;

        adjustOwnerCount(
            view(), sleAcct, -1, beast::Journal{beast::Journal::getNullSink()});

        view().erase(sleMpt);
        return tesSUCCESS;
    }

    // A potential holder wants to authorize/hold a mpt, the ledger must:
    //      - add the new mptokenKey to both the owner and mpt directries
    //      - create the MPToken object for the holder
    std::uint32_t const uOwnerCount = sleAcct->getFieldU32(sfOwnerCount);
    XRPAmount const reserveCreate(
        (uOwnerCount < 2) ? XRPAmount(beast::zero)
                          : view().fees().accountReserve(uOwnerCount + 1));

    if (mPriorBalance < reserveCreate)
        return tecINSUFFICIENT_RESERVE;

    auto const mptokenKey = keylet::mptoken(mptIssuanceID, account_);

    auto const ownerNode = view().dirInsert(
        keylet::ownerDir(account_), mptokenKey, describeOwnerDir(account_));

    if (!ownerNode)
        return tecDIR_FULL;

    auto const mptNode = view().dirInsert(
        keylet::mpt_dir(mptIssuanceID),
        mptokenKey,
        [&mptIssuanceID](std::shared_ptr<SLE> const& sle) {
            (*sle)[sfMPTokenIssuanceID] = mptIssuanceID;
        });

    if (!mptNode)
        return tecDIR_FULL;

    auto mptoken = std::make_shared<SLE>(mptokenKey);
    (*mptoken)[sfAccount] = account_;
    (*mptoken)[sfMPTokenIssuanceID] = mptIssuanceID;
    (*mptoken)[sfFlags] = 0;
    (*mptoken)[sfMPTAmount] = 0;
    (*mptoken)[sfOwnerNode] = *ownerNode;
    (*mptoken)[sfMPTokenNode] = *mptNode;
    view().insert(mptoken);

    // Update owner count.
    adjustOwnerCount(view(), sleAcct, 1, j_);

    return tesSUCCESS;
}

}  // namespace ripple
