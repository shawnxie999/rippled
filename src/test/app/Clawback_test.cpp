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

namespace ripple {

class Clawback_test : public beast::unit_test::suite
{
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
        //todo: try clawback while flag is turned off

    }

    void 
    testEnable(FeatureBitset features){
        testcase("Enable clawback");
        using namespace test::jtx;

        Env env(*this, features);
        auto j = env.app().journal("View");
        ApplyViewImpl av(&*env.current(), tapNONE);
  

        Account alice{"alice"};
        Account bob{"bob"};

        env.fund(XRP(1000), alice, bob);
        env.close();

        auto const USD_alice = alice["USD"];
        auto const iss = USD_alice.issue();
        auto const startingAmount = USD_alice(1000);

        env(fset(alice, asfAllowClawback));
        env.require(flags(alice, asfAllowClawback));
        
        env.trust(startingAmount, bob);
        env.close();


        // env(pay(alice, bob, USD_alice(1000)));
        // env.close();

        
        
        BEAST_EXPECT(
            accountFunds(
                av, bob, startingAmount, fhIGNORE_FREEZE, j) == startingAmount);  

        env(claw(alice, bob["USD"](1000), tfSetFreeze));
        env.close();
        BEAST_EXPECT(
            accountFunds(
                av, bob, startingAmount, fhIGNORE_FREEZE, j) == beast::zero);             


    }

    void
    testWithFeats(FeatureBitset features)
    {
        testAllowClawbackFlag(features);
        testEnable(features);


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
