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

#include <test/jtx.h>
#include <test/jtx/permissioned_dex.h>
#include <xrpld/app/tx/detail/PermissionedDomainSet.h>
#include <xrpld/ledger/ApplyViewImpl.h>
#include <xrpl/basics/Blob.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/Keylet.h>
#include <xrpl/protocol/jss.h>

#include "test/jtx/Account.h"
#include "test/jtx/txflags.h"
#include "xrpl/beast/unit_test/suite.h"
#include "xrpl/protocol/Indexes.h"
#include "xrpl/protocol/TER.h"
#include "xrpl/protocol/TxFlags.h"
#include <cstdint>
#include <exception>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <test/jtx/AMM.h>
#include <test/jtx/AMMTest.h>

namespace ripple {
namespace test {

using namespace jtx;

class PermissionedDEX_test : public beast::unit_test::suite
{
    [[nodiscard]] bool
    offerExists(Env const& env, Account const& account, std::uint32_t offerSeq)
    {
        if (auto const sle = env.le(keylet::offer(account.id(), offerSeq)))
            return true;
        return false;
    }

    [[nodiscard]] bool
    checkOfferBalance(
        Env const& env,
        Account const& account,
        std::uint32_t offerSeq,
        STAmount const& takerPays,
        STAmount const& takerGets)
    {
        if (auto const sle = env.le(keylet::offer(account.id(), offerSeq)))
            return sle->getFieldAmount(sfTakerGets) == takerGets &&
                sle->getFieldAmount(sfTakerPays) == takerPays;

        return false;
    }

    // Keylet
    // getOfferDirKey(
    //     Book const& book,
    //     STAmount const& takerPays,
    //     STAmount const& takerGets)
    // {
    //     return keylet::quality(keylet::book(book), getRate(takerGets,
    //     takerPays));
    // }

    std::optional<Keylet>
    getOfferDirKey(
        Env const& env,
        Account const& account,
        std::uint32_t offerSeq)
    {
        if (auto const sle = env.le(keylet::offer(account.id(), offerSeq)))
            return Keylet(ltDIR_NODE, (*sle)[sfBookDirectory]);

        return {};
    }

    [[nodiscard]] bool
    checkDirectorySize(Env const& env, uint256 directory, std::uint32_t dirSize)
    {
        std::optional<std::uint64_t> pageIndex{0};
        std::uint32_t dirCnt = 0;

        do
        {
            auto const page = env.le(keylet::page(directory, *pageIndex));
            if (!page)
                break;

            pageIndex = (*page)[~sfIndexNext];
            dirCnt += (*page)[sfIndexes].size();

        } while (pageIndex.value_or(0));

        return dirCnt == dirSize;
    }

