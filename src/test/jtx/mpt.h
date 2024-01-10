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

#ifndef RIPPLE_TEST_JTX_MPT_H_INCLUDED
#define RIPPLE_TEST_JTX_MPT_H_INCLUDED

#include <test/jtx.h>
#include <test/jtx/ter.h>
#include <test/jtx/txflags.h>

#include <ripple/protocol/UintTypes.h>

namespace ripple {
namespace test {
namespace jtx {

namespace {
using AccountP = Account const*;
}

struct MPTConstr
{
    std::vector<AccountP> holders = {};
    PrettyAmount const& xrp = XRP(10'000);
    PrettyAmount const& xrpHolders = XRP(10'000);
    bool fund = true;
    bool close = true;
};

struct MPTCreate
{
    std::optional<std::uint64_t> maxAmt = std::nullopt;
    std::optional<std::uint8_t> assetScale = std::nullopt;
    std::optional<std::uint16_t> transferFee = std::nullopt;
    std::optional<std::string> metadata = std::nullopt;
    std::optional<std::uint32_t> ownerCount = std::nullopt;
    bool fund = true;
    std::uint32_t flags = 0;
    std::optional<TER> err = std::nullopt;
};

struct MPTDestroy
{
    AccountP issuer = nullptr;
    std::optional<uint192> id = std::nullopt;
    std::optional<std::uint32_t> ownerCount = std::nullopt;
    std::uint32_t flags = 0;
    std::optional<TER> err = std::nullopt;
};

struct MPTAuthorize
{
    AccountP account = nullptr;
    AccountP holder = nullptr;
    std::optional<std::uint32_t> ownerCount = std::nullopt;
    std::uint32_t flags = 0;
    std::optional<TER> err = std::nullopt;
};

struct MPTSet
{
    AccountP account = nullptr;
    AccountP holder = nullptr;
    std::optional<uint192> id = std::nullopt;
    std::optional<std::uint32_t> ownerCount = std::nullopt;
    std::uint32_t flags = 0;
    std::optional<TER> err = std::nullopt;
};

class MPTTester
{
    Env& env_;
    Account const& issuer_;
    std::unordered_map<std::string, AccountP> const holders_;
    std::optional<std::uint32_t> sequence_;
    std::optional<uint192> id_;
    std::optional<uint256> issuanceID_;
    std::optional<ripple::MPT> mpt_;
    bool close_;

public:
    MPTTester(Env& env, Account const& issuer, MPTConstr const& constr = {});

    void
    create(MPTCreate const& arg = MPTCreate{});

    void
    destroy(MPTDestroy const& arg = MPTDestroy{});

    void
    authorize(MPTAuthorize const& arg = MPTAuthorize{});

    void
    set(MPTSet const& set = {});

    [[nodiscard]] bool
    checkMPTokenAmount(Account const& holder, std::uint64_t expectedAmount)
        const;

    [[nodiscard]] bool
    checkMPTokenOutstandingAmount(std::uint64_t expectedAmount) const;

    [[nodiscard]] bool
    checkFlags(uint32_t const expectedFlags, AccountP holder = nullptr) const;

    Account const&
    issuer() const
    {
        return issuer_;
    }
    Account const&
    holder(std::string const& h) const;

    void
    pay(Account const& src,
        Account const& dest,
        std::uint64_t amount,
        std::optional<TER> err = std::nullopt);

    PrettyAmount
    mpt(std::uint64_t amount) const;

    uint256 const&
    issuanceKey() const
    {
        assert(issuanceID_);
        return *issuanceID_;
    }

    uint192 const&
    issuanceID() const
    {
        assert(id_);
        return *id_;
    }

private:
    using SLEP = std::shared_ptr<SLE const>;
    bool
    forObject(
        std::function<bool(SLEP const& sle)> const& cb,
        AccountP holder = nullptr) const;

    template <typename A>
    void
    submit(A const& arg, Json::Value const& jv)
    {
        if (arg.err)
        {
            if (arg.flags)
                env_(jv, txflags(arg.flags), ter(*arg.err));
            else
                env_(jv, ter(*arg.err));
        }
        else if (arg.flags)
            env_(jv, txflags(arg.flags));
        else
            env_(jv);
        if constexpr (std::is_same_v<A, MPTCreate>)
        {
            if (env_.ter() != tesSUCCESS)
            {
                sequence_.reset();
                id_.reset();
                issuanceID_.reset();
                mpt_.reset();
            }
        }
        if (close_)
            env_.close();
        if (arg.ownerCount)
            env_.require(owners(issuer_, *arg.ownerCount));
    }

    std::unordered_map<std::string, AccountP>
    makeHolders(std::vector<AccountP> const& holders);
};

}  // namespace jtx
}  // namespace test
}  // namespace ripple

#endif
