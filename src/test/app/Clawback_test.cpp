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

#include <ripple/basics/random.h>
#include <ripple/json/to_string.h>
#include <ripple/ledger/ApplyViewImpl.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/jss.h>
#include <initializer_list>
#include <test/jtx.h>
#include <test/jtx/trust.h>

namespace ripple {

class Clawback_test : public beast::unit_test::suite
{
    template <class T>
    static std::string
    to_string(T const& t)
    {
        return boost::lexical_cast<std::string>(t);
    }

    // Helper function that returns the owner count of an account root.
    static std::uint32_t
    ownerCount(test::jtx::Env const& env, test::jtx::Account const& acct)
    {
        std::uint32_t ret{0};
        if (auto const sleAcct = env.le(acct))
            ret = sleAcct->at(sfOwnerCount);
        return ret;
    }

    // Helper function that returns the number of tickets held by an account.
    static std::uint32_t
    ticketCount(test::jtx::Env const& env, test::jtx::Account const& acct)
    {
        std::uint32_t ret{0};
        if (auto const sleAcct = env.le(acct))
            ret = sleAcct->at(~sfTicketCount).value_or(0);
        return ret;
    }

    // Helper function that returns the freeze status of a trustline
    static bool
    getLineFreezeFlag(
        test::jtx::Env const& env,
        test::jtx::Account const& src,
        test::jtx::Account const& dst,
        Currency const& cur)
    {
        if (auto sle = env.le(keylet::line(src, dst, cur)))
        {
            auto const useHigh = src.id() > dst.id();
            return sle->isFlag(useHigh ? lsfHighFreeze: lsfLowFreeze);
        }
        Throw<std::runtime_error>("No line in getLineFreezeFlag");
        return false;  // silence warning
    }

    void
    testAllowClawbackFlag(FeatureBitset features)
    {
        testcase("Enable AllowClawback flag");
        using namespace test::jtx;

        // Test if one can successfully set asfAllowClawback flag.
        // If successful, asfNoFreeze can no longer be set.
        // Also, asfAllowClawback cannot be cleared.
        {
            Env env(*this, features);
            Account alice{"alice"};

            env.fund(XRP(1000), alice);
            env.close();

            // set asfAllowClawback
            env(fset(alice, asfAllowClawback));
            env.close();

            // verify flag is still set (clear does not clear in this case)
            env.require(flags(alice, asfAllowClawback));

            // clear asfAllowClawback does nothing
            env(fclear(alice, asfAllowClawback));
            env.close();
            env.require(flags(alice, asfAllowClawback));

            // asfNoFreeze cannot be set when asfAllowClawback is set
            env.require(nflags(alice, asfNoFreeze));
            env(fset(alice, asfNoFreeze), ter(tecNO_PERMISSION));
            env.close();
        }

        // Test that asfAllowClawback cannot be set when
        // asfNoFreeze has been set
        {
            Env env(*this, features);
            Account alice{"alice"};

            env.fund(XRP(1000), alice);
            env.close();

            env.require(nflags(alice, asfNoFreeze));

            // set asfNoFreeze
            env(fset(alice, asfNoFreeze));
            env.close();

            // NoFreeze is set
            env.require(flags(alice, asfNoFreeze));

            // asfAllowClawback cannot be set if asfNoFreeze is set
            env(fset(alice, asfAllowClawback), ter(tecNO_PERMISSION));
            env.close();

            env.require(nflags(alice, asfAllowClawback));
        }

        // Test that asfAllowClawback is not allowed when owner dir is non-empty
        {
            Env env(*this, features);

            Account alice{"alice"};
            Account bob{"bob"};

            env.fund(XRP(1000), alice, bob);
            env.close();

            auto const USD = alice["USD"];
            env.require(nflags(alice, asfAllowClawback));

            // alice issues 10 USD to bob
            env.trust(USD(1000), bob);
            env(pay(alice, bob, USD(10)));
            env.close();

            BEAST_EXPECT(ownerCount(env, alice) == 0);
            BEAST_EXPECT(ownerCount(env, bob) == 1);

            // alice fails to enable clawback because she has trustline with bob
            env(fset(alice, asfAllowClawback), ter(tecOWNERS));
            env.close();

            // bob sets trustline to default limit and pays alice back to delete
            // the trustline
            env(trust(bob, USD(0), 0));
            env(pay(bob, alice, USD(10)));

            BEAST_EXPECT(ownerCount(env, bob) == 0);

            // alice now is able to set asfAllowClawback
            env(fset(alice, asfAllowClawback));
            env.require(flags(alice, asfAllowClawback));
            env.close();

            BEAST_EXPECT(ownerCount(env, alice) == 0);
            BEAST_EXPECT(ownerCount(env, bob) == 0);
        }
    }