    void
    testOfferCreate(FeatureBitset features)
    {
        testcase("OfferCreate");

        // test preflight
        {
            Env env(*this, features - featurePermissionedDEX);
            PermissionedDEX permDex(env);
            auto const& [gw, domainOwner, alice, bob, carol, USD, domainID, credType] =
                permDex;

            env(offer(bob, XRP(10), USD(10)),
                domain(domainID),
                ter(temDISABLED));
            env.close();

            env.enableFeature(featurePermissionedDEX);
            env.close();
            env(offer(bob, XRP(10), USD(10)), domain(domainID));
            env.close();
        }

        // test preflight: permissioned dex cannot be used without enable
        // flowcross
        {
            Env env(*this, features - featureFlowCross);
            PermissionedDEX permDex(env);
            auto const& [gw, domainOwner, alice, bob, carol, USD, domainID, credType] =
                permDex;

            env(offer(bob, XRP(10), USD(10)),
                domain(domainID),
                ter(temDISABLED));
            env.close();

            env.enableFeature(featureFlowCross);
            env.close();
            env(offer(bob, XRP(10), USD(10)), domain(domainID));
            env.close();
        }

        // preclaim
        {
            Env env(*this, features);
            PermissionedDEX permDex(env);
            auto const& [gw, domainOwner, alice, bob, carol, USD, domainID, credType] =
                permDex;

            // create devin account who is not part of the domain
            Account devin("devin");
            env.fund(XRP(1000), devin);
            env.close();
            env.trust(USD(1000), devin);
            env.close();
            env(pay(gw, devin, USD(100)));
            env.close();

            env(offer(devin, XRP(10), USD(10)),
                domain(domainID),
                ter(tecNO_PERMISSION));
            env.close();

            // domain owner also issues a credential for devin
            env(credentials::create(devin, domainOwner, credType));
            env.close();

            // devin still cannot create offer since he didn't accept credential
            env(offer(devin, XRP(10), USD(10)),
                domain(domainID),
                ter(tecNO_PERMISSION));
            env.close();

            env(credentials::accept(devin, domainOwner, credType));
            env.close();

            env(offer(devin, XRP(10), USD(10)), domain(domainID));
            env.close();
        }

        // preclaim: test expired cred
        {
            Env env(*this, features);
            PermissionedDEX permDex(env);
            auto const& [gw, domainOwner, alice, bob, carol, USD, domainID, credType] =
                permDex;

            // create devin account who is not part of the domain
            Account devin("devin");
            env.fund(XRP(1000), devin);
            env.close();
            env.trust(USD(1000), devin);
            env.close();
            env(pay(gw, devin, USD(100)));
            env.close();

            auto jv = credentials::create(devin, domainOwner, credType);
            uint32_t const t = env.current()
                                   ->info()
                                   .parentCloseTime.time_since_epoch()
                                   .count();
            jv[sfExpiration.jsonName] = t + 20;
            env(jv);

            env(credentials::accept(devin, domainOwner, credType));
            env.close();

            // devin can still create offer while his cred is not expired
            env(offer(devin, XRP(10), USD(10)), domain(domainID));
            env.close();

            // time advance
            env.close();
            env.close();
            env.close();

            // devin cannot create offer with expired cred
            env(offer(devin, XRP(10), USD(10)),
                domain(domainID),
                ter(tecNO_PERMISSION));
            env.close();
        }

        // preclaim - takergets issuer is not in domain
        {
            Env env(*this, features);
            PermissionedDEX permDex(env);
            auto const& [gw, domainOwner, alice, bob, carol, USD, domainID, credType] =
                permDex;

            env(credentials::deleteCred(
                domainOwner, gw, domainOwner, credType));
            env.close();

            auto const bobOfferSeq{env.seq(bob)};
            env(offer(bob, XRP(10), USD(10)),
                domain(domainID),
                ter(tecNO_PERMISSION));
            env.close();

            BEAST_EXPECT(!offerExists(env, bob, bobOfferSeq));
        }

        // preclaim - takerpays issuer is not in domain
        {
            Env env(*this, features);
            PermissionedDEX permDex(env);
            auto const& [gw, domainOwner, alice, bob, carol, USD, domainID, credType] =
                permDex;

            env(credentials::deleteCred(
                domainOwner, gw, domainOwner, credType));
            env.close();

            auto const bobOfferSeq{env.seq(bob)};
            env(offer(bob, USD(10), XRP(10)),
                domain(domainID),
                ter(tecNO_PERMISSION));
            env.close();

            BEAST_EXPECT(!offerExists(env, bob, bobOfferSeq));
        }

        // apply - offer cross
        {
            Env env(*this, features);
            PermissionedDEX permDex(env);
            auto const& [gw, domainOwner, alice, bob, carol, USD, domainID, credType] =
                permDex;

            auto const bobOfferSeq{env.seq(bob)};
            env(offer(bob, XRP(10), USD(10)), domain(domainID));
            env.close();

            BEAST_EXPECT(offerExists(env, bob, bobOfferSeq));
            BEAST_EXPECT(ownerCount(env, bob) == 3);

            // a non domain offer cannot cross with domain offer
            env(offer(carol, USD(10), XRP(10)));
            env.close();

            BEAST_EXPECT(offerExists(env, bob, bobOfferSeq));

            auto const aliceOfferSeq{env.seq(bob)};
            env(offer(alice, USD(10), XRP(10)), domain(domainID));
            env.close();

            BEAST_EXPECT(!offerExists(env, alice, aliceOfferSeq));
            BEAST_EXPECT(!offerExists(env, bob, bobOfferSeq));
            BEAST_EXPECT(ownerCount(env, alice) == 2);
        }
    }

