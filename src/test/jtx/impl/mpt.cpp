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

#include <test/jtx/mpt.h>

#include <ripple/protocol/jss.h>

namespace ripple {
namespace test {
namespace jtx {

namespace mpt {

Json::Value
create(jtx::Account const& account)
{
    Json::Value jv;
    jv[sfAccount.jsonName] = account.human();
    jv[sfTransactionType.jsonName] = jss::MPTokenIssuanceCreate;
    return jv;
}

Json::Value
create(
    jtx::Account const& account,
    std::uint32_t const maxAmt,
    std::uint8_t const assetScale,
    std::uint16_t transferFee,
    std::string metadata)
{
    Json::Value jv;
    jv[sfAccount.jsonName] = account.human();
    jv[sfTransactionType.jsonName] = jss::MPTokenIssuanceCreate;
    jv[sfMaximumAmount.jsonName] = maxAmt;
    jv[sfAssetScale.jsonName] = assetScale;
    jv[sfTransferFee.jsonName] = transferFee;
    jv[sfMPTokenMetadata.jsonName] = strHex(metadata);
    return jv;
}

Json::Value
destroy(jtx::Account const& account, uint192 const& id)
{
    Json::Value jv;
    jv[sfAccount.jsonName] = account.human();
    jv[sfMPTokenIssuanceID.jsonName] = to_string(id);
    jv[sfTransactionType.jsonName] = jss::MPTokenIssuanceDestroy;
    return jv;
}

Json::Value
authorize(
    jtx::Account const& account,
    uint192 const& issuanceID,
    std::optional<jtx::Account> const& holder)
{
    Json::Value jv;
    jv[sfAccount.jsonName] = account.human();
    jv[sfTransactionType.jsonName] = jss::MPTokenAuthorize;
    jv[sfMPTokenIssuanceID.jsonName] = to_string(issuanceID);
    if (holder)
        jv[sfMPTokenHolder.jsonName] = holder->human();

    return jv;
}

Json::Value
set(jtx::Account const& account,
    uint192 const& issuanceID,
    std::optional<jtx::Account> const& holder)
{
    Json::Value jv;
    jv[sfAccount.jsonName] = account.human();
    jv[sfTransactionType.jsonName] = jss::MPTokenIssuanceSet;
    jv[sfMPTokenIssuanceID.jsonName] = to_string(issuanceID);
    if (holder)
        jv[sfMPTokenHolder.jsonName] = holder->human();

    return jv;
}

}  // namespace mpt

}  // namespace jtx
}  // namespace test
}  // namespace ripple
