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
    void
    testCreateValidation(FeatureBitset features)
    {
        testcase("Create Validate");
        using namespace test::jtx;
        Account const alice("alice");

        // test preflight of MPTokenIssuanceCreate
        {
            // If the MPT amendment is not enabled, you should not be able to
            // create MPTokenIssuances
            Env env{*this, features - featureMPTokensV1};
            MPTTester mptAlice(env, alice);

            mptAlice.create({.ownerCount = 0, .err = temDISABLED});

            env.enableFeature(featureMPTokensV1);

            mptAlice.create({.flags = 0x00000001, .err = temINVALID_FLAG});

            // tries to set a txfee while not enabling in the flag
            mptAlice.create(
                {.maxAmt = 100,
                 .assetScale = 0,
                 .transferFee = 1,
                 .metadata = "test",
                 .err = temMALFORMED});

            // tries to set a txfee while not enabling transfer
            mptAlice.create(
                {.maxAmt = 100,
                 .assetScale = 0,
                 .transferFee = maxTransferFee + 1,
                 .metadata = "test",
                 .flags = tfMPTCanTransfer,
                 .err = temBAD_MPTOKEN_TRANSFER_FEE});

            // empty metadata returns error
            mptAlice.create(
                {.maxAmt = 100,
                 .assetScale = 0,
                 .transferFee = 0,
                 .metadata = "",
                 .err = temMALFORMED});

            // MaximumAmout of 0 returns error
            mptAlice.create(
                {.maxAmt = 0,
                 .assetScale = 1,
                 .transferFee = 1,
                 .metadata = "test",
                 .err = temMALFORMED});

            // MaximumAmount larger than 63 bit returns error
            mptAlice.create(
                {.maxAmt = 0xFFFFFFFFFFFFFFF0ull,
                 .assetScale = 0,
                 .transferFee = 0,
                 .metadata = "test",
                 .err = temMALFORMED});
        }
    }

    void
    testCreateEnabled(FeatureBitset features)
    {
        testcase("Create Enabled");

        using namespace test::jtx;
        Account const alice("alice");

        {
            // If the MPT amendment IS enabled, you should be able to create
            // MPTokenIssuances
            Env env{*this, features};
            MPTTester mptAlice(env, alice);
            mptAlice.create(
                {.maxAmt = 0x7FFFFFFFFFFFFFFF,
                 .assetScale = 1,
                 .transferFee = 10,
                 .metadata = "123",
                 .ownerCount = 1,
                 .flags = tfMPTCanLock | tfMPTRequireAuth | tfMPTCanEscrow |
                     tfMPTCanTrade | tfMPTCanTransfer | tfMPTCanClawback});
        }
    }

    void
    testDestroyValidation(FeatureBitset features)
    {
        testcase("Destroy Validate");

        using namespace test::jtx;
        Account const alice("alice");
        Account const bob("bob");
        // MPTokenIssuanceDestroy (preflight)
        {
            Env env{*this, features - featureMPTokensV1};
            MPTTester mptAlice(env, alice);
            auto const id = getMptID(alice, env.seq(alice));
            mptAlice.destroy({.id = id, .ownerCount = 0, .err = temDISABLED});

            env.enableFeature(featureMPTokensV1);

            mptAlice.destroy(
                {.id = id, .flags = 0x00000001, .err = temINVALID_FLAG});
        }

        // MPTokenIssuanceDestroy (preclaim)
        {
            Env env{*this, features};
            MPTTester mptAlice(env, alice, {.holders = {&bob}});

            mptAlice.destroy(
                {.id = getMptID(alice.id(), env.seq(alice)),
                 .ownerCount = 0,
                 .err = tecOBJECT_NOT_FOUND});

            mptAlice.create({.ownerCount = 1});

            // a non-issuer tries to destroy a mptissuance they didn't issue
            mptAlice.destroy({.issuer = &bob, .err = tecNO_PERMISSION});

            // Make sure that issuer can't delete issuance when it still has
            // outstanding balance
            {
                // bob now holds a mptoken object
                mptAlice.authorize({.account = &bob, .holderCount = 1});

                // alice pays bob 100 tokens
                mptAlice.pay(alice, bob, 100);

                mptAlice.destroy({.err = tecHAS_OBLIGATIONS});
            }
        }
    }

    void
    testDestroyEnabled(FeatureBitset features)
    {
        testcase("Destroy Enabled");

        using namespace test::jtx;
        Account const alice("alice");

        // If the MPT amendment IS enabled, you should be able to destroy
        // MPTokenIssuances
        Env env{*this, features};
        MPTTester mptAlice(env, alice);

        mptAlice.create({.ownerCount = 1});

        mptAlice.destroy({.ownerCount = 0});
    }

    void
    testAuthorizeValidation(FeatureBitset features)
    {
        testcase("Validate authorize transaction");

        using namespace test::jtx;
        Account const alice("alice");
        Account const bob("bob");
        Account const cindy("cindy");
        // Validate fields in MPTokenAuthorize (preflight)
        {
            Env env{*this, features - featureMPTokensV1};
            MPTTester mptAlice(env, alice, {.holders = {&bob}});

            mptAlice.authorize(
                {.account = &bob,
                 .id = getMptID(alice, env.seq(alice)),
                 .err = temDISABLED});

            env.enableFeature(featureMPTokensV1);

            mptAlice.create({.ownerCount = 1});

            mptAlice.authorize(
                {.account = &bob, .flags = 0x00000002, .err = temINVALID_FLAG});

            mptAlice.authorize(
                {.account = &bob, .holder = &bob, .err = temMALFORMED});

            mptAlice.authorize({.holder = &alice, .err = temMALFORMED});
        }

        // Try authorizing when MPTokenIssuance doesn't exist in
        // MPTokenAuthorize (preclaim)
        {
            Env env{*this, features};
            MPTTester mptAlice(env, alice, {.holders = {&bob}});
            auto const id = getMptID(alice, env.seq(alice));

            mptAlice.authorize(
                {.holder = &bob, .id = id, .err = tecOBJECT_NOT_FOUND});

            mptAlice.authorize(
                {.account = &bob, .id = id, .err = tecOBJECT_NOT_FOUND});
        }

        // Test bad scenarios without allowlisting in MPTokenAuthorize
        // (preclaim)
        {
            Env env{*this, features};
            MPTTester mptAlice(env, alice, {.holders = {&bob}});

            mptAlice.create({.ownerCount = 1});

            // bob submits a tx with a holder field
            mptAlice.authorize(
                {.account = &bob, .holder = &alice, .err = temMALFORMED});

            mptAlice.authorize(
                {.account = &bob, .holder = &bob, .err = temMALFORMED});

            mptAlice.authorize({.holder = &alice, .err = temMALFORMED});

            // the mpt does not enable allowlisting
            mptAlice.authorize({.holder = &bob, .err = tecNO_AUTH});

            // bob now holds a mptoken object
            mptAlice.authorize({.account = &bob, .holderCount = 1});

            // bob cannot create the mptoken the second time
            mptAlice.authorize({.account = &bob, .err = tecMPTOKEN_EXISTS});

            // Check that bob cannot delete CFToken when his balance is
            // non-zero
            {
                // alice pays bob 100 tokens
                mptAlice.pay(alice, bob, 100);

                // bob tries to delete his CFToken, but fails since he still
                // holds tokens
                mptAlice.authorize(
                    {.account = &bob,
                     .flags = tfMPTUnauthorize,
                     .err = tecHAS_OBLIGATIONS});

                // bob pays back alice 100 tokens
                mptAlice.pay(bob, alice, 100);
            }

            mptAlice.authorize({.account = &bob, .flags = tfMPTUnauthorize});

            mptAlice.authorize(
                {.account = &bob,
                 .holderCount = 0,
                 .flags = tfMPTUnauthorize,
                 .err = tecNO_ENTRY});
        }

        // Test bad scenarios with allow-listing in MPTokenAuthorize (preclaim)
        {
            Env env{*this, features};
            MPTTester mptAlice(env, alice, {.holders = {&bob}});

            mptAlice.create({.ownerCount = 1, .flags = tfMPTRequireAuth});

            // alice submits a tx without specifying a holder's account
            mptAlice.authorize({.err = temMALFORMED});

            // alice submits a tx to authorize a holder that hasn't created
            // a mptoken yet
            mptAlice.authorize({.holder = &bob, .err = tecNO_ENTRY});

            // alice specifys a holder acct that doesn't exist
            mptAlice.authorize({.holder = &cindy, .err = tecNO_DST});

            // bob now holds a mptoken object
            mptAlice.authorize({.account = &bob, .holderCount = 1});

            BEAST_EXPECT(mptAlice.checkFlags(0, &bob));

            // alice tries to unauthorize bob.
            // although tx is successful,
            // but nothing happens because bob hasn't been authorized yet
            mptAlice.authorize({.holder = &bob, .flags = tfMPTUnauthorize});
            BEAST_EXPECT(mptAlice.checkFlags(0, &bob));

            // alice authorizes bob
            // make sure bob's mptoken has set lsfMPTAuthorized
            mptAlice.authorize({.holder = &bob});
            BEAST_EXPECT(mptAlice.checkFlags(lsfMPTAuthorized, &bob));

            // alice tries authorizes bob again.
            // tx is successful, but bob is already authorized,
            // so no changes
            mptAlice.authorize({.holder = &bob});
            BEAST_EXPECT(mptAlice.checkFlags(lsfMPTAuthorized, &bob));

            // bob deletes his mptoken
            mptAlice.authorize(
                {.account = &bob, .holderCount = 0, .flags = tfMPTUnauthorize});
        }

        // Test mptoken reserve requirement - first two mpts free (doApply)
        {
            Env env{*this, features};
            auto const acctReserve = env.current()->fees().accountReserve(0);
            auto const incReserve = env.current()->fees().increment;

            MPTTester mptAlice1(
                env,
                alice,
                {.holders = {&bob},
                 .xrpHolders = acctReserve + XRP(1).value().xrp()});
            mptAlice1.create();

            MPTTester mptAlice2(env, alice, {.fund = false});
            mptAlice2.create();

            MPTTester mptAlice3(env, alice, {.fund = false});
            mptAlice3.create({.ownerCount = 3});

            // first mpt for free
            mptAlice1.authorize({.account = &bob, .holderCount = 1});

            // second mpt free
            mptAlice2.authorize({.account = &bob, .holderCount = 2});

            mptAlice3.authorize(
                {.account = &bob, .err = tecINSUFFICIENT_RESERVE});

            env(pay(
                env.master, bob, drops(incReserve + incReserve + incReserve)));
            env.close();

            mptAlice3.authorize({.account = &bob, .holderCount = 3});
        }
    }

    void
    testAuthorizeEnabled(FeatureBitset features)
    {
        testcase("Authorize Enabled");

        using namespace test::jtx;
        Account const alice("alice");
        Account const bob("bob");
        // Basic authorization without allowlisting
        {
            Env env{*this, features};

            // alice create mptissuance without allowisting
            MPTTester mptAlice(env, alice, {.holders = {&bob}});

            mptAlice.create({.ownerCount = 1});

            // bob creates a mptoken
            mptAlice.authorize({.account = &bob, .holderCount = 1});

            BEAST_EXPECT(mptAlice.checkFlags(0, &bob));
            BEAST_EXPECT(mptAlice.checkMPTokenAmount(bob, 0));

            // bob deletes his mptoken
            mptAlice.authorize(
                {.account = &bob, .holderCount = 0, .flags = tfMPTUnauthorize});
        }

        // With allowlisting
        {
            Env env{*this, features};

            // alice creates a mptokenissuance that requires authorization
            MPTTester mptAlice(env, alice, {.holders = {&bob}});

            mptAlice.create({.ownerCount = 1, .flags = tfMPTRequireAuth});

            // bob creates a mptoken
            mptAlice.authorize({.account = &bob, .holderCount = 1});

            BEAST_EXPECT(mptAlice.checkFlags(0, &bob));
            BEAST_EXPECT(mptAlice.checkMPTokenAmount(bob, 0));

            // alice authorizes bob
            mptAlice.authorize({.account = &alice, .holder = &bob});

            // make sure bob's mptoken has lsfMPTAuthorized set
            BEAST_EXPECT(mptAlice.checkFlags(lsfMPTAuthorized, &bob));

            // Unauthorize bob's mptoken
            mptAlice.authorize(
                {.account = &alice,
                 .holder = &bob,
                 .holderCount = 1,
                 .flags = tfMPTUnauthorize});

            // ensure bob's mptoken no longer has lsfMPTAuthorized set
            BEAST_EXPECT(mptAlice.checkFlags(0, &bob));

            mptAlice.authorize(
                {.account = &bob, .holderCount = 0, .flags = tfMPTUnauthorize});
        }
    }

    void
    testSetValidation(FeatureBitset features)
    {
        testcase("Validate set transaction");

        using namespace test::jtx;
        Account const alice("alice");  // issuer
        Account const bob("bob");      // holder
        Account const cindy("cindy");
        // Validate fields in MPTokenIssuanceSet (preflight)
        {
            Env env{*this, features - featureMPTokensV1};
            MPTTester mptAlice(env, alice, {.holders = {&bob}});

            mptAlice.set(
                {.account = &bob,
                 .id = getMptID(alice, env.seq(alice)),
                 .err = temDISABLED});

            env.enableFeature(featureMPTokensV1);

            mptAlice.create({.ownerCount = 1, .holderCount = 0});

            mptAlice.authorize({.account = &bob, .holderCount = 1});

            // test invalid flag
            mptAlice.set(
                {.account = &alice,
                 .flags = 0x00000008,
                 .err = temINVALID_FLAG});

            // set both lock and unlock flags at the same time will fail
            mptAlice.set(
                {.account = &alice,
                 .flags = tfMPTLock | tfMPTUnlock,
                 .err = temINVALID_FLAG});

            // if the holder is the same as the acct that submitted the tx,
            // tx fails
            mptAlice.set(
                {.account = &alice,
                 .holder = &alice,
                 .flags = tfMPTLock,
                 .err = temMALFORMED});
        }

        // Validate fields in MPTokenIssuanceSet (preclaim)
        // test when a mptokenissuance has disabled locking
        {
            Env env{*this, features};

            MPTTester mptAlice(env, alice, {.holders = {&bob}});

            mptAlice.create({.ownerCount = 1});

            // alice tries to lock a mptissuance that has disabled locking
            mptAlice.set(
                {.account = &alice,
                 .flags = tfMPTLock,
                 .err = tecNO_PERMISSION});

            // alice tries to unlock mptissuance that has disabled locking
            mptAlice.set(
                {.account = &alice,
                 .flags = tfMPTUnlock,
                 .err = tecNO_PERMISSION});

            // issuer tries to lock a bob's mptoken that has disabled
            // locking
            mptAlice.set(
                {.account = &alice,
                 .holder = &bob,
                 .flags = tfMPTLock,
                 .err = tecNO_PERMISSION});

            // issuer tries to unlock a bob's mptoken that has disabled
            // locking
            mptAlice.set(
                {.account = &alice,
                 .holder = &bob,
                 .flags = tfMPTUnlock,
                 .err = tecNO_PERMISSION});
        }

        // Validate fields in MPTokenIssuanceSet (preclaim)
        // test when mptokenissuance has enabled locking
        {
            Env env{*this, features};

            MPTTester mptAlice(env, alice, {.holders = {&bob}});

            // alice trying to set when the mptissuance doesn't exist yet
            mptAlice.set(
                {.id = getMptID(alice.id(), env.seq(alice)),
                 .flags = tfMPTLock,
                 .err = tecOBJECT_NOT_FOUND});

            // create a mptokenissuance with locking
            mptAlice.create({.ownerCount = 1, .flags = tfMPTCanLock});

            // a non-issuer acct tries to set the mptissuance
            mptAlice.set(
                {.account = &bob, .flags = tfMPTLock, .err = tecNO_PERMISSION});

            // trying to set a holder who doesn't have a mptoken
            mptAlice.set(
                {.holder = &bob,
                 .flags = tfMPTLock,
                 .err = tecOBJECT_NOT_FOUND});

            // trying to set a holder who doesn't exist
            mptAlice.set(
                {.holder = &cindy, .flags = tfMPTLock, .err = tecNO_DST});
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

        MPTTester mptAlice(env, alice, {.holders = {&bob}});

        // create a mptokenissuance with locking
        mptAlice.create(
            {.ownerCount = 1, .holderCount = 0, .flags = tfMPTCanLock});

        mptAlice.authorize({.account = &bob, .holderCount = 1});

        // both the mptissuance and mptoken are not locked
        BEAST_EXPECT(mptAlice.checkFlags(lsfMPTCanLock));
        BEAST_EXPECT(mptAlice.checkFlags(0, &bob));

        // locks bob's mptoken
        mptAlice.set({.account = &alice, .holder = &bob, .flags = tfMPTLock});

        BEAST_EXPECT(mptAlice.checkFlags(lsfMPTCanLock));
        BEAST_EXPECT(mptAlice.checkFlags(lsfMPTLocked, &bob));

        // trying to lock bob's mptoken again will still succeed
        // but no changes to the objects
        mptAlice.set({.account = &alice, .holder = &bob, .flags = tfMPTLock});

        // no changes to the objects
        BEAST_EXPECT(mptAlice.checkFlags(lsfMPTCanLock));
        BEAST_EXPECT(mptAlice.checkFlags(lsfMPTLocked, &bob));

        // alice locks the mptissuance
        mptAlice.set({.account = &alice, .flags = tfMPTLock});

        // now both the mptissuance and mptoken are locked up
        BEAST_EXPECT(mptAlice.checkFlags(lsfMPTCanLock | lsfMPTLocked));
        BEAST_EXPECT(mptAlice.checkFlags(lsfMPTLocked, &bob));

        // alice tries to lock up both mptissuance and mptoken again
        // it will not change the flags and both will remain locked.
        mptAlice.set({.account = &alice, .flags = tfMPTLock});
        mptAlice.set({.account = &alice, .holder = &bob, .flags = tfMPTLock});

        // now both the mptissuance and mptoken remain locked up
        BEAST_EXPECT(mptAlice.checkFlags(lsfMPTCanLock | lsfMPTLocked));
        BEAST_EXPECT(mptAlice.checkFlags(lsfMPTLocked, &bob));

        // alice unlocks bob's mptoken
        mptAlice.set({.account = &alice, .holder = &bob, .flags = tfMPTUnlock});

        // only mptissuance is locked
        BEAST_EXPECT(mptAlice.checkFlags(lsfMPTCanLock | lsfMPTLocked));
        BEAST_EXPECT(mptAlice.checkFlags(0, &bob));

        // locks up bob's mptoken again
        mptAlice.set({.account = &alice, .holder = &bob, .flags = tfMPTLock});

        // now both the mptissuance and mptokens are locked up
        BEAST_EXPECT(mptAlice.checkFlags(lsfMPTCanLock | lsfMPTLocked));
        BEAST_EXPECT(mptAlice.checkFlags(lsfMPTLocked, &bob));

        // alice unlocks mptissuance
        mptAlice.set({.account = &alice, .flags = tfMPTUnlock});

        // now mptissuance is unlocked
        BEAST_EXPECT(mptAlice.checkFlags(lsfMPTCanLock));
        BEAST_EXPECT(mptAlice.checkFlags(lsfMPTLocked, &bob));

        // alice unlocks bob's mptoken
        mptAlice.set({.account = &alice, .holder = &bob, .flags = tfMPTUnlock});

        // both mptissuance and bob's mptoken are unlocked
        BEAST_EXPECT(mptAlice.checkFlags(lsfMPTCanLock));
        BEAST_EXPECT(mptAlice.checkFlags(0, &bob));

        // alice unlocks mptissuance and bob's mptoken again despite that
        // they are already unlocked. Make sure this will not change the
        // flags
        mptAlice.set({.account = &alice, .holder = &bob, .flags = tfMPTUnlock});
        mptAlice.set({.account = &alice, .flags = tfMPTUnlock});

        // both mptissuance and bob's mptoken remain unlocked
        BEAST_EXPECT(mptAlice.checkFlags(lsfMPTCanLock));
        BEAST_EXPECT(mptAlice.checkFlags(0, &bob));
    }

    void
    testPayment(FeatureBitset features)
    {
        testcase("Payment");

        using namespace test::jtx;
        Account const alice("alice");  // issuer
        Account const bob("bob");      // holder
        Account const carol("carol");  // holder
        {
            Env env{*this, features};

            MPTTester mptAlice(env, alice, {.holders = {&bob, &carol}});

            mptAlice.create({.ownerCount = 1, .holderCount = 0});

            // env(mpt::authorize(alice, id.key, std::nullopt));
            // env.close();

            mptAlice.authorize({.account = &bob});
            mptAlice.authorize({.account = &carol});

            // issuer to holder
            mptAlice.pay(alice, bob, 100);

            // holder to issuer
            mptAlice.pay(bob, alice, 100);

            // holder to holder
            mptAlice.pay(alice, bob, 100);
            mptAlice.pay(bob, carol, 50);
        }

        // If allowlisting is enabled, Payment fails if the receiver is not
        // authorized
        {
            Env env{*this, features};

            MPTTester mptAlice(env, alice, {.holders = {&bob}});

            mptAlice.create(
                {.ownerCount = 1, .holderCount = 0, .flags = tfMPTRequireAuth});

            mptAlice.authorize({.account = &bob});

            mptAlice.pay(alice, bob, 100, tecNO_AUTH);
        }

        // If allowlisting is enabled, Payment fails if the sender is not
        // authorized
        {
            Env env{*this, features};

            MPTTester mptAlice(env, alice, {.holders = {&bob}});

            mptAlice.create(
                {.ownerCount = 1, .holderCount = 0, .flags = tfMPTRequireAuth});

            // bob creates an empty MPToken
            mptAlice.authorize({.account = &bob});

            // alice authorizes bob to hold funds
            mptAlice.authorize({.account = &alice, .holder = &bob});

            // alice sends 100 MPT to bob
            mptAlice.pay(alice, bob, 100);

            // alice UNAUTHORIZES bob
            mptAlice.authorize(
                {.account = &alice, .holder = &bob, .flags = tfMPTUnauthorize});

            // bob fails to send back to alice because he is no longer
            // authorize to move his funds!
            mptAlice.pay(bob, alice, 100, tecNO_AUTH);
        }

        // Payer doesn't have enough funds
        {
            Env env{*this, features};

            MPTTester mptAlice(env, alice, {.holders = {&bob, &carol}});

            mptAlice.create({.ownerCount = 1});

            mptAlice.authorize({.account = &bob});
            mptAlice.authorize({.account = &carol});

            mptAlice.pay(alice, bob, 100);

            // Pay to another holder
            mptAlice.pay(bob, carol, 101, tecINSUFFICIENT_FUNDS);

            // Pay to the issuer
            mptAlice.pay(bob, alice, 101, tecINSUFFICIENT_FUNDS);
        }
    }

    void
    testMPTInvalidInTx(FeatureBitset features)
    {
        testcase("MPT Amount Invalid in Transaction");
        using namespace test::jtx;
        Env env{*this, features};
        Account const alice("alice");  // issuer

        MPTTester mptAlice(env, alice);

        mptAlice.create();

        env(offer(alice, mptAlice.mpt(100), XRP(100)), ter(temINVALID));
        env.close();

        BEAST_EXPECT(expectOffers(env, alice, 0));
    }

    void
    testTxJsonMetaFields(FeatureBitset features)
    {
        // checks synthetically parsed mptissuanceid from  `tx` response
        // it checks the parsing logic
        testcase("Test synthetic fields from tx response");

        using namespace test::jtx;

        Account const alice{"alice"};

        Env env{*this, features};
        MPTTester mptAlice(env, alice);

        mptAlice.create();

        std::string const txHash{
            env.tx()->getJson(JsonOptions::none)[jss::hash].asString()};

        Json::Value const meta = env.rpc("tx", txHash)[jss::result][jss::meta];

        // Expect mpt_issuance_id field
        BEAST_EXPECT(meta.isMember(jss::mpt_issuance_id));
        BEAST_EXPECT(
            meta[jss::mpt_issuance_id] == to_string(mptAlice.issuanceID()));
    }

    void
    testMPTHoldersAPI(FeatureBitset features)
    {
        testcase("MPT Holders");
        using namespace test::jtx;

        // a lambda that checks API correctness given different numbers of
        // MPToken
        auto checkMPTokens = [&](int expectCount,
                                 int expectMarkerCount,
                                 int line) {
            Env env{*this, features};
            Account const alice("alice");  // issuer

            MPTTester mptAlice(env, alice);

            mptAlice.create();

            // create accounts that will create MPTokens
            for (auto i = 0; i < expectCount; i++)
            {
                Account const bob{std::string("bob") + std::to_string(i)};
                env.fund(XRP(1000), bob);
                env.close();

                // a holder creates a mptoken
                mptAlice.authorize({.account = &bob});
            }

            // Checks mpt_holder query responses
            {
                int markerCount = 0;
                Json::Value allHolders(Json::arrayValue);
                std::string marker;

                // The do/while collects results until no marker is
                // returned.
                do
                {
                    Json::Value mptHolders = [&env, &mptAlice, &marker]() {
                        Json::Value params;
                        params[jss::mpt_issuance_id] =
                            to_string(mptAlice.issuanceID());

                        if (!marker.empty())
                            params[jss::marker] = marker;
                        return env.rpc(
                            "json", "mpt_holders", to_string(params));
                    }();

                    // If there are mptokens we get an error
                    if (expectCount == 0)
                    {
                        if (expect(
                                mptHolders.isMember(jss::result),
                                "expected \"result\"",
                                __FILE__,
                                line))
                        {
                            if (expect(
                                    mptHolders[jss::result].isMember(
                                        jss::error),
                                    "expected \"error\"",
                                    __FILE__,
                                    line))
                            {
                                expect(
                                    mptHolders[jss::result][jss::error]
                                            .asString() == "objectNotFound",
                                    "expected \"objectNotFound\"",
                                    __FILE__,
                                    line);
                            }
                        }
                        break;
                    }

                    marker.clear();
                    if (expect(
                            mptHolders.isMember(jss::result),
                            "expected \"result\"",
                            __FILE__,
                            line))
                    {
                        Json::Value& result = mptHolders[jss::result];

                        if (result.isMember(jss::marker))
                        {
                            ++markerCount;
                            marker = result[jss::marker].asString();
                        }

                        if (expect(
                                result.isMember(jss::holders),
                                "expected \"holders\"",
                                __FILE__,
                                line))
                        {
                            Json::Value& someHolders = result[jss::holders];
                            for (std::size_t i = 0; i < someHolders.size(); ++i)
                                allHolders.append(someHolders[i]);
                        }
                    }
                } while (!marker.empty());

                // Verify the contents of allHolders makes sense.
                expect(
                    allHolders.size() == expectCount,
                    "Unexpected returned offer count",
                    __FILE__,
                    line);
                expect(
                    markerCount == expectMarkerCount,
                    "Unexpected marker count",
                    __FILE__,
                    line);
                std::optional<int> globalFlags;
                std::set<std::string> mptIndexes;
                std::set<std::string> holderAddresses;
                for (Json::Value const& holder : allHolders)
                {
                    // The flags on all found offers should be the same.
                    if (!globalFlags)
                        globalFlags = holder[jss::flags].asInt();

                    expect(
                        *globalFlags == holder[jss::flags].asInt(),
                        "Inconsistent flags returned",
                        __FILE__,
                        line);

                    // The test conditions should produce unique indexes and
                    // amounts for all holders.
                    mptIndexes.insert(holder[jss::mptoken_index].asString());
                    holderAddresses.insert(holder[jss::account].asString());
                }

                expect(
                    mptIndexes.size() == expectCount,
                    "Duplicate indexes returned?",
                    __FILE__,
                    line);
                expect(
                    holderAddresses.size() == expectCount,
                    "Duplicate addresses returned?",
                    __FILE__,
                    line);
            }
        };

        // Test 1 MPToken
        checkMPTokens(1, 0, __LINE__);

        // Test 10 MPTokens
        checkMPTokens(10, 0, __LINE__);

        // Test 200 MPTokens
        checkMPTokens(200, 0, __LINE__);

        // Test 201 MPTokens
        checkMPTokens(201, 1, __LINE__);

        // Test 400 MPTokens
        checkMPTokens(400, 1, __LINE__);

        // Test 401 MPTokesn
        checkMPTokens(401, 2, __LINE__);
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

        // Test Direct Payment
        testPayment(all);

        // Test MPT Amount is invalid in non-Payment Tx
        testMPTInvalidInTx(all);

        // Test parsed MPTokenIssuanceID in API response metadata
        // TODO: This test exercises the parsing logic of mptID in `tx`,
        // but,
        //       mptID is also parsed in different places like `account_tx`,
        //       `subscribe`, `ledger`. We should create test for these
        //       occurances (lower prioirity).
        testTxJsonMetaFields(all);

        // Test mpt_holders
        testMPTHoldersAPI(all);
    }
};

BEAST_DEFINE_TESTSUITE_PRIO(MPToken, tx, ripple, 2);

}  // namespace ripple