    void
    testPayment(FeatureBitset features)
    {
        testcase("Payment");

        // test preflight
        {
            Env env(*this, features - featurePermissionedDEX);
            PermissionedDEX permDex(env);
            auto const& [gw, domainOwner, alice, bob, carol, USD, domainID, credType] =
                permDex;

            env(pay(bob, alice, USD(10)),
                path(~USD),
                sendmax(XRP(10)),
                domain(domainID),
                ter(temDISABLED));
            env.close();

            env.enableFeature(featurePermissionedDEX);
            env.close();

            env(offer(bob, XRP(10), USD(10)), domain(domainID));
            env.close();

            env(pay(bob, alice, USD(10)),
                path(~USD),
                sendmax(XRP(10)),
                domain(domainID));
            env.close();
        }

        // preclaim: non-domain destination cannot accept
        {
            Env env(*this, features);
            PermissionedDEX permDex(env);
            auto const& [gw, domainOwner, alice, bob, carol, USD, domainID, credType] =
                permDex;

            env(offer(bob, XRP(10), USD(10)), domain(domainID));
            env.close();

            // create devin account who is not part of the domain
            Account devin("devin");
            env.fund(XRP(1000), devin);
            env.close();
            env.trust(USD(1000), devin);
            env.close();
            env(pay(gw, devin, USD(100)));
            env.close();

            // devin is not part of domain
            env(pay(alice, devin, USD(10)),
                path(~USD),
                sendmax(XRP(10)),
                domain(domainID),
                ter(tecNO_PERMISSION));
            env.close();

            // domain owner also issues a credential for devin
            env(credentials::create(devin, domainOwner, credType));
            env.close();

            // devin has not yet accepted cred
            env(pay(alice, devin, USD(10)),
                path(~USD),
                sendmax(XRP(10)),
                domain(domainID),
                ter(tecNO_PERMISSION));
            env.close();

            env(credentials::accept(devin, domainOwner, credType));
            env.close();

            env(pay(alice, devin, USD(10)),
                path(~USD),
                sendmax(XRP(10)),
                domain(domainID));
            env.close();
        }

        // preclaim: non-domain sender cannot send
        {
            Env env(*this, features);
            PermissionedDEX permDex(env);
            auto const& [gw, domainOwner, alice, bob, carol, USD, domainID, credType] =
                permDex;

            env(offer(bob, XRP(10), USD(10)), domain(domainID));
            env.close();

            // create devin account who is not part of the domain
            Account devin("devin");
            env.fund(XRP(1000), devin);
            env.close();
            env.trust(USD(1000), devin);
            env.close();
            env(pay(gw, devin, USD(100)));
            env.close();

            // devin is not part of domain
            env(pay(devin, alice, USD(10)),
                path(~USD),
                sendmax(XRP(10)),
                domain(domainID),
                ter(tecNO_PERMISSION));
            env.close();

            // domain owner also issues a credential for devin
            env(credentials::create(devin, domainOwner, credType));
            env.close();

            // devin has not yet accepted cred
            env(pay(devin, alice, USD(10)),
                path(~USD),
                sendmax(XRP(10)),
                domain(domainID),
                ter(tecNO_PERMISSION));
            env.close();

            env(credentials::accept(devin, domainOwner, credType));
            env.close();

            env(pay(devin, alice, USD(10)),
                path(~USD),
                sendmax(XRP(10)),
                domain(domainID));
            env.close();
        }

        // preclaim: non-domain sender cannot send
        {
            Env env(*this, features);
            PermissionedDEX permDex(env);
            auto const& [gw, domainOwner, alice, bob, carol, USD, domainID, credType] =
                permDex;

            env(offer(bob, XRP(10), USD(10)), domain(domainID));
            env.close();

            // domain owner can always be destination
            env(pay(alice, domainOwner, USD(10)),
                path(~USD),
                sendmax(XRP(10)),
                domain(domainID));
            env.close();

            env(offer(bob, XRP(10), USD(10)), domain(domainID));
            env.close();

            // domain owner can send
            env(pay(domainOwner, alice, USD(10)),
                path(~USD),
                sendmax(XRP(10)),
                domain(domainID));
            env.close();
        }
    }

