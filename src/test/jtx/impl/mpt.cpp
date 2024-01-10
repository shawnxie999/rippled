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

static std::array<std::uint8_t, 8>
uint64ToByteArray(std::uint64_t value)
{
    value = boost::endian::native_to_big(value);
    std::array<std::uint8_t, 8> result;
    std::memcpy(result.data(), &value, sizeof(value));
    return result;
}

std::unordered_map<std::string, AccountP>
MPTTester::makeHolders(std::vector<AccountP> const& holders)
{
    std::unordered_map<std::string, AccountP> accounts;
    for (auto const& h : holders)
    {
        assert(h && holders_.find(h->human()) == accounts.cend());
        accounts.emplace(h->human(), h);
    }
    return accounts;
}

MPTTester::MPTTester(Env& env, Account const& issuer, MPTConstr const& arg)
    : env_(env)
    , issuer_(issuer)
    , holders_(makeHolders(arg.holders))
    , close_(arg.close)
{
    if (arg.fund)
    {
        env_.fund(arg.xrp, issuer_);
        for (auto it : holders_)
            env_.fund(arg.xrpHolders, *it.second);
    }
    if (close_)
        env.close();
    if (arg.fund)
    {
        env_.require(owners(issuer_, 0));
        for (auto it : holders_)
        {
            assert(issuer_.id() != it.second->id());
            env_.require(owners(*it.second, 0));
        }
    }
}

void
MPTTester::create(const MPTCreate& arg)
{
    if (sequence_)
        Throw<std::runtime_error>("MPT can't be reused");
    sequence_ = env_.seq(issuer_);
    id_ = getMptID(issuer_.id(), *sequence_);
    issuanceID_ = keylet::mptIssuance(*id_).key;
    mpt_ = std::make_pair(*sequence_, issuer_.id());
    Json::Value jv;
    jv[sfAccount.jsonName] = issuer_.human();
    jv[sfTransactionType.jsonName] = jss::MPTokenIssuanceCreate;
    if (arg.assetScale)
        jv[sfAssetScale.jsonName] = *arg.assetScale;
    if (arg.transferFee)
        jv[sfTransferFee.jsonName] = *arg.transferFee;
    if (arg.metadata)
        jv[sfMPTokenMetadata.jsonName] = strHex(*arg.metadata);

    // convert maxAmt to hex string, since json doesn't accept 64-bit int
    if (arg.maxAmt)
        jv[sfMaximumAmount.jsonName] = strHex(uint64ToByteArray(*arg.maxAmt));
    submit(arg, jv);
}

void
MPTTester::destroy(MPTDestroy const& arg)
{
    Json::Value jv;
    if (arg.issuer)
        jv[sfAccount.jsonName] = arg.issuer->human();
    else
        jv[sfAccount.jsonName] = issuer_.human();
    if (arg.id)
        jv[sfMPTokenIssuanceID.jsonName] = to_string(*arg.id);
    else
        jv[sfMPTokenIssuanceID.jsonName] = to_string(*id_);
    jv[sfTransactionType.jsonName] = jss::MPTokenIssuanceDestroy;
    submit(arg, jv);
}

Account const&
MPTTester::holder(std::string const& holder_) const
{
    auto const& it = holders_.find(holder_);
    assert(it != holders_.cend());
    if (it == holders_.cend())
        Throw<std::runtime_error>("Holder is not found");
    return *it->second;
}

void
MPTTester::authorize(MPTAuthorize const& arg)
{
    Json::Value jv;
    if (arg.account)
        jv[sfAccount.jsonName] = arg.account->human();
    else
        jv[sfAccount.jsonName] = issuer_.human();
    jv[sfTransactionType.jsonName] = jss::MPTokenAuthorize;
    jv[sfMPTokenIssuanceID.jsonName] = to_string(*id_);
    if (arg.holder)
        jv[sfMPTokenHolder.jsonName] = arg.holder->human();
    submit(arg, jv);
}

void
MPTTester::set(MPTSet const& arg)
{
    Json::Value jv;
    if (arg.account)
        jv[sfAccount.jsonName] = arg.account->human();
    else
        jv[sfAccount.jsonName] = issuer_.human();
    jv[sfTransactionType.jsonName] = jss::MPTokenIssuanceSet;
    if (arg.id)
        jv[sfMPTokenIssuanceID.jsonName] = to_string(*arg.id);
    else
        jv[sfMPTokenIssuanceID.jsonName] = to_string(*id_);
    if (arg.holder)
        jv[sfMPTokenHolder.jsonName] = arg.holder->human();
    submit(arg, jv);
}

bool
MPTTester::forObject(
    std::function<bool(SLEP const& sle)> const& cb,
    AccountP holder_) const
{
    auto const key = [&]() {
        if (holder_)
            return keylet::mptoken(*issuanceID_, holder_->id());
        return keylet::mptIssuance(*issuanceID_);
    }();
    if (auto const sle = env_.le(key))
        return cb(sle);
    return false;
}

[[nodiscard]] bool
MPTTester::checkMPTokenAmount(
    Account const& holder_,
    std::uint64_t expectedAmount) const
{
    return forObject(
        [&](SLEP const& sle) { return expectedAmount == (*sle)[sfMPTAmount]; },
        &holder_);
}

[[nodiscard]] bool
MPTTester::checkMPTokenOutstandingAmount(std::uint64_t expectedAmount) const
{
    return forObject([&](SLEP const& sle) {
        return expectedAmount == (*sle)[sfOutstandingAmount];
    });
}

[[nodiscard]] bool
MPTTester::checkFlags(uint32_t const expectedFlags, AccountP holder_) const
{
    return forObject(
        [&](SLEP const& sle) { return expectedFlags == sle->getFlags(); },
        holder_);
}

void
MPTTester::pay(
    Account const& src,
    Account const& dest,
    std::uint64_t amount,
    std::optional<TER> err)
{
    assert(mpt_);
    if (err)
        env_(jtx::pay(src, dest, mpt(amount)), ter(*err));
    else
        env_(jtx::pay(src, dest, mpt(amount)));
    if (close_)
        env_.close();
}

PrettyAmount
MPTTester::mpt(std::uint64_t amount) const
{
    assert(mpt_);
    return ripple::test::jtx::MPT(issuer_.name(), *mpt_)(amount);
}

}  // namespace jtx
}  // namespace test
}  // namespace ripple