    void
    testPermission(FeatureBitset features)
    {
        testcase("Permission");
        using namespace test::jtx;

        // Test that trustline cannot be clawed by someone who is
        // not the issuer of the currency
        {
            Env env(*this, features);

            Account alice{"alice"};
            Account bob{"bob"};
            Account cindy{"cindy"};

            env.fund(XRP(1000), alice, bob, cindy);
            env.close();

            auto const USD = alice["USD"];

            // alice sets asfAllowClawback
            env(fset(alice, asfAllowClawback));
            env.require(flags(alice, asfAllowClawback));
            env.close();

            // cindy sets asfAllowClawback
            env(fset(cindy, asfAllowClawback));
            env.require(flags(cindy, asfAllowClawback));
            env.close();

            // alice issues 1000 USD to bob
            env.trust(USD(1000), bob);
            env(pay(alice, bob, USD(1000)));
            env.close();

            env.require(balance(bob, alice["USD"](1000)));
            env.require(balance(alice, bob["USD"](-1000)));

            // cindy tries to claw from bob, and fails because trustline does not exist
            env(claw(cindy, bob["USD"](200)), ter(tecNO_LINE));
            env.close();
        }

        // When a trustline is created between issuer and holder,
        // we must make sure the holder is unable to claw back from
        // the issuer by impersonating the issuer account.
        //
        // This must be tested bidirectionally for both accounts because the issuer
        // could be either the low or high account in the trustline object
        {
            Env env(*this, features);

            Account alice{"alice"};
            Account bob{"bob"};

            env.fund(XRP(1000), alice, bob);
            env.close();

            auto const USD = alice["USD"];
            auto const CAD = bob["CAD"];

            // alice sets asfAllowClawback
            env(fset(alice, asfAllowClawback));
            env.require(flags(alice, asfAllowClawback));
            env.close();

            // bob sets asfAllowClawback
            env(fset(bob, asfAllowClawback));
            env.require(flags(bob, asfAllowClawback));
            env.close();

            // alice issues 10 USD to bob.
            // bob then attempts to submit a clawback tx to claw USD from alice.
            // this must FAIL, because bob is not the issuer for this trustline!!!
            {
                // bob creates a trustline with alice, and alice sends 10 USD to bob
                env.trust(USD(1000), bob);
                env(pay(alice, bob, USD(10)));
                env.close();

                env.require(balance(bob, alice["USD"](10)));
                env.require(balance(alice, bob["USD"](-10)));

                // bob cannot claw back USD from alice because he's not the issuer
                env(claw(bob, alice["USD"](5)), ter(tecNO_PERMISSION));
                env.close();
            }

            // bob issues 10 CAD to alice.
            // alice then attempts to submit a clawback tx to claw CAD from bob.
            // this must FAIL, because alice is not the issuer for this trustline!!!
            {
                // alice creates a trustline with bob, and bob sends 10 CAD to alice
                env.trust(CAD(1000), alice);
                env(pay(bob, alice, CAD(10)));
                env.close();

                env.require(balance(bob, alice["CAD"](-10)));
                env.require(balance(alice, bob["CAD"](10)));

                // alice cannot claw back CAD from bob because she's not the issuer
                env(claw(alice, bob["CAD"](5)), ter(tecNO_PERMISSION));
                env.close();
            }
        }
    }

