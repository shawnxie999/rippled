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

class MPToken_test : public beast::unit_test::suite
{
    bool
    checkMPTokenAmount(
        test::jtx::Env const& env,
        ripple::uint256 const mptIssuanceid,
        test::jtx::Account const& holder,
        std::uint64_t expectedAmount)
    {
        auto const sleMpt = env.le(keylet::mptoken(mptIssuanceid, holder));
        if (!sleMpt)
            return false;

        std::uint64_t const amount = (*sleMpt)[sfMPTAmount];
        return amount == expectedAmount;
    }

    bool
    checkMPTokenIssuanceFlags(
        test::jtx::Env const& env,
        ripple::uint256 const mptIssuanceid,
        uint32_t const expectedFlags)
    {
        auto const sleMptIssuance = env.le(keylet::mptIssuance(mptIssuanceid));
        if (!sleMptIssuance)
            return false;

        uint32_t const mptIssuanceFlags = sleMptIssuance->getFlags();
        return expectedFlags == mptIssuanceFlags;
    }

    bool
    checkMPTokenFlags(
        test::jtx::Env const& env,
        ripple::uint256 const mptIssuanceid,
        test::jtx::Account const& holder,
        uint32_t const expectedFlags)
    {
        auto const sleMpt = env.le(keylet::mptoken(mptIssuanceid, holder));
        if (!sleMpt)
            return false;
        uint32_t const mptFlags = sleMpt->getFlags();
        return mptFlags == expectedFlags;
    }

    void
    testCreateValidation(FeatureBitset features)
    {
        testcase("Create Validate");
        using namespace test::jtx;

        // test preflight of MPTokenIssuanceCreate
        {
            // If the MPT amendment is not enabled, you should not be able to
            // create MPTokenIssuances
            Env env{*this, features - featureMPTokensV1};
            Account const alice("alice");  // issuer

            env.fund(XRP(10000), alice);
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 0);

            env(mpt::create(alice), ter(temDISABLED));
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 0);

            env.enableFeature(featureMPTokensV1);

            env(mpt::create(alice), txflags(0x00000001), ter(temINVALID_FLAG));
            env.close();

            // tries to set a txfee while not enabling in the flag
            env(mpt::create(alice, 100, 0, 1, "test"), ter(temMALFORMED));
            env.close();

            // tries to set a txfee while not enabling transfer
            env(mpt::create(alice, 100, 0, maxTransferFee + 1, "test"),
                txflags(tfMPTCanTransfer),
                ter(temBAD_MPTOKEN_TRANSFER_FEE));
            env.close();

