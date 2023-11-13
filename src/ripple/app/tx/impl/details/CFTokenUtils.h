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

#ifndef RIPPLE_TX_IMPL_DETAILS_CFTOKENUTILS_H_INCLUDED
#define RIPPLE_TX_IMPL_DETAILS_CFTOKENUTILS_H_INCLUDED

#include <ripple/app/tx/impl/details/PageUtils.h>
#include <ripple/basics/base_uint.h>
#include <ripple/basics/tagged_integer.h>
#include <ripple/ledger/ApplyView.h>
#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/TER.h>
#include <ripple/protocol/cft.h>

namespace ripple {

namespace nft {

/** Finds the specified token in the owner's token directory. */
std::optional<STObject>
findToken(
    ReadView const& view,
    AccountID const& owner,
    uint256 const& nftokenID);

std::optional<TokenAndPage>
findTokenAndPage(
    ApplyView& view,
    AccountID const& owner,
    uint256 const& nftokenID);

/** Insert the token in the owner's token directory. */
TER
insertToken(ApplyView& view, AccountID owner, STObject&& nft);

/** Remove the token from the owner's token directory. */
TER
removeToken(ApplyView& view, AccountID const& owner, uint256 const& nftokenID);

TER
removeToken(
    ApplyView& view,
    AccountID const& owner,
    uint256 const& nftokenID,
    std::shared_ptr<SLE>&& page);

bool
compareTokens(uint256 const& a, uint256 const& b);

}  // namespace nft

}  // namespace ripple

#endif  // RIPPLE_TX_IMPL_DETAILS_CFTOKENUTILS_H_INCLUDED