    void
    testSimpleBookStep(FeatureBitset features)
    {
        testcase("Simple book step");

        // test that a domain offer can be consumed and non-domain offer is not
        // consumed during a domain payment
        {
            Env env(*this, features);
            PermissionedDEX permDex(env);
            auto const& [gw, domainOwner, alice, bob, carol, USD, domainID, credType] =
                permDex;

            auto const regularOfferSeq{env.seq(bob)};
            env(offer(bob, XRP(10), USD(10)));
            env.close();
            BEAST_EXPECT(
                checkOfferBalance(env, bob, regularOfferSeq, XRP(10), USD(10)));

            // if trying to make permissioned payment with a normal offer, it
            // fails
            env(pay(alice, carol, USD(10)),
                path(~USD),
                sendmax(XRP(10)),
                domain(domainID),
                ter(tecPATH_PARTIAL));
            env.close();

            auto const domainOfferSeq{env.seq(bob)};
            env(offer(bob, XRP(10), USD(10)), domain(domainID));
            env.close();

            BEAST_EXPECT(
                checkOfferBalance(env, bob, domainOfferSeq, XRP(10), USD(10)));

            auto const domainDirKey = getOfferDirKey(env, bob, domainOfferSeq);
            BEAST_EXPECT(checkDirectorySize(env, domainDirKey->key, 1));

            // cross-currency permissioned payment consumed
            // domain offer instead of regular offer
            env(pay(alice, carol, USD(10)),
                path(~USD),
                sendmax(XRP(10)),
                domain(domainID));
            env.close();
            BEAST_EXPECT(!offerExists(env, bob, domainOfferSeq));
            BEAST_EXPECT(
                checkOfferBalance(env, bob, regularOfferSeq, XRP(10), USD(10)));

            // domain directory is empty
            BEAST_EXPECT(checkDirectorySize(env, domainDirKey->key, 0));
        }

        // test a non-domain account can still be part of rippling in a domain
        // payment. If the domain wishes to control who is allowed to ripple
        // through, they should set the rippling individually
        {
            Env env(*this, features);
            PermissionedDEX permDex(env);
            auto const& [gw, domainOwner, alice, bob, carol, USD, domainID, credType] =
                permDex;

            auto const EURA = alice["EUR"];
            auto const EURB = bob["EUR"];

            env.trust(EURA(100), bob);
            env.trust(EURB(100), carol);
            env.close();

            // remove bob from domain
            env(credentials::deleteCred(
                domainOwner, bob, domainOwner, credType));
            env.close();

            // alice can still ripple through bob even though he's not part
            // of the domain
            env(pay(alice, carol, EURB(10)), paths(EURA), domain(domainID));
            env.close();
            env.require(balance(bob, EURA(10)), balance(carol, EURB(10)));
        }
    }