    void 
    testValidation(FeatureBitset features)
    {
        testcase("Validation");
        using namespace test::jtx;

        // Tests enabling asfAllowClawback when amendment is disabled, and
        // tests Clawback tx fails for the following:
        // 1. when amendment is disabled
        // 2. when asfAllowClawback flag has not been set
        {
            Env env(*this, features - featureClawback);

            Account alice{"alice"};
            Account bob{"bob"};

            env.fund(XRP(1000), alice, bob);
            env.close();

            env.require(nflags(bob, asfAllowClawback));

            // alice attempts to set asfAllowClawback flag while amendment is disabled.
            // no error is returned, but the flag remains to be unset.
            env(fset(alice, asfAllowClawback));
            env.require(nflags(alice, asfAllowClawback));
            env.close();

            auto const USD = alice["USD"];

            // alice issues 10 USD to bob
            env.trust(USD(1000), bob);
            env(pay(alice, bob, USD(10)));
            env.close();

            env.require(balance(bob, alice["USD"](10)));
            env.require(balance(alice, bob["USD"](-10)));

            // clawback fails because amendment is disabled
            env(claw(alice, bob["USD"](5)), ter(temDISABLED));
            env.close();

            // now enable clawback amendment
            env.enableFeature(featureClawback);
            env.close();

            // clawback fails because asfAllowClawback has not been set
            env(claw(alice, bob["USD"](5)), ter(tecNO_PERMISSION));
            env.close();

            env.require(balance(bob, alice["USD"](10)));
            env.require(balance(alice, bob["USD"](-10)));
        }

        // Testing Clawback tx fails for the following:
        // 1. invalid flag
        // 2. negative STAmount
        // 3. zero STAmount
        // 4. XRP amount
        // 5. `account` and `issuer` fields are same account
        // 6. trustline has a balance of 0
        // 7. trustline does not exist
        {
            Env env(*this, features);

            Account alice{"alice"};
            Account bob{"bob"};

            env.fund(XRP(1000), alice, bob);
            env.close();

            // alice sets asfAllowClawback
            env(fset(alice, asfAllowClawback));
            env.close();
            env.require(flags(alice, asfAllowClawback));

            auto const USD = alice["USD"];

            // alice issues 10 USD to bob
            env.trust(USD(1000), bob);
            env(pay(alice, bob, USD(10)));
            env.close();

            env.require(balance(bob, alice["USD"](10)));
            env.require(balance(alice, bob["USD"](-10)));

            // fails due to invalid flag
            env(claw(alice, bob["USD"](5)),
                txflags(0x00008000),
                ter(temINVALID_FLAG));
            env.close();

            // fails due to negative amount
            env(claw(alice, bob["USD"](-5)), ter(temBAD_AMOUNT));
            env.close();

            // fails due to zero amount
            env(claw(alice, bob["USD"](0)), ter(temBAD_AMOUNT));
            env.close();

            // fails because amount is in XRP
            env(claw(alice, XRP(10)), ter(temBAD_AMOUNT));
            env.close();

            // fails when `issuer` field in `amount` is not token holder
            // NOTE: we are using the `issuer` field for the token holder
            env(claw(alice, alice["USD"](5)), ter(temBAD_AMOUNT));
            env.close();

            // bob pays alice back, trustline has a balance of 0
            env(pay(bob, alice, USD(10)));
            env.close();

            // bob still owns the trustline that has 0 balance
            BEAST_EXPECT(ownerCount(env, bob) == 1);
            env.require(balance(bob, alice["USD"](0)));
            env.require(balance(alice, bob["USD"](0)));

            // clawback fails because because balance is 0
            env(claw(alice, bob["USD"](5)), ter(tecNO_LINE));
            env.close();

            // set the limit to default, which should delete the trustline
            env(trust(bob, USD(0), 0));
            env.close();

            BEAST_EXPECT(ownerCount(env, bob) == 0);

            // clawback fails because trustline does not exist
            env(claw(alice, bob["USD"](5)), ter(tecNO_LINE));
            env.close();
        }
    }

