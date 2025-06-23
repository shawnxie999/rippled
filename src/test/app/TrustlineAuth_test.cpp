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
#include <test/jtx/AMM.h>
#include <test/jtx/AMMTest.h>
#include <test/jtx/amount.h>
#include <test/jtx/sendmax.h>

#include <xrpld/app/misc/AMMHelpers.h>
#include <xrpld/app/misc/AMMUtils.h>
#include <xrpld/app/paths/AMMContext.h>
#include <xrpld/app/paths/AMMOffer.h>
#include <xrpld/app/tx/detail/AMMBid.h>
#include <xrpld/ledger/ApplyViewImpl.h>
#include <xrpld/rpc/RPCHandler.h>
#include <xrpld/rpc/detail/RPCHelpers.h>

#include <xrpl/basics/Number.h>
#include <xrpl/protocol/AMMCore.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/STParsedJSON.h>
#include <xrpl/resource/Fees.h>

#include <boost/regex.hpp>

#include "xrpl/protocol/TER.h"

#include <chrono>
#include <utility>
#include <vector>
namespace ripple {
namespace test {

class TrustlineAuth_test : public jtx::AMMTest
{
    void
    testUnauthLPTokenRippling(FeatureBitset features)
    {
        testcase("Unauthorized LPToken rippling");

        using namespace jtx;

        // disable AMMClawback to allow single side deposit without owning one
        // of the assets
        Env env(*this, features - featureAMMClawback);
        env.fund(XRP(1000), gw, alice, carol, bob);
        env(fset(gw, asfRequireAuth));
        env(rate(gw, 1.25));
        env.close();

        // gateway authorizes alice
        auto authAndFund = [&](Account const& account,
                               std::string const currency) {
            env(trust(gw, account[currency](100'000)), txflags(tfSetfAuth));
            env(trust(account, gw[currency](100'000)));
            env.close();
            env(pay(gw, account, gw[currency](30'000)));
            env.close();
        };

        // carol has BTC line but not USD line
        // bob has USD line but not BTC line
        // alice has both USD and BTC line
        authAndFund(alice, "BTC");
        authAndFund(alice, "USD");
        authAndFund(bob, "BTC");
        authAndFund(carol, "USD");

        AMM ammAlice(env, alice, USD(20'000), BTC(0.5));

        // bob single side deposits with BTC
        ammAlice.deposit(bob, BTC(10));

        // carol single side deposits with USD
        ammAlice.deposit(carol, USD(2000));

        // increase limit for lptoken lines so that they can transfer lptokens
        // to each other
        auto const lpIssue = ammAlice.lptIssue();
        env.trust(STAmount{lpIssue, 500}, alice);
        env.trust(STAmount{lpIssue, 500}, bob);
        env.trust(STAmount{lpIssue, 500}, carol);
        env.close();

        env.enableFeature(featureAMMClawback);
        env.close();

        // transfer LPToken between alice, bob and carol and validate expected
        // result
        auto executeLPTokenPayments = [&](TER const code) {
            env(pay(carol, alice, STAmount{lpIssue, 1}), ter(code));
            env.close();
            env(pay(alice, carol, STAmount{lpIssue, 1}), ter(code));
            env.close();
            env(pay(bob, alice, STAmount{lpIssue, 1}), ter(code));
            env.close();
            env(pay(alice, bob, STAmount{lpIssue, 1}), ter(code));
            env.close();
            env(pay(bob, carol, STAmount{lpIssue, 1}), ter(code));
            env.close();
            env(pay(carol, bob, STAmount{lpIssue, 1}), ter(code));
            env.close();
        };

        // We are going to test the behavior when bob and carol tries to send
        // lptoken pre/post amendment.
        //
        // With fixEnforceTrustlineAuth amendment, carol and bob cannot receive
        // nor send lptoken if they doesn't have one of the trustlines
        if (features[fixEnforceTrustlineAuth])
        {
            executeLPTokenPayments(tecNO_LINE);
        }
        // without fixEnforceTrustlineAuth, carol and bob can still receive and
        // send lptoken freely even tho they don't have trustlines for one of
        // the assets
        else
        {
            executeLPTokenPayments(tesSUCCESS);
        }

        // bob and carol create trustlines for the assets that they are
        // missing.
        // HOWEVER, they are still unauthorized!
        env(trust(bob, gw["USD"](100'000)));
        env(trust(carol, gw["BTC"](100'000)));
        env.close();

        // With fixEnforceTrustlineAuth amendment, carol and bob cannot
        // receive nor send lptoken if they have unauthorized trustlines
        if (features[fixEnforceTrustlineAuth])
        {
            executeLPTokenPayments(tecNO_AUTH);
        }
        // without fixEnforceTrustlineAuth, carol and bob can still receive
        // and send lptoken freely even tho they don't have authorized
        // trustlines
        else
        {
            executeLPTokenPayments(tesSUCCESS);
        }

        // gateway authorizes bob and carol for their respective trustlines
        env(trust(gw, bob["USD"](100'000)), txflags(tfSetfAuth));
        env.close();
        env(trust(gw, carol["BTC"](100'000)), txflags(tfSetfAuth));
        env.close();

        // bob and carol can now transfer lptoken freely since they have
        // authorized lines for both assets
        executeLPTokenPayments(tesSUCCESS);
    }

public:
    void
    run() override
    {
        using namespace test::jtx;
        FeatureBitset const all{supported_amendments()};

        for (auto const features : {all, all - fixEnforceTrustlineAuth})
        {
            testUnauthLPTokenRippling(features);
        }
    }
};

BEAST_DEFINE_TESTSUITE(TrustlineAuth, app, ripple);
}  // namespace test
}  // namespace ripple
