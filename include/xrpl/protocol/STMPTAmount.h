//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2024 Ripple Labs Inc.

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

#ifndef RIPPLE_PROTOCOL_STMPTAMOUNT_H_INCLUDED
#define RIPPLE_PROTOCOL_STMPTAMOUNT_H_INCLUDED

#include <xrpl/basics/CountedObject.h>
#include <xrpl/basics/MPTAmount.h>
#include <xrpl/protocol/MPTIssue.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STBase.h>

namespace ripple {

struct Rate;

class STMPTAmount final : public MPTAmount,
                          public STBase,
                          public CountedObject<STMPTAmount>
{
private:
    MPTIssue issue_;

public:
    static constexpr std::uint64_t cMPToken = 0x2000000000000000ull;

    STMPTAmount(std::uint64_t value, SerialIter& sit, SField const& name);
    STMPTAmount(
        SField const& name,
        MPTIssue const& issue,
        std::int64_t value = 0);
    STMPTAmount(MPTIssue const& issue, std::uint64_t value);
    STMPTAmount(MPTIssue const& issue, std::int64_t value = 0);
    explicit STMPTAmount(value_type value = 0);

    SerializedTypeID
    getSType() const override;

    std::string
    getFullText() const override;

    std::string
    getText() const override;

    Json::Value getJson(JsonOptions) const override;

    void
    add(Serializer& s) const override;

    void
    setJson(Json::Value& elem) const;

    bool
    isEquivalent(const STBase& t) const override;

    bool
    isDefault() const override;

    AccountID const&
    getIssuer() const;

    MPTIssue const&
    issue() const;

    uint192
    getCurrency() const;

    void
    clear();

    void
    clear(MPTIssue const& issue);

    STMPTAmount
    zeroed() const;

    int
    signum() const noexcept;

    STMPTAmount&
    operator+=(STMPTAmount const& other);

    STMPTAmount&
    operator-=(STMPTAmount const& other);

    STMPTAmount
    operator-() const;

    STMPTAmount& operator=(beast::Zero);
};

inline STMPTAmount
operator+(STMPTAmount const& lhs, STMPTAmount const& rhs)
{
    if (lhs.issue() != rhs.issue())
        Throw<std::runtime_error>("Can't add amounts that aren't comparable!");
    return {lhs.issue(), lhs.value() + rhs.value()};
}

inline STMPTAmount
operator-(STMPTAmount const& lhs, STMPTAmount const& rhs)
{
    return lhs + (-rhs);
}

inline STMPTAmount&
STMPTAmount::operator+=(const ripple::STMPTAmount& other)
{
    *this = *this + other;
    return *this;
}

inline STMPTAmount&
STMPTAmount::operator-=(const ripple::STMPTAmount& other)
{
    *this = *this - other;
    return *this;
}

inline STMPTAmount
STMPTAmount::operator-() const
{
    return {issue_, -value_};
}

inline STMPTAmount& STMPTAmount::operator=(beast::Zero)
{
    clear();
    return *this;
}

inline bool
operator==(STMPTAmount const& lhs, STMPTAmount const& rhs)
{
    return lhs.issue() == rhs.issue() && lhs.value() == rhs.value();
}

inline bool
operator<(STMPTAmount const& lhs, STMPTAmount const& rhs)
{
    if (lhs.issue() != rhs.issue())
        Throw<std::runtime_error>(
            "Can't compare amounts that are't comparable!");
    return lhs.value() < rhs.value();
}

inline bool
operator!=(STMPTAmount const& lhs, STMPTAmount const& rhs)
{
    return !(lhs == rhs);
}

inline bool
operator>(STMPTAmount const& lhs, STMPTAmount const& rhs)
{
    return rhs < lhs;
}

inline bool
operator<=(STMPTAmount const& lhs, STMPTAmount const& rhs)
{
    return !(rhs < lhs);
}

inline bool
operator>=(STMPTAmount const& lhs, STMPTAmount const& rhs)
{
    return !(lhs < rhs);
}

STMPTAmount
amountFromString(MPTIssue const& issue, std::string const& amount);

STMPTAmount
multiply(STMPTAmount const& amount, Rate const& rate);

}  // namespace ripple

#endif  // RIPPLE_PROTOCOL_STMPTAMOUNT_H_INCLUDED