    void
    testEnabled(FeatureBitset features)
    {
        testcase("Enable clawback");
        using namespace test::jtx;

        // Test that alice is able to successfully clawback tokens from bob
        Env env(*this, features);

        Account alice{"alice"};
        Account bob{"bob"};

        env.fund(XRP(1000), alice, bob);
        env.close();

        auto const USD = alice["USD"];

        // alice sets asfAllowClawback
        env(fset(alice, asfAllowClawback));
        env.require(flags(alice, asfAllowClawback));
        env.close();

        // alice issues 1000 USD to bob
        env.trust(USD(1000), bob);
        env(pay(alice, bob, USD(1000)));
        env.close();

        env.require(balance(bob, alice["USD"](1000)));
        env.require(balance(alice, bob["USD"](-1000)));

        // alice claws back 200 USD from bob
        env(claw(alice, bob["USD"](200)));
        env.close();

        // bob should have 800 USD left
        env.require(balance(bob, alice["USD"](800)));
        env.require(balance(alice, bob["USD"](-800)));

        // bob pays alice back with all the USD
        env(pay(bob, alice, USD(800)));
        env.close();

        // trustline has a balance of 0
        env.require(balance(bob, alice["USD"](0)));
        env.require(balance(alice, bob["USD"](0)));
    }

    void
    testMultiLine(FeatureBitset features){
        testcase("Multi line");
        using namespace test::jtx;

        // Both alice and bob issues their own "USD" to cindy.
        // When alice and bob tries to claw back, they will only
        // claw back from their respective trustline.
        Env env(*this, features);

        Account alice{"alice"};
        Account bob{"bob"};
        Account cindy{"cindy"};

        env.fund(XRP(1000), alice, bob, cindy);
        env.close();

        // alice sets asfAllowClawback
        env(fset(alice, asfAllowClawback));
        env.require(flags(alice, asfAllowClawback));
        env.close();

        // bob sets asfAllowClawback
        env(fset(bob, asfAllowClawback));
        env.require(flags(bob, asfAllowClawback));
        env.close();

        // alice sends 1000 USD to cindy
        env.trust(alice["USD"](1000), cindy);
        env(pay(alice, cindy, alice["USD"](1000)));
        env.close();

        // bob sends 1000 USD to cindy
        env.trust(bob["USD"](1000), cindy);
        env(pay(bob, cindy, bob["USD"](1000)));
        env.close();

        // alice claws back 200 USD from cindy
        env(claw(alice, cindy["USD"](200)));
        env.close();

        // cindy has 800 USD left in alice's trustline after clawed by alice
        env.require(balance(cindy, alice["USD"](800)));
        env.require(balance(alice, cindy["USD"](-800)));

        // cindy still has 1000 USD in bob's trustline
        env.require(balance(cindy, bob["USD"](1000)));
        env.require(balance(bob, cindy["USD"](-1000)));

        // bob claws back 600 USD from cindy
        env(claw(bob, cindy["USD"](600)));
        env.close();

        // cindy has 400 USD left in bob's trustline after clawed by bob
        env.require(balance(cindy, bob["USD"](400)));
        env.require(balance(bob, cindy["USD"](-400)));

        // cindy still has 800 USD in alice's trustline
        env.require(balance(cindy, alice["USD"](800)));
        env.require(balance(alice, cindy["USD"](-800)));
    }

