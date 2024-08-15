//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

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

#include <xrpld/app/paths/RippleCalc.h>
#include <xrpld/app/tx/detail/Payment.h>
#include <xrpld/ledger/View.h>
#include <xrpl/basics/Log.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/TxFlags.h>
#include <xrpl/protocol/jss.h>
#include <xrpl/protocol/st.h>

namespace ripple {

template <ValidAmountType TDel>
static TxConsequences
makeTxConsequencesHelper(PreflightContext const& ctx);

template <>
TxConsequences
makeTxConsequencesHelper<STAmount>(PreflightContext const& ctx)
{
    auto calculateMaxXRPSpend = [](STTx const& tx) -> XRPAmount {
        auto const maxAmount = tx.isFieldPresent(sfSendMax)
            ? tx[sfSendMax]
            : get<STAmount>(tx[sfAmount]);

        // If there's no sfSendMax in XRP, and the sfAmount isn't
        // in XRP, then the transaction does not spend XRP.
        return maxAmount.native() ? maxAmount.xrp() : beast::zero;
    };

    return TxConsequences{ctx.tx, calculateMaxXRPSpend(ctx.tx)};
}

template <>
TxConsequences
makeTxConsequencesHelper<STMPTAmount>(PreflightContext const& ctx)
{
    return TxConsequences{ctx.tx, beast::zero};
}

template <ValidAmountType TDel>
static NotTEC
preflightHelper(PreflightContext const& ctx);

template <>
NotTEC
preflightHelper<STAmount>(PreflightContext const& ctx)
{
    if (auto const ret = preflight1(ctx); !isTesSuccess(ret))
        return ret;

    auto& tx = ctx.tx;
    auto& j = ctx.j;

    std::uint32_t const uTxFlags = tx.getFlags();

    if (uTxFlags & tfPaymentMask)
    {
        JLOG(j.trace()) << "Malformed transaction: "
                        << "Invalid flags set.";
        return temINVALID_FLAG;
    }

    bool const partialPaymentAllowed = uTxFlags & tfPartialPayment;
    bool const limitQuality = uTxFlags & tfLimitQuality;
    bool const defaultPathsAllowed = !(uTxFlags & tfNoRippleDirect);
    bool const bPaths = tx.isFieldPresent(sfPaths);
    bool const bMax = tx.isFieldPresent(sfSendMax);

    STAmount const saDstAmount(get<STAmount>(tx.getFieldAmount(sfAmount)));

    STAmount maxSourceAmount;
    auto const account = tx.getAccountID(sfAccount);

    if (bMax)
        maxSourceAmount = tx.getFieldAmount(sfSendMax);
    else if (saDstAmount.native())
        maxSourceAmount = saDstAmount;
    else
        maxSourceAmount = STAmount(
            {saDstAmount.getCurrency(), account},
            saDstAmount.mantissa(),
            saDstAmount.exponent(),
            saDstAmount < beast::zero);

    auto const& uSrcCurrency = maxSourceAmount.getCurrency();
    auto const& uDstCurrency = saDstAmount.getCurrency();

    // isZero() is XRP.  FIX!
    bool const bXRPDirect = uSrcCurrency.isZero() && uDstCurrency.isZero();

    if (!isLegalNet(saDstAmount) || !isLegalNet(maxSourceAmount))
        return temBAD_AMOUNT;

    auto const uDstAccountID = tx.getAccountID(sfDestination);

    if (!uDstAccountID)
    {
        JLOG(j.trace()) << "Malformed transaction: "
                        << "Payment destination account not specified.";
        return temDST_NEEDED;
    }
    if (bMax && maxSourceAmount <= beast::zero)
    {
        JLOG(j.trace()) << "Malformed transaction: "
                        << "bad max amount: " << maxSourceAmount.getFullText();
        return temBAD_AMOUNT;
    }
    if (saDstAmount <= beast::zero)
    {
        JLOG(j.trace()) << "Malformed transaction: "
                        << "bad dst amount: " << saDstAmount.getFullText();
        return temBAD_AMOUNT;
    }
    if (badCurrency() == uSrcCurrency || badCurrency() == uDstCurrency)
    {
        JLOG(j.trace()) << "Malformed transaction: "
                        << "Bad currency.";
        return temBAD_CURRENCY;
    }
    if (account == uDstAccountID && uSrcCurrency == uDstCurrency && !bPaths)
    {
        // You're signing yourself a payment.
        // If bPaths is true, you might be trying some arbitrage.
        JLOG(j.trace()) << "Malformed transaction: "
                        << "Redundant payment from " << to_string(account)
                        << " to self without path for "
                        << to_string(uDstCurrency);
        return temREDUNDANT;
    }
    if (bXRPDirect && bMax)
    {
        // Consistent but redundant transaction.
        JLOG(j.trace()) << "Malformed transaction: "
                        << "SendMax specified for XRP to XRP.";
        return temBAD_SEND_XRP_MAX;
    }
    if (bXRPDirect && bPaths)
    {
        // XRP is sent without paths.
        JLOG(j.trace()) << "Malformed transaction: "
                        << "Paths specified for XRP to XRP.";
        return temBAD_SEND_XRP_PATHS;
    }
    if (bXRPDirect && partialPaymentAllowed)
    {
        // Consistent but redundant transaction.
        JLOG(j.trace()) << "Malformed transaction: "
                        << "Partial payment specified for XRP to XRP.";
        return temBAD_SEND_XRP_PARTIAL;
    }
    if (bXRPDirect && limitQuality)
    {
        // Consistent but redundant transaction.
        JLOG(j.trace()) << "Malformed transaction: "
                        << "Limit quality specified for XRP to XRP.";
        return temBAD_SEND_XRP_LIMIT;
    }
    if (bXRPDirect && !defaultPathsAllowed)
    {
        // Consistent but redundant transaction.
        JLOG(j.trace()) << "Malformed transaction: "
                        << "No ripple direct specified for XRP to XRP.";
        return temBAD_SEND_XRP_NO_DIRECT;
    }

    auto const deliverMin = tx[~sfDeliverMin];
    if (deliverMin)
    {
        if (!partialPaymentAllowed)
        {
            JLOG(j.trace()) << "Malformed transaction: Partial payment not "
                               "specified for "
                            << jss::DeliverMin.c_str() << ".";
            return temBAD_AMOUNT;
        }

        auto const dMin = *deliverMin;
        if (!isLegalNet(dMin) || dMin <= beast::zero)
        {
            JLOG(j.trace())
                << "Malformed transaction: Invalid " << jss::DeliverMin.c_str()
                << " amount. " << dMin.getFullText();
            return temBAD_AMOUNT;
        }
        if (dMin.issue() != saDstAmount.issue())
        {
            JLOG(j.trace())
                << "Malformed transaction: Dst issue differs "
                   "from "
                << jss::DeliverMin.c_str() << ". " << dMin.getFullText();
            return temBAD_AMOUNT;
        }
        if (dMin > saDstAmount)
        {
            JLOG(j.trace())
                << "Malformed transaction: Dst amount less than "
                << jss::DeliverMin.c_str() << ". " << dMin.getFullText();
            return temBAD_AMOUNT;
        }
    }

    return preflight2(ctx);
}

template <>
NotTEC
preflightHelper<STMPTAmount>(PreflightContext const& ctx)
{
    if (auto const ret = preflight1(ctx); !isTesSuccess(ret))
        return ret;

    if (!ctx.rules.enabled(featureMPTokensV1))
        return temDISABLED;

    if (ctx.tx.isFieldPresent(sfDeliverMin) ||
        ctx.tx.isFieldPresent(sfSendMax) || ctx.tx.isFieldPresent(sfPaths))
        return temMALFORMED;

    auto& tx = ctx.tx;
    auto& j = ctx.j;

    std::uint32_t const uTxFlags = tx.getFlags();

    if (uTxFlags & tfPaymentMask)
    {
        JLOG(j.trace()) << "Malformed transaction: "
                        << "Invalid flags set.";
        return temINVALID_FLAG;
    }

    STMPTAmount const saDstAmount(
        get<STMPTAmount>(tx.getFieldAmount(sfAmount)));

    auto const account = tx.getAccountID(sfAccount);

    auto const& uDstCurrency = saDstAmount.getCurrency();

    auto const uDstAccountID = tx.getAccountID(sfDestination);

    if (!uDstAccountID)
    {
        JLOG(j.trace()) << "Malformed transaction: "
                        << "Payment destination account not specified.";
        return temDST_NEEDED;
    }
    if (saDstAmount <= beast::zero)
    {
        JLOG(j.trace()) << "Malformed transaction: "
                        << "bad dst amount: " << saDstAmount.getFullText();
        return temBAD_AMOUNT;
    }
    if (badMPT() == uDstCurrency)
    {
        JLOG(j.trace()) << "Malformed transaction: "
                        << "Bad asset.";
        return temBAD_CURRENCY;
    }
    if (account == uDstAccountID)
    {
        // You're signing yourself a payment.
        JLOG(j.trace()) << "Malformed transaction: "
                        << "Redundant payment from " << to_string(account)
                        << " to self without path for "
                        << to_string(uDstCurrency);
        return temREDUNDANT;
    }
    if (uTxFlags & (tfPartialPayment | tfLimitQuality | tfNoRippleDirect))
    {
        JLOG(j.trace()) << "Malformed transaction: invalid MPT flags: "
                        << uTxFlags;
        return temMALFORMED;
    }

    return preflight2(ctx);
}

template <ValidAmountType TDel>
static TER
preclaimHelper(
    PreclaimContext const& ctx,
    std::size_t maxPathSize,
    std::size_t maxPathLength);

template <>
TER
preclaimHelper<STAmount>(
    PreclaimContext const& ctx,
    std::size_t maxPathSize,
    std::size_t maxPathLength)
{
    // Ripple if source or destination is non-native or if there are paths.
    std::uint32_t const uTxFlags = ctx.tx.getFlags();
    bool const partialPaymentAllowed = uTxFlags & tfPartialPayment;
    auto const paths = ctx.tx.isFieldPresent(sfPaths);
    auto const sendMax = ctx.tx[~sfSendMax];

    AccountID const uDstAccountID(ctx.tx[sfDestination]);
    STAmount const saDstAmount(get<STAmount>(ctx.tx[sfAmount]));

    auto const k = keylet::account(uDstAccountID);
    auto const sleDst = ctx.view.read(k);

    if (!sleDst)
    {
        // Destination account does not exist.
        if (!saDstAmount.native())
        {
            JLOG(ctx.j.trace())
                << "Delay transaction: Destination account does not exist.";

            // Another transaction could create the account and then this
            // transaction would succeed.
            return tecNO_DST;
        }
        else if (ctx.view.open() && partialPaymentAllowed)
        {
            // You cannot fund an account with a partial payment.
            // Make retry work smaller, by rejecting this.
            JLOG(ctx.j.trace()) << "Delay transaction: Partial payment not "
                                   "allowed to create account.";

            // Another transaction could create the account and then this
            // transaction would succeed.
            return telNO_DST_PARTIAL;
        }
        else if (saDstAmount < STAmount(ctx.view.fees().accountReserve(0)))
        {
            // accountReserve is the minimum amount that an account can have.
            // Reserve is not scaled by load.
            JLOG(ctx.j.trace())
                << "Delay transaction: Destination account does not exist. "
                << "Insufficent payment to create account.";

            // TODO: dedupe
            // Another transaction could create the account and then this
            // transaction would succeed.
            return tecNO_DST_INSUF_XRP;
        }
    }
    else if (
        (sleDst->getFlags() & lsfRequireDestTag) &&
        !ctx.tx.isFieldPresent(sfDestinationTag))
    {
        // The tag is basically account-specific information we don't
        // understand, but we can require someone to fill it in.

        // We didn't make this test for a newly-formed account because there's
        // no way for this field to be set.
        JLOG(ctx.j.trace())
            << "Malformed transaction: DestinationTag required.";

        return tecDST_TAG_NEEDED;
    }

    // Payment with at least one intermediate step and uses transitive balances.
    if ((paths || sendMax || !saDstAmount.native()) && ctx.view.open())
    {
        STPathSet const& paths = ctx.tx.getFieldPathSet(sfPaths);

        if (paths.size() > maxPathSize ||
            std::any_of(paths.begin(), paths.end(), [&](STPath const& path) {
                return path.size() > maxPathLength;
            }))
        {
            return telBAD_PATH_COUNT;
        }
    }

    return tesSUCCESS;
}

template <>
TER
preclaimHelper<STMPTAmount>(
    PreclaimContext const& ctx,
    std::size_t,
    std::size_t)
{
    AccountID const uDstAccountID(ctx.tx[sfDestination]);

    auto const k = keylet::account(uDstAccountID);
    auto const sleDst = ctx.view.read(k);

    if (!sleDst)
    {
        JLOG(ctx.j.trace())
            << "Delay transaction: Destination account does not exist.";

        // Another transaction could create the account and then this
        // transaction would succeed.
        return tecNO_DST;
    }
    else if (
        (sleDst->getFlags() & lsfRequireDestTag) &&
        !ctx.tx.isFieldPresent(sfDestinationTag))
    {
        // The tag is basically account-specific information we don't
        // understand, but we can require someone to fill it in.

        // We didn't make this test for a newly-formed account because there's
        // no way for this field to be set.
        JLOG(ctx.j.trace())
            << "Malformed transaction: DestinationTag required.";

        return tecDST_TAG_NEEDED;
    }

    return tesSUCCESS;
}

template <ValidAmountType TDel>
static TER
applyHelper(
    ApplyContext& ctx,
    XRPAmount const& priorBalance,
    XRPAmount const& sourceBalance);

template <>
TER
applyHelper<STAmount>(
    ApplyContext& ctx,
    XRPAmount const& priorBalance,
    XRPAmount const& sourceBalance)
{
    AccountID const account = ctx.tx[sfAccount];
    auto const deliverMin = ctx.tx[~sfDeliverMin];

    // Ripple if source or destination is non-native or if there are paths.
    std::uint32_t const uTxFlags = ctx.tx.getFlags();
    bool const partialPaymentAllowed = uTxFlags & tfPartialPayment;
    bool const limitQuality = uTxFlags & tfLimitQuality;
    bool const defaultPathsAllowed = !(uTxFlags & tfNoRippleDirect);
    auto const paths = ctx.tx.isFieldPresent(sfPaths);
    auto const sendMax = ctx.tx[~sfSendMax];

    AccountID const uDstAccountID(ctx.tx.getAccountID(sfDestination));
    STAmount const saDstAmount(get<STAmount>(ctx.tx.getFieldAmount(sfAmount)));
    STAmount maxSourceAmount;
    if (sendMax)
        maxSourceAmount = *sendMax;
    else if (saDstAmount.native())
        maxSourceAmount = saDstAmount;
    else
        maxSourceAmount = STAmount(
            {saDstAmount.getCurrency(), account},
            saDstAmount.mantissa(),
            saDstAmount.exponent(),
            saDstAmount < beast::zero);

    JLOG(ctx.journal.trace())
        << "maxSourceAmount=" << maxSourceAmount.getFullText()
        << " saDstAmount=" << saDstAmount.getFullText();

    // Open a ledger for editing.
    auto const k = keylet::account(uDstAccountID);
    SLE::pointer sleDst = ctx.view().peek(k);

    if (!sleDst)
    {
        std::uint32_t const seqno{
            ctx.view().rules().enabled(featureDeletableAccounts)
                ? ctx.view().seq()
                : 1};

        // Create the account.
        sleDst = std::make_shared<SLE>(k);
        sleDst->setAccountID(sfAccount, uDstAccountID);
        sleDst->setFieldU32(sfSequence, seqno);

        ctx.view().insert(sleDst);
    }
    else
    {
        // Tell the engine that we are intending to change the destination
        // account.  The source account gets always charged a fee so it's always
        // marked as modified.
        ctx.view().update(sleDst);
    }

    // Determine whether the destination requires deposit authorization.
    bool const reqDepositAuth = sleDst->getFlags() & lsfDepositAuth &&
        ctx.view().rules().enabled(featureDepositAuth);

    bool const depositPreauth =
        ctx.view().rules().enabled(featureDepositPreauth);

    bool const bRipple = paths || sendMax || !saDstAmount.native();

    // If the destination has lsfDepositAuth set, then only direct XRP
    // payments (no intermediate steps) are allowed to the destination.
    if (!depositPreauth && bRipple && reqDepositAuth)
        return tecNO_PERMISSION;

    if (bRipple)
    {
        // Ripple payment with at least one intermediate step and uses
        // transitive balances.

        if (depositPreauth && reqDepositAuth)
        {
            // If depositPreauth is enabled, then an account that requires
            // authorization has two ways to get an IOU Payment in:
            //  1. If Account == Destination, or
            //  2. If Account is deposit preauthorized by destination.
            if (uDstAccountID != account)
            {
                if (!ctx.view().exists(
                        keylet::depositPreauth(uDstAccountID, account)))
                    return tecNO_PERMISSION;
            }
        }

        path::RippleCalc::Input rcInput;
        rcInput.partialPaymentAllowed = partialPaymentAllowed;
        rcInput.defaultPathsAllowed = defaultPathsAllowed;
        rcInput.limitQuality = limitQuality;
        rcInput.isLedgerOpen = ctx.view().open();

        path::RippleCalc::Output rc;
        {
            PaymentSandbox pv(&ctx.view());
            JLOG(ctx.journal.debug()) << "Entering RippleCalc in payment: "
                                      << ctx.tx.getTransactionID();
            rc = path::RippleCalc::rippleCalculate(
                pv,
                maxSourceAmount,
                saDstAmount,
                uDstAccountID,
                account,
                ctx.tx.getFieldPathSet(sfPaths),
                ctx.app.logs(),
                &rcInput);
            // VFALCO NOTE We might not need to apply, depending
            //             on the TER. But always applying *should*
            //             be safe.
            pv.apply(ctx.rawView());
        }

        // TODO: is this right?  If the amount is the correct amount, was
        // the delivered amount previously set?
        if (rc.result() == tesSUCCESS && rc.actualAmountOut != saDstAmount)
        {
            if (deliverMin && rc.actualAmountOut < *deliverMin)
                rc.setResult(tecPATH_PARTIAL);
            else
                ctx.deliver(rc.actualAmountOut);
        }

        auto terResult = rc.result();

        // Because of its overhead, if RippleCalc
        // fails with a retry code, claim a fee
        // instead. Maybe the user will be more
        // careful with their path spec next time.
        if (isTerRetry(terResult))
            terResult = tecPATH_DRY;
        return terResult;
    }

    assert(saDstAmount.native());

    // Direct XRP payment.

    auto const sleSrc = ctx.view().peek(keylet::account(account));
    if (!sleSrc)
        return tefINTERNAL;

    // uOwnerCount is the number of entries in this ledger for this
    // account that require a reserve.
    auto const uOwnerCount = sleSrc->getFieldU32(sfOwnerCount);

    // This is the total reserve in drops.
    auto const reserve = ctx.view().fees().accountReserve(uOwnerCount);

    // mPriorBalance is the balance on the sending account BEFORE the
    // fees were charged. We want to make sure we have enough reserve
    // to send. Allow final spend to use reserve for fee.
    auto const mmm = std::max(reserve, ctx.tx.getFieldAmount(sfFee).xrp());

    if (priorBalance < saDstAmount.xrp() + mmm)
    {
        // Vote no. However the transaction might succeed, if applied in
        // a different order.
        JLOG(ctx.journal.trace()) << "Delay transaction: Insufficient funds: "
                                  << " " << to_string(priorBalance) << " / "
                                  << to_string(saDstAmount.xrp() + mmm) << " ("
                                  << to_string(reserve) << ")";

        return tecUNFUNDED_PAYMENT;
    }

    // AMMs can never receive an XRP payment.
    // Must use AMMDeposit transaction instead.
    if (sleDst->isFieldPresent(sfAMMID))
        return tecNO_PERMISSION;

    // The source account does have enough money.  Make sure the
    // source account has authority to deposit to the destination.
    if (reqDepositAuth)
    {
        // If depositPreauth is enabled, then an account that requires
        // authorization has three ways to get an XRP Payment in:
        //  1. If Account == Destination, or
        //  2. If Account is deposit preauthorized by destination, or
        //  3. If the destination's XRP balance is
        //    a. less than or equal to the base reserve and
        //    b. the deposit amount is less than or equal to the base reserve,
        // then we allow the deposit.
        //
        // Rule 3 is designed to keep an account from getting wedged
        // in an unusable state if it sets the lsfDepositAuth flag and
        // then consumes all of its XRP.  Without the rule if an
        // account with lsfDepositAuth set spent all of its XRP, it
        // would be unable to acquire more XRP required to pay fees.
        //
        // We choose the base reserve as our bound because it is
        // a small number that seldom changes but is always sufficient
        // to get the account un-wedged.
        if (uDstAccountID != account)
        {
            if (!ctx.view().exists(
                    keylet::depositPreauth(uDstAccountID, account)))
            {
                // Get the base reserve.
                XRPAmount const dstReserve{ctx.view().fees().accountReserve(0)};

                if (saDstAmount > dstReserve ||
                    sleDst->getFieldAmount(sfBalance) > dstReserve)
                    return tecNO_PERMISSION;
            }
        }
    }

    // Do the arithmetic for the transfer and make the ledger change.
    sleSrc->setFieldAmount(sfBalance, sourceBalance - saDstAmount);
    sleDst->setFieldAmount(
        sfBalance, sleDst->getFieldAmount(sfBalance) + saDstAmount);

    // Re-arm the password change fee if we can and need to.
    if ((sleDst->getFlags() & lsfPasswordSpent))
        sleDst->clearFlag(lsfPasswordSpent);

    return tesSUCCESS;
}

template <>
TER
applyHelper<STMPTAmount>(ApplyContext& ctx, XRPAmount const&, XRPAmount const&)
{
    auto const account = ctx.tx[sfAccount];

    AccountID const uDstAccountID(ctx.tx.getAccountID(sfDestination));
    auto const saDstAmount(get<STMPTAmount>(ctx.tx.getFieldAmount(sfAmount)));

    JLOG(ctx.journal.trace()) << " saDstAmount=" << saDstAmount.getFullText();

    if (auto const ter = requireAuth(ctx.view(), saDstAmount.issue(), account);
        ter != tesSUCCESS)
        return ter;

    if (auto const ter =
            requireAuth(ctx.view(), saDstAmount.issue(), uDstAccountID);
        ter != tesSUCCESS)
        return ter;

    if (auto const ter = canTransfer(
            ctx.view(), saDstAmount.issue(), account, uDstAccountID);
        ter != tesSUCCESS)
        return ter;

    auto const& mpt = saDstAmount.issue();
    auto const& issuer = mpt.getIssuer();
    // If globally/individually locked then
    //   - can't send between holders
    //   - holder can send back to issuer
    //   - issuer can send to holder
    if (account != issuer && uDstAccountID != issuer &&
        (isFrozen(ctx.view(), account, mpt) ||
         isFrozen(ctx.view(), uDstAccountID, mpt)))
        return tecMPT_LOCKED;

    PaymentSandbox pv(&ctx.view());
    auto const res =
        accountSend(pv, account, uDstAccountID, saDstAmount, ctx.journal);
    pv.apply(ctx.rawView());
    return res;
}

TxConsequences
Payment::makeTxConsequences(PreflightContext const& ctx)
{
    return std::visit(
        [&]<typename TDel>(TDel const&) {
            return makeTxConsequencesHelper<TDel>(ctx);
        },
        ctx.tx[sfAmount].getValue());
}

NotTEC
Payment::preflight(PreflightContext const& ctx)
{
    return std::visit(
        [&]<typename TDel>(TDel const&) { return preflightHelper<TDel>(ctx); },
        ctx.tx[sfAmount].getValue());
}

TER
Payment::preclaim(PreclaimContext const& ctx)
{
    return std::visit(
        [&]<typename TDel>(TDel const&) {
            return preclaimHelper<TDel>(ctx, MaxPathSize, MaxPathLength);
        },
        ctx.tx[sfAmount].getValue());
}

TER
Payment::doApply()
{
    return std::visit(
        [&]<typename TDel>(TDel const&) {
            return applyHelper<TDel>(ctx_, mPriorBalance, mSourceBalance);
        },
        ctx_.tx[sfAmount].getValue());
}

}  // namespace ripple
