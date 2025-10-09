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

#include <test/jtx.h>
#include <test/jtx/confidentialTransfer.h>
#include <test/jtx/trust.h>

#include <xrpl/protocol/Feature.h>

namespace ripple {

class ConfidentialTransfer_test : public beast::unit_test::suite
{
    void
    testConvert(FeatureBitset features)
    {
        testcase("test convert");
        using namespace test::jtx;
        Env env{*this, features};
        Account const alice("alice");
        Account const bob("bob");
        MPTTester mptAlice(env, alice, {.holders = {bob}});

        mptAlice.create(
            {.ownerCount = 1, .holderCount = 0, .flags = tfMPTCanTransfer});

        mptAlice.authorize({.account = bob});
        env.close();
        mptAlice.pay(alice, bob, 100);
        env.close();

        // std::uint8_t const rawData[] = {
        //     0xB9, 0xE8, 0xC7, 0xD6, 0xA5, 0xF4, 0xB3, 0xA2, 0xC1, 0xD0, 0xE9,
        //     0xF8, 0xB7, 0xA6, 0xC5, 0xD4, 0xE3, 0xF2, 0xA1, 0xB0, 0xC9, 0xD8,
        //     0xE7, 0xF6, 0xA5, 0xB4, 0xC3, 0xD2, 0xE1, 0xF0, 0xB9, 0xA8, 0xC7,
        //     0xD6, 0xE5, 0xF4, 0xB3, 0xA2, 0xC1, 0xD0, 0xB9, 0xE8, 0xA7, 0xF6,
        //     0xC5, 0xD4, 0xB3, 0xA2, 0xE1, 0xF0, 0xB9, 0xC8, 0xD7, 0xA6, 0xE5,
        //     0xF4, 0xB3, 0xA2, 0xC1, 0xD0, 0xB9, 0xE8, 0xA7, 0xF6, 0xB5,
        //     0xA4};

        // Slice encAmt{rawData, ecGamalEncryptedTotalLength};
        std::string data =
            "B9E8C7D6A5F4B3A2C1D0E9F8B7A6C5D4E3F2A1B0C9D8E7F6A5B4C3D2E1F0B9A8C7"
            "D6E5F4B3A2C1D0B9E8A7F6C5D4B3A2E1F0B9C8D7A6E5F4B3A2C1D0B9E8A7F6B5A"
            "4";
        env(convert(mptAlice.issuanceID(), bob, 10, "123", data, data, "1243"));
        mptAlice.set({.account = alice, .pubKey = "123"});
        mptAlice.convert(
            {.account = bob,
             .amt = 10,
             .proof = "123",
             .holderPubKey = "123",
             .holderEncryptedAmt = data,
             .issuerEncryptedAmt = data});
        env.close();
    }

    void
    testWithFeats(FeatureBitset features)
    {
        testConvert(features);
    }

public:
    void
    run() override
    {
        using namespace test::jtx;
        FeatureBitset const all{testable_amendments()};

        testWithFeats(all);
    }
};

BEAST_DEFINE_TESTSUITE(ConfidentialTransfer, app, ripple);
}  // namespace ripple