    void testBidirectionalLine(FeatureBitset features){
        testcase("Bidirectional line");
        using namespace test::jtx;

        // Test when both alice and bob issues USD to each other.
        // This scenario creates only one trustline,
        // In this case, both alice and bob can be seen as the "issuer"
        // and they can send however many USDs to each other.
        // We test that only the person who has a negative balance from their
        // perspective is allowed to clawback
        Env env(*this, features);

        Account alice{"alice"};
        Account bob{"bob"};

        env.fund(XRP(1000), alice, bob);
        env.close();

        // alice sets asfAllowClawback
        env(fset(alice, asfAllowClawback));
        env.require(flags(alice, asfAllowClawback));
        env.close();

        // bob sets asfAllowClawback
        env(fset(bob, asfAllowClawback));
        env.require(flags(bob, asfAllowClawback));
        env.close();

        // alice issues 1000 USD to bob
        env.trust(alice["USD"](1000), bob);
        env(pay(alice, bob, alice["USD"](1000)));
        env.close();

        // bob is the holder, and alice is the issuer
        env.require(balance(bob, alice["USD"](1000)));
        env.require(balance(alice, bob["USD"](-1000)));

        // bob issues 1500 USD to alice
        env.trust(bob["USD"](1500), alice);
        env(pay(bob, alice, bob["USD"](1500)));
        env.close();

        // bob has negative 500 USD because bob issued 500 USD more than alice
        // bob can now been seen as the issuer, while alice is the holder
        env.require(balance(bob, alice["USD"](-500)));
        env.require(balance(alice, bob["USD"](500)));

        // At this point, both alice and bob are the issuers of USD
        // and can send USD to each other through one trustline

        // alice fails to clawback. Even though she is also an issuer,
        // the trustline balance is positive from her perspective
        env(claw(alice, bob["USD"](200)), ter(tecNO_PERMISSION));
        env.close();

        // bob is able to successfully clawback from alice because
        // the trustline balance is negative from his perspective
        env(claw(bob, alice["USD"](200)));
        env.close();

        env.require(balance(bob, alice["USD"](-300)));
        env.require(balance(alice, bob["USD"](300)));

        // alice pays bob 1000 USD
        env(pay(alice, bob, alice["USD"](1000)));
        env.close();

        // bob's balance becomes positive from his perspective because
        // alice issued more USD than the balance
        env.require(balance(bob, alice["USD"](700)));
        env.require(balance(alice, bob["USD"](-700)));

        // bob is now the holder and fails to clawback
        env(claw(bob, alice["USD"](200)), ter(tecNO_PERMISSION));
        env.close();

        // alice successfully claws back
        env(claw(alice, bob["USD"](200)));
        env.close();

        env.require(balance(bob, alice["USD"](500)));
        env.require(balance(alice, bob["USD"](-500)));
    }

    void
    testDeleteDefaultLine(FeatureBitset features)
    {
        testcase("Delete default trustline");
        using namespace test::jtx;

        // If clawback results the trustline to be default,
        // trustline should be automatically deleted
        Env env(*this, features);
        Account alice{"alice"};
        Account bob{"bob"};

        env.fund(XRP(1000), alice, bob);
        env.close();

        auto const USD = alice["USD"];

        // alice sets asfAllowClawback
        env(fset(alice, asfAllowClawback));
        env.require(flags(alice, asfAllowClawback));
        env.close();

        // alice issues 1000 USD to bob
        env.trust(USD(1000), bob);
        env(pay(alice, bob, USD(1000)));
        env.close();

        BEAST_EXPECT(ownerCount(env, bob) == 1);

        env.require(balance(bob, alice["USD"](1000)));
        env.require(balance(alice, bob["USD"](-1000)));

        // set limit to default,
        env(trust(bob, USD(0), 0));
        env.close();

        BEAST_EXPECT(ownerCount(env, bob) == 1);

        // alice claws back full amount from bob, and should also delete trustline
        env(claw(alice, bob["USD"](1000)));
        env.close();

        // bob no longer owns the trustline because it was deleted
        BEAST_EXPECT(ownerCount(env, bob) == 0);
    }

    void
    testFrozenLine(FeatureBitset features)
    {
        testcase("Frozen trustline");
        using namespace test::jtx;

        // Claws back from frozen trustline
        // and the trustline should remain frozen
        Env env(*this, features);
        Account alice{"alice"};
        Account bob{"bob"};

        env.fund(XRP(1000), alice, bob);
        env.close();

        auto const USD = alice["USD"];

        // alice sets asfAllowClawback
        env(fset(alice, asfAllowClawback));
        env.require(flags(alice, asfAllowClawback));
        env.close();

        // alice issues 1000 USD to bob
        env.trust(USD(1000), bob);
        env(pay(alice, bob, USD(1000)));
        env.close();

        env.require(balance(bob, alice["USD"](1000)));
        env.require(balance(alice, bob["USD"](-1000)));

        // freeze trustline
        env(trust(alice, bob["USD"](0), tfSetFreeze));
        env.close();

        // alice claws back 200 USD from bob
        env(claw(alice, bob["USD"](200)));
        env.close();

        // bob should have 800 USD left
        env.require(balance(bob, alice["USD"](800)));
        env.require(balance(alice, bob["USD"](-800)));

        // trustline remains frozen
        BEAST_EXPECT(getLineFreezeFlag(env, alice, bob, USD.currency));
    }

