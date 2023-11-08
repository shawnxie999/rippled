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

#include <ripple/protocol/Feature.h>
#include <ripple/protocol/jss.h>
#include <test/jtx.h>

namespace ripple {

class CFToken_test : public beast::unit_test::suite
{
    bool 
    cftEqualsAmount(test::jtx::Env const& env,
        ripple::uint256 const cftIssuanceID,
            test::jtx::Account const& holder, std::uint64_t expectedAmount){
        auto const sleCft = env.le(keylet::cftoken(cftIssuanceID, holder));
        std::uint64_t const amount = (*sleCft)[sfCFTAmount];
        return amount == expectedAmount;
    }

    bool   
    cftIsAuthorized(test::jtx::Env const& env,
        ripple::uint256 const cftIssuanceID,
            test::jtx::Account const& holder)
    {
            auto const sleCft = env.le(keylet::cftoken(cftIssuanceID, holder));
            uint32_t const cftFlags = sleCft->getFlags();
            return cftFlags & lsfCFTAuthorized;
    }

    void
    testEnabled(FeatureBitset features)
    {
        testcase("Enabled");

        using namespace test::jtx;
        {
            // If the CFT amendment is not enabled, you should not be able to
            // create CFTokenIssuances
            Env env{*this, features - featureCFTokensV1};
            Account const& master = env.master;

            BEAST_EXPECT(env.ownerCount(master) == 0);

            env(cft::create(master), ter(temDISABLED));
            env.close();

            BEAST_EXPECT(env.ownerCount(master) == 0);
        }
        {
            // If the CFT amendment IS enabled, you should be able to create
            // CFTokenIssuances
            Env env{*this, features | featureCFTokensV1};
            Account const& master = env.master;

            BEAST_EXPECT(env.ownerCount(master) == 0);

            env(cft::create(master));
            env.close();

            BEAST_EXPECT(env.ownerCount(master) == 1);
        }
        {
            // If the CFT amendment is not enabled, you should not be able to
            // destroy CFTokenIssuances
            Env env{*this, features - featureCFTokensV1};
            Account const& master = env.master;

            BEAST_EXPECT(env.ownerCount(master) == 0);

            auto const id = keylet::cftIssuance(master.id(), env.seq(master));
            env(cft::destroy(master, ripple::to_string(id.key)),
                ter(temDISABLED));
            env.close();

            BEAST_EXPECT(env.ownerCount(master) == 0);
        }
        {
            // If the CFT amendment IS enabled, you should be able to destroy
            // CFTokenIssuances
            Env env{*this, features | featureCFTokensV1};
            Account const& master = env.master;

            BEAST_EXPECT(env.ownerCount(master) == 0);

            auto const id = keylet::cftIssuance(master.id(), env.seq(master));
            env(cft::create(master));
            env.close();
            BEAST_EXPECT(env.ownerCount(master) == 1);

            env(cft::destroy(master, ripple::to_string(id.key)));
            env.close();
            BEAST_EXPECT(env.ownerCount(master) == 0);
        }
    }

    void
    testAuthorizeValidation(FeatureBitset features)
    {
        testcase("Validate authorize");

        using namespace test::jtx;
        // Validate fields in CFTokenAuthorize (preflight)
        {
            Env env{*this, features - featureCFTokensV1};
            Account const alice("alice"); //issuer
            Account const bob("bob"); //holder

            env.fund(XRP(10000), alice, bob);
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 0);

            auto const id = keylet::cftIssuance(alice.id(), env.seq(alice));

            env(cft::authorize(bob, id.key, std::nullopt), ter(temDISABLED));
            env.close();

            env.enableFeature(featureCFTokensV1);

            env(cft::create(alice));
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 1);

            env(cft::authorize(bob, id.key, std::nullopt), txflags(0x00000002), ter(temINVALID_FLAG));
            env.close();

            env(cft::authorize(bob, id.key, bob), ter(temMALFORMED));
            env.close();

