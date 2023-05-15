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

#include <ripple/ledger/ApplyViewImpl.h>
#include <ripple/basics/random.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/jss.h>
#include <test/jtx.h>
#include <test/jtx/trust.h>
#include <initializer_list>
#include <ripple/json/to_string.h>

namespace ripple {

class Clawback_test : public beast::unit_test::suite
{
        template <class T>
        static std::string
    to_string(T const& t)
    {
        return boost::lexical_cast<std::string>(t);
    }
    void
    testAllowClawbackFlag(FeatureBitset features){
        testcase("Enable clawback flag");

        using namespace test::jtx;

        {
            Env env(*this, features);
            Account alice{"alice"};


            env.fund(XRP(1000), alice);
            env.close();

            env(fset(alice, asfAllowClawback));

            // verify flag is still set (clear does not clear in this case)
            env.require(flags(alice, asfAllowClawback));
            env(fclear(alice, asfAllowClawback));
            env.require(flags(alice, asfAllowClawback));

            env.require(nflags(alice, asfNoFreeze));
            env(fset(alice, asfNoFreeze) , ter(tecNO_PERMISSION));
        }
         {
            Env env(*this, features);
            Account alice{"alice"};


            env.fund(XRP(1000), alice);
            env.close();

            env.require(nflags(alice, asfNoFreeze));
            env(fset(alice, asfNoFreeze));
            env.require(flags(alice, asfNoFreeze));
            env(fset(alice, asfAllowClawback), ter(tecNO_PERMISSION));
            env.require(nflags(alice, asfAllowClawback));
        }

    }

    void
    testEnable(FeatureBitset features){
        testcase("Enable clawback");
        using namespace test::jtx;

        // Test when alice tries to claw back without setting asfAllowClawback flag
        {
            Env env(*this, features);
            
            Account alice{"alice"};
            Account bob{"bob"};

            env.fund(XRP(1000), alice, bob);
            env.close();

            env.require(nflags(bob, asfAllowClawback));

            auto const USD = alice["USD"];
            env.trust(USD(1000), bob);
            env(pay(alice, bob, USD(10)));
            env.close();

            BEAST_EXPECT(to_string(env.balance("bob", USD)) == "10/USD(alice)");
            BEAST_EXPECT(
                to_string(env.balance(alice, bob["USD"])) == "-10/USD(bob)");

            //clawback fails
            env(claw(alice, bob["USD"](5), 0), ter(tecNO_PERMISSION));
            env.close();

            // alice and bob's balances remain the same
            BEAST_EXPECT(to_string(env.balance("bob", USD)) == "10/USD(alice)");
            BEAST_EXPECT(
                to_string(env.balance(alice, bob["USD"])) == "-10/USD(bob)");

        }
        {
            Env env(*this, features);
            
            Account alice{"alice"};
            Account bob{"bob"};

            env.fund(XRP(1000), alice, bob);
            env.close();

            auto const USD = alice["USD"];

            env(fset(alice, asfAllowClawback));
            env.require(flags(alice, asfAllowClawback));
            env.close();

            env.trust(USD(1000), bob);
            env(pay(alice, bob, USD(10)));
            env.close();

            BEAST_EXPECT(to_string(env.balance("bob", USD)) == "10/USD(alice)");
            BEAST_EXPECT(
                to_string(env.balance(alice, bob["USD"])) == "-10/USD(bob)");

            // fails due to negative amount 
            env(claw(alice, bob["USD"](-5), 0), ter(temBAD_AMOUNT));
            env.close();

            // fails when `issuer` field in `amount` is not token holder
            // NOTE: we are using the `issuer` field for the token holder
            env(claw(alice, alice["USD"](5), 0), ter(temBAD_AMOUNT));
            env.close();

            //clawback is a success
            env(claw(alice, bob["USD"](5), 0));
            env.close();

            // check alice and bob's balance
            BEAST_EXPECT(to_string(env.balance("bob", USD)) == "5/USD(alice)");
            BEAST_EXPECT(
                to_string(env.balance(alice, bob["USD"])) == "-5/USD(bob)");
            env.close();
            
        }

        
    }

    void
    testNoTrustline(FeatureBitset features){
        testcase("Claw no trustline");
        using namespace test::jtx;
        Env env(*this, features);
        

        Account alice{"alice"};
        Account bob{"bob"};

        env.fund(XRP(1000), alice, bob);
        env.close();

        auto const USD = alice["USD"];

        env(fset(alice, asfAllowClawback));
        env.require(flags(alice, asfAllowClawback));
        env.close();

        // Returns error because no trustline exists
        env(claw(alice, bob["USD"](5), 0), ter(tecNO_LINE));
        env.close();
    }

    void
    testWithFeats(FeatureBitset features)
    {
        testAllowClawbackFlag(features);
        testEnable(features);

        testNoTrustline(features);
    }

public:
    void
    run() override
    {
        using namespace test::jtx;
        FeatureBitset const all{supported_amendments()};

        testWithFeats(all);
    }
};


BEAST_DEFINE_TESTSUITE(Clawback, app, ripple);
}  // namespace ripple
