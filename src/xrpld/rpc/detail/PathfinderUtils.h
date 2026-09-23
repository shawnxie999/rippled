#pragma once

#include <xrpl/basics/Number.h>
#include <xrpl/protocol/AmountConversions.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/MPTIssue.h>
#include <xrpl/protocol/Protocol.h>
#include <xrpl/protocol/STAmount.h>
#include <xrpl/protocol/SystemParameters.h>

#include <cstdint>

namespace xrpl {

inline STAmount
largestAmount(STAmount const& amt)
{
    return amt.asset().visit(
        [&](Issue const& issue) -> STAmount {
            if (issue.native())
                return kInitialXrp;
            return STAmount(amt.asset(), STAmount::kMaxValue, STAmount::kMaxOffset);
        },
        [&](MPTIssue const&) { return STAmount(amt.asset(), kMaxMpTokenAmount, 0); });
}

inline STAmount
convertAmount(STAmount const& amt, bool all)
{
    if (!all)
        return amt;

    return largestAmount(amt);
};

// Return the smallest amount of useful liquidity for a given amount, and the
// total number of paths we have to evaluate.
inline STAmount
smallestUsefulAmount(STAmount const& amount, int maxPaths)
{
    auto const den = maxPaths + 2;

    // divide() assumes an IOU mantissa, normalized into [1e15, 1e16). An MPT
    // mantissa is the raw int64, so scaling it by 1e17 overflows uint64 above
    // ~1.1e18 and throws.
    if (amount.holds<MPTIssue>())
    {
        return toSTAmount(
            amount.asset(), Number{amount.mpt()} / Number{den}, Number::RoundingMode::ToNearest);
    }

    return divide(amount, STAmount(static_cast<std::uint64_t>(den)), amount.asset());
}

inline bool
convertAllCheck(STAmount const& a)
{
    return a == largestAmount(a);
}

}  // namespace xrpl