            env(cft::authorize(alice, id.key, alice), ter(temMALFORMED));
            env.close();
        }

        // Try authorizing when CFTokenIssuance doesnt exist in CFTokenAuthorize (preclaim)
        {
            Env env{*this, features};
            Account const alice("alice"); //issuer
            Account const bob("bob"); //holder

            env.fund(XRP(10000), alice, bob);
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 0);

            auto const id = keylet::cftIssuance(alice.id(), env.seq(alice));

            env(cft::authorize(alice, id.key, bob), ter(tecOBJECT_NOT_FOUND));
            env.close();

            env(cft::authorize(bob, id.key, std::nullopt), ter(tecOBJECT_NOT_FOUND));
            env.close();
        }

        // Test bad scenarios without allowlisting in CFTokenAuthorize (preclaim)
        {
            Env env{*this, features};
            Account const alice("alice"); //issuer
            Account const bob("bob"); //holder

            env.fund(XRP(10000), alice, bob);
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 0);

            auto const id = keylet::cftIssuance(alice.id(), env.seq(alice));
            env(cft::create(alice));
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 1);

            // bob submits a tx with a holder field
            env(cft::authorize(bob, id.key, alice), ter(temMALFORMED));
            env.close();

            env(cft::authorize(bob, id.key, bob), ter(temMALFORMED));
            env.close();

            env(cft::authorize(alice, id.key, alice), ter(temMALFORMED));
            env.close();

            // the cft does not enable allowlisting
            env(cft::authorize(alice, id.key, bob), ter(tecNO_AUTH));
            env.close();

            // bob now holds a cftoken object
            env(cft::authorize(bob, id.key, std::nullopt));
            env.close();

            BEAST_EXPECT(env.ownerCount(bob) == 1);

            // bob cannot create the cftoken the second time
            env(cft::authorize(bob, id.key, std::nullopt), ter(tecCFTOKEN_EXISTS));
            env.close();

            // TODO: check where cftoken balance is nonzero

            env(cft::authorize(bob, id.key, std::nullopt), txflags(tfCFTUnathorize));
            env.close();

            env(cft::authorize(bob, id.key, std::nullopt), txflags(tfCFTUnathorize), ter(tecNO_ENTRY));
            env.close();

            BEAST_EXPECT(env.ownerCount(bob) == 0);
        }

        // Test bad scenarios with allow-listing in CFTokenAuthorize (preclaim)
        {
            Env env{*this, features};
            Account const alice("alice"); //issuer
            Account const bob("bob"); //holder
            Account const cindy("cindy");

            env.fund(XRP(10000), alice, bob);
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 0);

            auto const id = keylet::cftIssuance(alice.id(), env.seq(alice));
            env(cft::create(alice), txflags(tfCFTRequireAuth));
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 1);

            // alice submits a tx without specifying a holder's account
            env(cft::authorize(alice, id.key, std::nullopt), ter(temMALFORMED));
            env.close();

            // alice submits a tx to authorize a holder that hasn't created a cftoken yet
            env(cft::authorize(alice, id.key, bob), ter(tecNO_ENTRY));
            env.close();

            // alice specifys a holder acct that doesn't exist
            env(cft::authorize(alice, id.key, cindy), ter(tecNO_DST));
            env.close();

            // bob now holds a cftoken object
            env(cft::authorize(bob, id.key, std::nullopt));
            env.close();

            BEAST_EXPECT(env.ownerCount(bob) == 1);

            // alice submits a tx to "unauthorize" a holder that hasn't been authorized
            env(cft::authorize(alice, id.key, bob), txflags(tfCFTUnathorize), ter(temINVALID_FLAG));
            env.close();  

            // alice authorizes and set flag on bob's cftoken
            env(cft::authorize(alice, id.key, bob));
            env.close();  

            // if alice tries to set again, it will fail
            env(cft::authorize(alice, id.key, bob), ter(tecCFTOKEN_ALREADY_AUTHORIZED));
            env.close();        

            env(cft::authorize(bob, id.key, std::nullopt), txflags(tfCFTUnathorize));
            env.close();

            BEAST_EXPECT(env.ownerCount(bob) == 0);
        }
        
        // Test cftoken reserve requirement - first two cfts free (doApply)
        {
            Env env{*this, features};
            auto const acctReserve = env.current()->fees().accountReserve(0);
            auto const incReserve = env.current()->fees().increment;

            Account const alice("alice");
            Account const bob("bob");

            env.fund(XRP(10000), alice);
            env.fund(acctReserve + XRP(1), bob);
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 0);

            auto const id1 = keylet::cftIssuance(alice.id(), env.seq(alice));
            env(cft::create(alice));
            env.close();

            auto const id2 = keylet::cftIssuance(alice.id(), env.seq(alice));
            env(cft::create(alice));
            env.close();

            auto const id3 = keylet::cftIssuance(alice.id(), env.seq(alice));
            env(cft::create(alice));
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 3);

            // first cft for free
            env(cft::authorize(bob, id1.key, std::nullopt));
            env.close();

            BEAST_EXPECT(env.ownerCount(bob) == 1);

            // second cft free
            env(cft::authorize(bob, id2.key, std::nullopt));
            env.close();
            BEAST_EXPECT(env.ownerCount(bob) == 2);

            env(cft::authorize(bob, id3.key, std::nullopt), ter(tecINSUFFICIENT_RESERVE));
            env.close();

            env(pay(env.master, bob, drops(incReserve + incReserve + incReserve)));
            env.close();

            env(cft::authorize(bob, id3.key, std::nullopt));
            env.close();

            BEAST_EXPECT(env.ownerCount(bob) == 3);
        }
    }

    void
    testBasicAuthorize(FeatureBitset features)
    {
        testcase("Basic authorize");
        
        using namespace test::jtx;
        // Basic authorization without allowlisting
        {
            Env env{*this, features};
            Account const alice("alice");
            Account const bob("bob");

            env.fund(XRP(10000), alice, bob);
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 0);

            auto const id = keylet::cftIssuance(alice.id(), env.seq(alice));
            env(cft::create(alice));
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 1);

            env(cft::authorize(bob, id.key, std::nullopt));
            env.close();

            BEAST_EXPECT(env.ownerCount(bob) == 1);

            BEAST_EXPECT(!cftIsAuthorized(env, id.key, bob));

            env(cft::authorize(bob, id.key, std::nullopt), txflags(tfCFTUnathorize));
            env.close();

            BEAST_EXPECT(env.ownerCount(bob) == 0);
        }

        // With allowlisting 
        {
            Env env{*this, features};
            Account const alice("alice");
            Account const bob("bob");

            env.fund(XRP(10000), alice, bob);
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 0);

            auto const id = keylet::cftIssuance(alice.id(), env.seq(alice));
            env(cft::create(alice), txflags(tfCFTRequireAuth));
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 1);

            env(cft::authorize(bob, id.key, std::nullopt));
            env.close();

            BEAST_EXPECT(env.ownerCount(bob) == 1);

            BEAST_EXPECT(!cftIsAuthorized(env, id.key, bob));

            env(cft::authorize(alice, id.key, bob));
            env.close();

            BEAST_EXPECT(cftIsAuthorized(env, id.key, bob));

            BEAST_EXPECT(env.ownerCount(bob) == 1);

            env(cft::authorize(bob, id.key, std::nullopt), txflags(tfCFTUnathorize));
            env.close();

            BEAST_EXPECT(env.ownerCount(bob) == 0);
        }

    }

public:
    void
    run() override
    {
        using namespace test::jtx;
        FeatureBitset const all{supported_amendments()};

        testEnabled(all);
        testAuthorizeValidation(all);
        testBasicAuthorize(all);
    }
};

BEAST_DEFINE_TESTSUITE_PRIO(CFToken, tx, ripple, 2);

}  // namespace ripple
