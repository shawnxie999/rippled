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
        ripple::uint256 const cftIssuanceid,
            test::jtx::Account const& holder, std::uint64_t expectedAmount){
        auto const sleCft = env.le(keylet::cftoken(cftIssuanceid, holder));
        std::uint64_t const amount = (*sleCft)[sfCFTAmount];
        return amount == expectedAmount;
    }

    bool cftIssuanceHasFlags(test::jtx::Env const& env, ripple::uint256 const cftIssuanceid, uint32_t const expectedFlags){
        auto const sleCftIssuance = env.le(keylet::cftIssuance(cftIssuanceid));
        if(!sleCftIssuance)
            return false;
        uint32_t const cftIssuanceFlags = sleCftIssuance->getFlags();
        return expectedFlags == cftIssuanceFlags;
    }

    bool   
    cftokenHasFlags(test::jtx::Env const& env,
        ripple::uint256 const cftIssuanceid,
            test::jtx::Account const& holder, uint32_t const expectedFlags)
    {
            auto const sleCft = env.le(keylet::cftoken(cftIssuanceid, holder));
            if(!sleCft)
                return false;
            uint32_t const cftFlags = sleCft->getFlags();
            return cftFlags == expectedFlags;
    }

    bool   
    cftIssuanceIsLocked(test::jtx::Env const& env,
        ripple::uint256 const cftIssuanceid)
    {
        auto const sleCftIssuance = env.le(keylet::cftIssuance(cftIssuanceid));
        uint32_t const cftIssuanceFlags = sleCftIssuance->getFlags();
        return cftIssuanceFlags & lsfCFTLocked;
    }

    void
    testCreateEnabled(FeatureBitset features)
    {
        testcase("Create Enabled");

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
    }

    void
    testDestoryEnabled(FeatureBitset features)
    {
        testcase("Destory Enabled");

        using namespace test::jtx;
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
        testcase("Validate authorize transaction");

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
    testAuthorizeEnabled(FeatureBitset features)
    {
        testcase("Authorize Enabled");
        
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

            BEAST_EXPECT(cftokenHasFlags(env, id.key, bob, 0));
            BEAST_EXPECT(cftEqualsAmount(env, id.key, bob, 0));

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

            BEAST_EXPECT(cftokenHasFlags(env, id.key, bob, 0));
            BEAST_EXPECT(cftEqualsAmount(env, id.key, bob, 0));

            env(cft::authorize(alice, id.key, bob));
            env.close();

            BEAST_EXPECT(cftokenHasFlags(env, id.key, bob, lsfCFTAuthorized));
            BEAST_EXPECT(cftEqualsAmount(env, id.key, bob, 0));

            BEAST_EXPECT(env.ownerCount(bob) == 1);

            env(cft::authorize(bob, id.key, std::nullopt), txflags(tfCFTUnathorize));
            env.close();

            BEAST_EXPECT(env.ownerCount(bob) == 0);
        }
    }

    void
    testSetValidation(FeatureBitset features)
    {
        testcase("Validate set transaction");

        using namespace test::jtx;
        // Validate fields in CFTokenIssuanceSet (preflight)
        {
            Env env{*this, features - featureCFTokensV1};
            Account const alice("alice"); //issuer
            Account const bob("bob"); //holder

            env.fund(XRP(10000), alice, bob);
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 0);

            auto const id = keylet::cftIssuance(alice.id(), env.seq(alice));

            env(cft::set(bob, id.key, std::nullopt), ter(temDISABLED));
            env.close();

            env.enableFeature(featureCFTokensV1);

            env(cft::create(alice));
            env.close();
            
            BEAST_EXPECT(env.ownerCount(alice) == 1);
            BEAST_EXPECT(env.ownerCount(bob) == 0);

            env(cft::authorize(bob, id.key, std::nullopt));
            env.close();

            BEAST_EXPECT(env.ownerCount(bob) == 1);

            // test invalid flag
            env(cft::set(alice, id.key, std::nullopt), txflags(0x00000008), ter(temINVALID_FLAG));
            env.close();

            // set both lock and unlock flags at the same time will fail
            env(cft::set(alice, id.key, std::nullopt), txflags(tfCFTLock | tfCFTUnlock), ter(temINVALID_FLAG));
            env.close();

            // if the holder is the same as the acct that submitted the tx, tx fails
            env(cft::set(alice, id.key, alice), txflags(tfCFTLock), ter(temMALFORMED));
            env.close();
        }

        // Validate fields in CFTokenIssuanceSet (preclaim)
        // test when a cftokenissuance has disabled locking
        {
            Env env{*this, features };
            Account const alice("alice"); //issuer
            Account const bob("bob"); //holder
            Account const cindy("cindy");

            env.fund(XRP(10000), alice, bob);
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 0);

            auto const id = keylet::cftIssuance(alice.id(), env.seq(alice));

            env(cft::create(alice)); // no locking
            env.close();
            
            BEAST_EXPECT(env.ownerCount(alice) == 1);

            // alice tries to lock a cftissuance that has disabled locking
            env(cft::set(alice, id.key, std::nullopt), txflags(tfCFTLock), ter(tecNO_PERMISSION));
            env.close();

            // alice tries to unlock cftissuance that has disabled locking
            env(cft::set(alice, id.key, std::nullopt), txflags(tfCFTUnlock), ter(tecNO_PERMISSION));
            env.close();

            // issuer tries to lock a bob's cftoken that has disabled locking
            env(cft::set(alice, id.key, bob), txflags(tfCFTLock), ter(tecNO_PERMISSION));
            env.close();

            // issuer tries to unlock a bob's cftoken that has disabled locking
            env(cft::set(alice, id.key, bob), txflags(tfCFTUnlock), ter(tecNO_PERMISSION));
            env.close();
        }

        // Validate fields in CFTokenIssuanceSet (preclaim)
        // test when cftokenissuance has enabled locking
        {
            Env env{*this, features };
            Account const alice("alice"); //issuer
            Account const bob("bob"); //holder
            Account const cindy("cindy");

            env.fund(XRP(10000), alice, bob);
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 0);

            auto const badID = keylet::cftIssuance(alice.id(), env.seq(alice));

            // alice trying to set when the cftissuance doesn't exist yet
            env(cft::set(alice, badID.key, std::nullopt), txflags(tfCFTLock), ter(tecOBJECT_NOT_FOUND));
            env.close();

            auto const id = keylet::cftIssuance(alice.id(), env.seq(alice));

            // create a cftokenissuance with locking
            env(cft::create(alice), txflags(tfCFTCanLock));
            env.close();
            
            BEAST_EXPECT(env.ownerCount(alice) == 1);

            // a non-issuer acct tries to set the cftissuance
            env(cft::set(bob, id.key, std::nullopt), txflags(tfCFTLock), ter(tecNO_PERMISSION));
            env.close();

            // trying to set a holder who doesn't have a cftoken
            env(cft::set(alice, id.key, bob), txflags(tfCFTLock), ter(tecOBJECT_NOT_FOUND));
            env.close();

            // trying to set a holder who doesn't exist
            env(cft::set(alice, id.key, cindy), txflags(tfCFTLock), ter(tecOBJECT_NOT_FOUND));
            env.close();
        }
    }

    void
    testSetEnabled(FeatureBitset features)
    {
        testcase("Enabled set transaction");

        using namespace test::jtx;

        // Test locking and unlocking
        {
            Env env{*this, features };
            Account const alice("alice"); //issuer
            Account const bob("bob"); //holder

            env.fund(XRP(10000), alice, bob);
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 0);

            auto const id = keylet::cftIssuance(alice.id(), env.seq(alice));

            // create a cftokenissuance with locking
            env(cft::create(alice), txflags(tfCFTCanLock));
            env.close();
            
            BEAST_EXPECT(env.ownerCount(alice) == 1);
            BEAST_EXPECT(env.ownerCount(bob) == 0);

            env(cft::authorize(bob, id.key, std::nullopt));
            env.close();

            BEAST_EXPECT(env.ownerCount(bob) == 1);
            env.close();

            // both the cftissuance and cftoken are not locked
            BEAST_EXPECT(cftIssuanceHasFlags(env, id.key, tfCFTCanLock));
            BEAST_EXPECT(cftokenHasFlags(env, id.key, bob, 0));

            // locks bob's cftoken
            env(cft::set(alice, id.key, bob), txflags(tfCFTLock));
            env.close();

            BEAST_EXPECT(cftIssuanceHasFlags(env, id.key, tfCFTCanLock));
            BEAST_EXPECT(cftokenHasFlags(env, id.key, bob, lsfCFTLocked));

            // trying to lock bob's cftoken again will still succeed
            // but no changes to the objects
            env(cft::set(alice, id.key, bob), txflags(tfCFTLock));
            env.close();

            // no changes to the objects
            BEAST_EXPECT(cftIssuanceHasFlags(env, id.key, tfCFTCanLock));
            BEAST_EXPECT(cftokenHasFlags(env, id.key, bob, lsfCFTLocked));

            // alice locks the cftissuance
            env(cft::set(alice, id.key, std::nullopt), txflags(tfCFTLock));
            env.close();

            // now both the cftissuance and cftoken are locked up
            BEAST_EXPECT(cftIssuanceHasFlags(env, id.key, tfCFTCanLock | lsfCFTLocked));
            BEAST_EXPECT(cftokenHasFlags(env, id.key, bob, lsfCFTLocked));

            // alice tries to lock up both cftissuance and cftoken again
            // it will not change the flags and both will remain locked.
            env(cft::set(alice, id.key, std::nullopt), txflags(tfCFTLock));
            env.close();
            env(cft::set(alice, id.key, bob), txflags(tfCFTLock));
            env.close();

            // now both the cftissuance and cftoken remain locked up
            BEAST_EXPECT(cftIssuanceHasFlags(env, id.key, tfCFTCanLock | lsfCFTLocked));
            BEAST_EXPECT(cftokenHasFlags(env, id.key, bob, lsfCFTLocked));

            // alice unlocks bob's cftoken
            env(cft::set(alice, id.key, bob), txflags(tfCFTUnlock));
            env.close();

            // only cftissuance is locked
            BEAST_EXPECT(cftIssuanceHasFlags(env, id.key, tfCFTCanLock | lsfCFTLocked));
            BEAST_EXPECT(cftokenHasFlags(env, id.key, bob, 0));

            // locks up bob's cftoken again
            env(cft::set(alice, id.key, bob), txflags(tfCFTLock));
            env.close();

            // now both the cftissuance and cftokens are locked up
            BEAST_EXPECT(cftIssuanceHasFlags(env, id.key, tfCFTCanLock | lsfCFTLocked));
            BEAST_EXPECT(cftokenHasFlags(env, id.key, bob, lsfCFTLocked));

            // alice unlocks cftissuance
            env(cft::set(alice, id.key, std::nullopt), txflags(tfCFTUnlock));
            env.close();

            // now cftissuance is unlocked
            BEAST_EXPECT(cftIssuanceHasFlags(env, id.key, tfCFTCanLock));
            BEAST_EXPECT(cftokenHasFlags(env, id.key, bob, lsfCFTLocked));

            // alice unlocks bob's cftoken
            env(cft::set(alice, id.key, bob), txflags(tfCFTUnlock));
            env.close();

            // both cftissuance and bob's cftoken are unlocked
            BEAST_EXPECT(cftIssuanceHasFlags(env, id.key, tfCFTCanLock));
            BEAST_EXPECT(cftokenHasFlags(env, id.key, bob, 0));

            // alice unlocks cftissuance and bob's cftoken again despite that they are
            // already unlocked. Make sure this will not change the flags
            env(cft::set(alice, id.key, bob), txflags(tfCFTUnlock));
            env.close();
            env(cft::set(alice, id.key, std::nullopt), txflags(tfCFTUnlock));
            env.close();

            // both cftissuance and bob's cftoken remain unlocked
            BEAST_EXPECT(cftIssuanceHasFlags(env, id.key, tfCFTCanLock));
            BEAST_EXPECT(cftokenHasFlags(env, id.key, bob, 0));
        }
    }

public:
    void
    run() override
    {
        using namespace test::jtx;
        FeatureBitset const all{supported_amendments()};

        // CFTokenIssuanceCreate
        testCreateEnabled(all);

        // CFTokenIssuanceDestory
        testDestoryEnabled(all);

        // CFTokenAuthorize
        testAuthorizeValidation(all);
        testAuthorizeEnabled(all);

        // CFTokenIssuanceSet
        testSetValidation(all);
        testSetEnabled(all);
    }
};

BEAST_DEFINE_TESTSUITE_PRIO(CFToken, tx, ripple, 2);

}  // namespace ripple