    void
    testAmountExceedsAvailable(FeatureBitset features)
    {
        testcase("Amount exceeds available");
        using namespace test::jtx;

        // When alice tries to claw back an amount that is greater
        // than what bob holds, only the max available balance is clawed
        Env env(*this, features);
        Account alice{"alice"};
        Account bob{"bob"};

        env.fund(XRP(1000), alice, bob);
        env.close();

        auto const USD = alice["USD"];

        // alice sets asfAllowClawback
        env(fset(alice, asfAllowClawback));
        env.require(flags(alice, asfAllowClawback));
        env.close();

        // alice issues 1000 USD to bob
        env.trust(USD(1000), bob);
        env(pay(alice, bob, USD(1000)));
        env.close();

        env.require(balance(bob, alice["USD"](1000)));
        env.require(balance(alice, bob["USD"](-1000)));

        // alice tries to claw back 2000 USD
        env(claw(alice, bob["USD"](2000)));
        env.close();

        // check alice and bob's balance.
        // alice was only able to claw back 1000 USD at maximum
        env.require(balance(bob, alice["USD"](0)));
        env.require(balance(alice, bob["USD"](0)));

        // bob still owns the trustline because trustline is not in default state
        BEAST_EXPECT(ownerCount(env, bob) == 1);

        // set limit to default,
        env(trust(bob, USD(0), 0));
        env.close();

        // bob now deletes his trustline
        BEAST_EXPECT(ownerCount(env, bob) == 0);
    }

    void
    testTickets(FeatureBitset features)
    {
        testcase("Tickets");
        using namespace test::jtx;

        // Tests clawback with tickets
        Env env(*this, features);
        Account alice{"alice"};
        Account bob{"bob"};

        env.fund(XRP(1000), alice, bob);
        env.close();

        auto const USD = alice["USD"];

        // alice sets asfAllowClawback
        env(fset(alice, asfAllowClawback));
        env.require(flags(alice, asfAllowClawback));
        env.close();

        // alice issues 100 USD to bob
        env.trust(USD(1000), bob);
        env(pay(alice, bob, USD(100)));
        env.close();

        env.require(balance(bob, alice["USD"](100)));
        env.require(balance(alice, bob["USD"](-100)));

        // alice creates 10 tickets
        std::uint32_t ticketCnt = 10;
        std::uint32_t aliceTicketSeq{env.seq(alice) + 1};
        env(ticket::create(alice, ticketCnt));
        env.close();
        std::uint32_t const aliceSeq{env.seq(alice)};
        BEAST_EXPECT(ticketCount(env, alice) == ticketCnt);
        BEAST_EXPECT(ownerCount(env, alice) == ticketCnt);

        while (ticketCnt > 0)
        {
            // alice claws back 5 USD using a ticket
            env(claw(alice, bob["USD"](5)), ticket::use(aliceTicketSeq++));
            env.close();

            ticketCnt--;
            BEAST_EXPECT(ticketCount(env, alice) == ticketCnt);
            BEAST_EXPECT(ownerCount(env, alice) == ticketCnt);
        }

        // alice clawed back 50 USD total, trustline has 50 USD remaining
        env.require(balance(bob, alice["USD"](50)));
        env.require(balance(alice, bob["USD"](-50)));

        // Verify that the account sequence numbers did not advance.
        BEAST_EXPECT(env.seq(alice) == aliceSeq);
    }

    void
    testWithFeats(FeatureBitset features)
    {
        testAllowClawbackFlag(features);
        testPermission(features);
        testValidation(features);
        testEnabled(features);
        testMultiLine(features);
        testBidirectionalLine(features);
        testDeleteDefaultLine(features);
        testFrozenLine(features);
        testAmountExceedsAvailable(features);
        testTickets(features);
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