    void
    testOfferTokenIssuerInDomain(FeatureBitset features)
    {
        testcase("Offer token issuer in domain");

        Env env(*this, features);
        PermissionedDEX permDex(env);
        auto const& [gw, domainOwner, alice, bob, carol, USD, domainID, credType] =
            permDex;

        // create an offer with usd as takergets
        auto const bobOffer1Seq{env.seq(bob)};
        env(offer(bob, XRP(10), USD(10)), domain(domainID));
        env.close();

        // create an offer with usd as takerpays
        auto const bobOffer2Seq{env.seq(bob)};
        env(offer(bob, USD(10), XRP(10)), domain(domainID), txflags(tfPassive));
        env.close();

        BEAST_EXPECT(
            checkOfferBalance(env, bob, bobOffer1Seq, XRP(10), USD(10)));
        BEAST_EXPECT(
            checkOfferBalance(env, bob, bobOffer2Seq, USD(10), XRP(10)));

        // remove gateway from domain
        env(credentials::deleteCred(domainOwner, gw, domainOwner, credType));
        env.close();

        // payment fails since gateway is not in domain
        env(pay(alice, carol, USD(10)),
            path(~USD),
            sendmax(XRP(10)),
            domain(domainID),
            ter(tecPATH_PARTIAL));
        env.close();
        BEAST_EXPECT(offerExists(env, bob, bobOffer1Seq));

        env(pay(alice, carol, XRP(10)),
            path(~XRP),
            sendmax(USD(10)),
            domain(domainID),
            ter(tecPATH_PARTIAL));
        env.close();
        BEAST_EXPECT(offerExists(env, bob, bobOffer2Seq));

        // add gateway back to domain
        env(credentials::create(gw, domainOwner, credType));
        env.close();
        env(credentials::accept(gw, domainOwner, credType));
        env.close();

        // offers can now be consumed again
        env(pay(alice, carol, USD(10)),
            path(~USD),
            sendmax(XRP(10)),
            domain(domainID));
        env.close();
        BEAST_EXPECT(!offerExists(env, bob, bobOffer1Seq));

        env(pay(alice, carol, XRP(10)),
            path(~XRP),
            sendmax(USD(10)),
            domain(domainID));
        env.close();
        BEAST_EXPECT(!offerExists(env, bob, bobOffer2Seq));
    }

    void
    testRemoveUnfundedOffer(FeatureBitset features)
    {
        testcase("Remove unfunded offer");

        Env env(*this, features);
        PermissionedDEX permDex(env);
        auto const& [gw, domainOwner, alice, bob, carol, USD, domainID, credType] =
            permDex;

        auto const bobOfferSeq{env.seq(bob)};
        env(offer(bob, XRP(10), USD(10)), domain(domainID));
        env.close();

        auto const aliceOfferSeq{env.seq(alice)};
        env(offer(alice, XRP(50), USD(50)), domain(domainID));
        env.close();

        BEAST_EXPECT(offerExists(env, bob, bobOfferSeq));
        BEAST_EXPECT(offerExists(env, alice, aliceOfferSeq));

        auto const domainDirKey = getOfferDirKey(env, bob, bobOfferSeq);
        BEAST_EXPECT(checkDirectorySize(env, domainDirKey->key, 2));

        // remove alice from domain
        env(credentials::deleteCred(domainOwner, alice, domainOwner, credType));
        env.close();

        env(pay(gw, carol, USD(10)),
            path(~USD),
            sendmax(XRP(10)),
            domain(domainID));
        env.close();

        BEAST_EXPECT(!offerExists(env, bob, bobOfferSeq));

        // alice's unfunded offer is removed implicitly
        BEAST_EXPECT(!offerExists(env, alice, aliceOfferSeq));
        BEAST_EXPECT(checkDirectorySize(env, domainDirKey->key, 0));
    }

public:
    void
    run() override
    {
        FeatureBitset const all{
            jtx::supported_amendments() | featurePermissionedDomains |
            featureCredentials | featurePermissionedDEX};

        testOfferCreate(all);
        testPayment(all);
        testSimpleBookStep(all);
        testOfferTokenIssuerInDomain(all);
        testRemoveUnfundedOffer(all);

        // domain does not affect non offers eg rippling
        // test rippling with multi issuers
        // test more complex path
        // test directory
    }
};

BEAST_DEFINE_TESTSUITE(PermissionedDEX, app, ripple);

}  // namespace test
}  // namespace ripple