            // empty metadata returns error
            env(mpt::create(alice, 100, 0, 0, ""), ter(temMALFORMED));
            env.close();
        }
    }

    void
    testCreateEnabled(FeatureBitset features)
    {
        testcase("Create Enabled");

        using namespace test::jtx;

        {
            // If the MPT amendment IS enabled, you should be able to create
            // MPTokenIssuances
            Env env{*this, features};
            Account const alice("alice");  // issuer

            env.fund(XRP(10000), alice);
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 0);

            auto const id = keylet::mptIssuance(alice.id(), env.seq(alice));
            env(mpt::create(alice, 100, 1, 10, "123"),
                txflags(
                    tfMPTCanLock | tfMPTRequireAuth | tfMPTCanEscrow |
                    tfMPTCanTrade | tfMPTCanTransfer | tfMPTCanClawback));
            env.close();

            BEAST_EXPECT(checkMPTokenIssuanceFlags(
                env,
                id.key,
                lsfMPTCanLock | lsfMPTRequireAuth | lsfMPTCanEscrow |
                    lsfMPTCanTrade | lsfMPTCanTransfer | lsfMPTCanClawback));

            BEAST_EXPECT(env.ownerCount(alice) == 1);
        }
    }

    void
    testDestroyValidation(FeatureBitset features)
    {
        testcase("Destroy Validate");

        using namespace test::jtx;
        // MPTokenIssuanceDestroy (preflight)
        {
            Env env{*this, features - featureMPTokensV1};
            Account const alice("alice");  // issuer

            env.fund(XRP(10000), alice);
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 0);

            auto const id = keylet::mptIssuance(alice.id(), env.seq(alice));
            env(mpt::destroy(alice, id.key), ter(temDISABLED));
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 0);

            env.enableFeature(featureMPTokensV1);

            env(mpt::destroy(alice, id.key),
                txflags(0x00000001),
                ter(temINVALID_FLAG));
            env.close();
        }

        // MPTokenIssuanceDestroy (preclaim)
        {
            Env env{*this, features};
            Account const alice("alice");  // issuer
            Account const bob("bob");      // holder

            env.fund(XRP(10000), alice, bob);
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 0);

            auto const fakeID = keylet::mptIssuance(alice.id(), env.seq(alice));

            env(mpt::destroy(alice, fakeID.key), ter(tecOBJECT_NOT_FOUND));
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 0);

            auto const id = keylet::mptIssuance(alice.id(), env.seq(alice));
            env(mpt::create(alice));
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 1);

            // a non-issuer tries to destroy a mptissuance they didn't issue
            env(mpt::destroy(bob, id.key), ter(tecNO_PERMISSION));
            env.close();

            // TODO: add test when OutstandingAmount is non zero
        }
    }

    void
    testDestroyEnabled(FeatureBitset features)
    {
        testcase("Destroy Enabled");

        using namespace test::jtx;

        // If the MPT amendment IS enabled, you should be able to destroy
        // MPTokenIssuances
        Env env{*this, features};
        Account const alice("alice");  // issuer

        env.fund(XRP(10000), alice);
        env.close();

        BEAST_EXPECT(env.ownerCount(alice) == 0);

        auto const id = keylet::mptIssuance(alice.id(), env.seq(alice));
        env(mpt::create(alice));
        env.close();

        BEAST_EXPECT(env.ownerCount(alice) == 1);

        env(mpt::destroy(alice, id.key));
        env.close();
        BEAST_EXPECT(env.ownerCount(alice) == 0);
    }

    void
    testAuthorizeValidation(FeatureBitset features)
    {
        testcase("Validate authorize transaction");

        using namespace test::jtx;
        // Validate fields in MPTokenAuthorize (preflight)
        {
            Env env{*this, features - featureMPTokensV1};
            Account const alice("alice");  // issuer
            Account const bob("bob");      // holder

            env.fund(XRP(10000), alice, bob);
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 0);

            auto const id = keylet::mptIssuance(alice.id(), env.seq(alice));

            env(mpt::authorize(bob, id.key, std::nullopt), ter(temDISABLED));
            env.close();

            env.enableFeature(featureMPTokensV1);

            env(mpt::create(alice));
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 1);

            env(mpt::authorize(bob, id.key, std::nullopt),
                txflags(0x00000002),
                ter(temINVALID_FLAG));
            env.close();

            env(mpt::authorize(bob, id.key, bob), ter(temMALFORMED));
            env.close();

            env(mpt::authorize(alice, id.key, alice), ter(temMALFORMED));
            env.close();
        }

        // Try authorizing when MPTokenIssuance doesnt exist in MPTokenAuthorize
        // (preclaim)
        {
            Env env{*this, features};
            Account const alice("alice");  // issuer
            Account const bob("bob");      // holder

            env.fund(XRP(10000), alice, bob);
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 0);

            auto const id = keylet::mptIssuance(alice.id(), env.seq(alice));

            env(mpt::authorize(alice, id.key, bob), ter(tecOBJECT_NOT_FOUND));
            env.close();

            env(mpt::authorize(bob, id.key, std::nullopt),
                ter(tecOBJECT_NOT_FOUND));
            env.close();
        }

        // Test bad scenarios without allowlisting in MPTokenAuthorize
        // (preclaim)
        {
            Env env{*this, features};
            Account const alice("alice");  // issuer
            Account const bob("bob");      // holder

            env.fund(XRP(10000), alice, bob);
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 0);

            auto const id = keylet::mptIssuance(alice.id(), env.seq(alice));
            env(mpt::create(alice));
            env.close();

            BEAST_EXPECT(checkMPTokenIssuanceFlags(env, id.key, 0));

            BEAST_EXPECT(env.ownerCount(alice) == 1);

            // bob submits a tx with a holder field
            env(mpt::authorize(bob, id.key, alice), ter(temMALFORMED));
            env.close();

            env(mpt::authorize(bob, id.key, bob), ter(temMALFORMED));
            env.close();

            env(mpt::authorize(alice, id.key, alice), ter(temMALFORMED));
            env.close();

            // the mpt does not enable allowlisting
            env(mpt::authorize(alice, id.key, bob), ter(tecNO_AUTH));
            env.close();

            // bob now holds a mptoken object
            env(mpt::authorize(bob, id.key, std::nullopt));
            env.close();

            BEAST_EXPECT(env.ownerCount(bob) == 1);

            // bob cannot create the mptoken the second time
            env(mpt::authorize(bob, id.key, std::nullopt),
                ter(tecMPTOKEN_EXISTS));
            env.close();

            // TODO: check where mptoken balance is nonzero

            env(mpt::authorize(bob, id.key, std::nullopt),
                txflags(tfMPTUnauthorize));
            env.close();

            env(mpt::authorize(bob, id.key, std::nullopt),
                txflags(tfMPTUnauthorize),
                ter(tecNO_ENTRY));
            env.close();

            BEAST_EXPECT(env.ownerCount(bob) == 0);
        }

        // Test bad scenarios with allow-listing in MPTokenAuthorize (preclaim)
        {
            Env env{*this, features};
            Account const alice("alice");  // issuer
            Account const bob("bob");      // holder
            Account const cindy("cindy");

            env.fund(XRP(10000), alice, bob);
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 0);

            auto const id = keylet::mptIssuance(alice.id(), env.seq(alice));
            env(mpt::create(alice), txflags(tfMPTRequireAuth));
            env.close();

            BEAST_EXPECT(
                checkMPTokenIssuanceFlags(env, id.key, lsfMPTRequireAuth));

            BEAST_EXPECT(env.ownerCount(alice) == 1);

            // alice submits a tx without specifying a holder's account
            env(mpt::authorize(alice, id.key, std::nullopt), ter(temMALFORMED));
            env.close();

            // alice submits a tx to authorize a holder that hasn't created a
            // mptoken yet
            env(mpt::authorize(alice, id.key, bob), ter(tecNO_ENTRY));
            env.close();

            // alice specifys a holder acct that doesn't exist
            env(mpt::authorize(alice, id.key, cindy), ter(tecNO_DST));
            env.close();

            // bob now holds a mptoken object
            env(mpt::authorize(bob, id.key, std::nullopt));
            env.close();

            BEAST_EXPECT(env.ownerCount(bob) == 1);

            BEAST_EXPECT(checkMPTokenFlags(env, id.key, bob, 0));

            // alice tries to unauthorize bob.
            // although tx is successful,
            // but nothing happens because bob hasn't been authorized yet
            env(mpt::authorize(alice, id.key, bob), txflags(tfMPTUnauthorize));
            env.close();
            BEAST_EXPECT(checkMPTokenFlags(env, id.key, bob, 0));

            // alice authorizes bob
            // make sure bob's mptoken has set lsfMPTAuthorized
            env(mpt::authorize(alice, id.key, bob));
            env.close();
            BEAST_EXPECT(checkMPTokenFlags(env, id.key, bob, lsfMPTAuthorized));

            // alice tries authorizes bob again.
            // tx is successful, but bob is already authorized,
            // so no changes
            env(mpt::authorize(alice, id.key, bob));
            env.close();
            BEAST_EXPECT(checkMPTokenFlags(env, id.key, bob, lsfMPTAuthorized));

            // bob deletes his mptoken
            env(mpt::authorize(bob, id.key, std::nullopt),
                txflags(tfMPTUnauthorize));
            env.close();

            BEAST_EXPECT(env.ownerCount(bob) == 0);
        }

        // Test mptoken reserve requirement - first two mpts free (doApply)
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

            auto const id1 = keylet::mptIssuance(alice.id(), env.seq(alice));
            env(mpt::create(alice));
            env.close();

            auto const id2 = keylet::mptIssuance(alice.id(), env.seq(alice));
            env(mpt::create(alice));
            env.close();

            auto const id3 = keylet::mptIssuance(alice.id(), env.seq(alice));
            env(mpt::create(alice));
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 3);

            // first mpt for free
            env(mpt::authorize(bob, id1.key, std::nullopt));
            env.close();

            BEAST_EXPECT(env.ownerCount(bob) == 1);

            // second mpt free
            env(mpt::authorize(bob, id2.key, std::nullopt));
            env.close();
            BEAST_EXPECT(env.ownerCount(bob) == 2);

            env(mpt::authorize(bob, id3.key, std::nullopt),
                ter(tecINSUFFICIENT_RESERVE));
            env.close();

            env(pay(
                env.master, bob, drops(incReserve + incReserve + incReserve)));
            env.close();

            env(mpt::authorize(bob, id3.key, std::nullopt));
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

            // alice create mptissuance without allowisting
            auto const id = keylet::mptIssuance(alice.id(), env.seq(alice));
            env(mpt::create(alice));
            env.close();

            BEAST_EXPECT(checkMPTokenIssuanceFlags(env, id.key, 0));

            BEAST_EXPECT(env.ownerCount(alice) == 1);

            // bob creates a mptoken
            env(mpt::authorize(bob, id.key, std::nullopt));
            env.close();

            BEAST_EXPECT(env.ownerCount(bob) == 1);

            BEAST_EXPECT(checkMPTokenFlags(env, id.key, bob, 0));
            BEAST_EXPECT(checkMPTokenAmount(env, id.key, bob, 0));

            // bob deletes his mptoken
            env(mpt::authorize(bob, id.key, std::nullopt),
                txflags(tfMPTUnauthorize));
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

            // alice creates a mptokenissuance that requires authorization
            auto const id = keylet::mptIssuance(alice.id(), env.seq(alice));
            env(mpt::create(alice), txflags(tfMPTRequireAuth));
            env.close();

            BEAST_EXPECT(
                checkMPTokenIssuanceFlags(env, id.key, lsfMPTRequireAuth));

            BEAST_EXPECT(env.ownerCount(alice) == 1);

            // bob creates a mptoken
            env(mpt::authorize(bob, id.key, std::nullopt));
            env.close();

            BEAST_EXPECT(env.ownerCount(bob) == 1);

            BEAST_EXPECT(checkMPTokenFlags(env, id.key, bob, 0));
            BEAST_EXPECT(checkMPTokenAmount(env, id.key, bob, 0));

            // alice authorizes bob
            env(mpt::authorize(alice, id.key, bob));
            env.close();

            // make sure bob's mptoken has lsfMPTAuthorized set
            BEAST_EXPECT(checkMPTokenFlags(env, id.key, bob, lsfMPTAuthorized));

            // Unauthorize bob's mptoken
            env(mpt::authorize(alice, id.key, bob), txflags(tfMPTUnauthorize));
            env.close();

            // ensure bob's mptoken no longer has lsfMPTAuthorized set
            BEAST_EXPECT(checkMPTokenFlags(env, id.key, bob, 0));

            BEAST_EXPECT(env.ownerCount(bob) == 1);

            env(mpt::authorize(bob, id.key, std::nullopt),
                txflags(tfMPTUnauthorize));
            env.close();

            BEAST_EXPECT(env.ownerCount(bob) == 0);
        }

        // TODO: test allowlisting cases where bob tries to send tokens
        //       without being authorized.
    }

    void
    testSetValidation(FeatureBitset features)
    {
        testcase("Validate set transaction");

        using namespace test::jtx;
        // Validate fields in MPTokenIssuanceSet (preflight)
        {
            Env env{*this, features - featureMPTokensV1};
            Account const alice("alice");  // issuer
            Account const bob("bob");      // holder

            env.fund(XRP(10000), alice, bob);
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 0);

            auto const id = keylet::mptIssuance(alice.id(), env.seq(alice));

            env(mpt::set(bob, id.key, std::nullopt), ter(temDISABLED));
            env.close();

            env.enableFeature(featureMPTokensV1);

            env(mpt::create(alice));
            env.close();

            BEAST_EXPECT(checkMPTokenIssuanceFlags(env, id.key, 0));

            BEAST_EXPECT(env.ownerCount(alice) == 1);
            BEAST_EXPECT(env.ownerCount(bob) == 0);

            env(mpt::authorize(bob, id.key, std::nullopt));
            env.close();

            BEAST_EXPECT(env.ownerCount(bob) == 1);

            // test invalid flag
            env(mpt::set(alice, id.key, std::nullopt),
                txflags(0x00000008),
                ter(temINVALID_FLAG));
            env.close();

            // set both lock and unlock flags at the same time will fail
            env(mpt::set(alice, id.key, std::nullopt),
                txflags(tfMPTLock | tfMPTUnlock),
                ter(temINVALID_FLAG));
            env.close();

            // if the holder is the same as the acct that submitted the tx, tx
            // fails
            env(mpt::set(alice, id.key, alice),
                txflags(tfMPTLock),
                ter(temMALFORMED));
            env.close();
        }

        // Validate fields in MPTokenIssuanceSet (preclaim)
        // test when a mptokenissuance has disabled locking
        {
            Env env{*this, features};
            Account const alice("alice");  // issuer
            Account const bob("bob");      // holder
            Account const cindy("cindy");

            env.fund(XRP(10000), alice, bob);
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 0);

            auto const id = keylet::mptIssuance(alice.id(), env.seq(alice));

            env(mpt::create(alice));  // no locking
            env.close();

            BEAST_EXPECT(checkMPTokenIssuanceFlags(env, id.key, 0));

            BEAST_EXPECT(env.ownerCount(alice) == 1);

            // alice tries to lock a mptissuance that has disabled locking
            env(mpt::set(alice, id.key, std::nullopt),
                txflags(tfMPTLock),
                ter(tecNO_PERMISSION));
            env.close();

            // alice tries to unlock mptissuance that has disabled locking
            env(mpt::set(alice, id.key, std::nullopt),
                txflags(tfMPTUnlock),
                ter(tecNO_PERMISSION));
            env.close();

            // issuer tries to lock a bob's mptoken that has disabled locking
            env(mpt::set(alice, id.key, bob),
                txflags(tfMPTLock),
                ter(tecNO_PERMISSION));
            env.close();

            // issuer tries to unlock a bob's mptoken that has disabled locking
            env(mpt::set(alice, id.key, bob),
                txflags(tfMPTUnlock),
                ter(tecNO_PERMISSION));
            env.close();
        }

        // Validate fields in MPTokenIssuanceSet (preclaim)
        // test when mptokenissuance has enabled locking
        {
            Env env{*this, features};
            Account const alice("alice");  // issuer
            Account const bob("bob");      // holder
            Account const cindy("cindy");

            env.fund(XRP(10000), alice, bob);
            env.close();

            BEAST_EXPECT(env.ownerCount(alice) == 0);

            auto const badID = keylet::mptIssuance(alice.id(), env.seq(alice));

            // alice trying to set when the mptissuance doesn't exist yet
            env(mpt::set(alice, badID.key, std::nullopt),
                txflags(tfMPTLock),
                ter(tecOBJECT_NOT_FOUND));
            env.close();

            auto const id = keylet::mptIssuance(alice.id(), env.seq(alice));

            // create a mptokenissuance with locking
            env(mpt::create(alice), txflags(tfMPTCanLock));
            env.close();

            BEAST_EXPECT(checkMPTokenIssuanceFlags(env, id.key, lsfMPTCanLock));

            BEAST_EXPECT(env.ownerCount(alice) == 1);

            // a non-issuer acct tries to set the mptissuance
            env(mpt::set(bob, id.key, std::nullopt),
                txflags(tfMPTLock),
                ter(tecNO_PERMISSION));
            env.close();

            // trying to set a holder who doesn't have a mptoken
            env(mpt::set(alice, id.key, bob),
                txflags(tfMPTLock),
                ter(tecOBJECT_NOT_FOUND));
            env.close();

            // trying to set a holder who doesn't exist
            env(mpt::set(alice, id.key, cindy),
                txflags(tfMPTLock),
                ter(tecNO_DST));
            env.close();
        }
    }

    void
    testSetEnabled(FeatureBitset features)
    {
        testcase("Enabled set transaction");

        using namespace test::jtx;

        // Test locking and unlocking
        Env env{*this, features};
        Account const alice("alice");  // issuer
        Account const bob("bob");      // holder

        env.fund(XRP(10000), alice, bob);
        env.close();

        BEAST_EXPECT(env.ownerCount(alice) == 0);

        auto const id = keylet::mptIssuance(alice.id(), env.seq(alice));

        // create a mptokenissuance with locking
        env(mpt::create(alice), txflags(tfMPTCanLock));
        env.close();

        BEAST_EXPECT(checkMPTokenIssuanceFlags(env, id.key, lsfMPTCanLock));

        BEAST_EXPECT(env.ownerCount(alice) == 1);
        BEAST_EXPECT(env.ownerCount(bob) == 0);

        env(mpt::authorize(bob, id.key, std::nullopt));
        env.close();

        BEAST_EXPECT(env.ownerCount(bob) == 1);
        env.close();

        // both the mptissuance and mptoken are not locked
        BEAST_EXPECT(checkMPTokenIssuanceFlags(env, id.key, lsfMPTCanLock));
        BEAST_EXPECT(checkMPTokenFlags(env, id.key, bob, 0));

        // locks bob's mptoken
        env(mpt::set(alice, id.key, bob), txflags(tfMPTLock));
        env.close();

        BEAST_EXPECT(checkMPTokenIssuanceFlags(env, id.key, lsfMPTCanLock));
        BEAST_EXPECT(checkMPTokenFlags(env, id.key, bob, lsfMPTLocked));

        // trying to lock bob's mptoken again will still succeed
        // but no changes to the objects
        env(mpt::set(alice, id.key, bob), txflags(tfMPTLock));
        env.close();

        // no changes to the objects
        BEAST_EXPECT(checkMPTokenIssuanceFlags(env, id.key, lsfMPTCanLock));
        BEAST_EXPECT(checkMPTokenFlags(env, id.key, bob, lsfMPTLocked));

        // alice locks the mptissuance
        env(mpt::set(alice, id.key, std::nullopt), txflags(tfMPTLock));
        env.close();

        // now both the mptissuance and mptoken are locked up
        BEAST_EXPECT(checkMPTokenIssuanceFlags(
            env, id.key, lsfMPTCanLock | lsfMPTLocked));
        BEAST_EXPECT(checkMPTokenFlags(env, id.key, bob, lsfMPTLocked));

        // alice tries to lock up both mptissuance and mptoken again
        // it will not change the flags and both will remain locked.
        env(mpt::set(alice, id.key, std::nullopt), txflags(tfMPTLock));
        env.close();
        env(mpt::set(alice, id.key, bob), txflags(tfMPTLock));
        env.close();

        // now both the mptissuance and mptoken remain locked up
        BEAST_EXPECT(checkMPTokenIssuanceFlags(
            env, id.key, lsfMPTCanLock | lsfMPTLocked));
        BEAST_EXPECT(checkMPTokenFlags(env, id.key, bob, lsfMPTLocked));

        // alice unlocks bob's mptoken
        env(mpt::set(alice, id.key, bob), txflags(tfMPTUnlock));
        env.close();

        // only mptissuance is locked
        BEAST_EXPECT(checkMPTokenIssuanceFlags(
            env, id.key, lsfMPTCanLock | lsfMPTLocked));
        BEAST_EXPECT(checkMPTokenFlags(env, id.key, bob, 0));

        // locks up bob's mptoken again
        env(mpt::set(alice, id.key, bob), txflags(tfMPTLock));
        env.close();

        // now both the mptissuance and mptokens are locked up
        BEAST_EXPECT(checkMPTokenIssuanceFlags(
            env, id.key, lsfMPTCanLock | lsfMPTLocked));
        BEAST_EXPECT(checkMPTokenFlags(env, id.key, bob, lsfMPTLocked));

        // alice unlocks mptissuance
        env(mpt::set(alice, id.key, std::nullopt), txflags(tfMPTUnlock));
        env.close();

        // now mptissuance is unlocked
        BEAST_EXPECT(checkMPTokenIssuanceFlags(env, id.key, lsfMPTCanLock));
        BEAST_EXPECT(checkMPTokenFlags(env, id.key, bob, lsfMPTLocked));

        // alice unlocks bob's mptoken
        env(mpt::set(alice, id.key, bob), txflags(tfMPTUnlock));
        env.close();

        // both mptissuance and bob's mptoken are unlocked
        BEAST_EXPECT(checkMPTokenIssuanceFlags(env, id.key, lsfMPTCanLock));
        BEAST_EXPECT(checkMPTokenFlags(env, id.key, bob, 0));

        // alice unlocks mptissuance and bob's mptoken again despite that
        // they are already unlocked. Make sure this will not change the
        // flags
        env(mpt::set(alice, id.key, bob), txflags(tfMPTUnlock));
        env.close();
        env(mpt::set(alice, id.key, std::nullopt), txflags(tfMPTUnlock));
        env.close();

        // both mptissuance and bob's mptoken remain unlocked
        BEAST_EXPECT(checkMPTokenIssuanceFlags(env, id.key, lsfMPTCanLock));
        BEAST_EXPECT(checkMPTokenFlags(env, id.key, bob, 0));
    }

public:
    void
    run() override
    {
        using namespace test::jtx;
        FeatureBitset const all{supported_amendments()};

        // MPTokenIssuanceCreate
        testCreateValidation(all);
        testCreateEnabled(all);

        // MPTokenIssuanceDestroy
        testDestroyValidation(all);
        testDestroyEnabled(all);

        // MPTokenAuthorize
        testAuthorizeValidation(all);
        testAuthorizeEnabled(all);

        // MPTokenIssuanceSet
        testSetValidation(all);
        testSetEnabled(all);
    }
};

BEAST_DEFINE_TESTSUITE_PRIO(MPToken, tx, ripple, 2);

}  // namespace ripple
