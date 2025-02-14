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
#include "xrpl/beast/unit_test/suite.h"
#include "xrpl/protocol/Indexes.h"
#include "xrpl/protocol/TER.h"
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
    testOfferCreateDomainValidations(FeatureBitset features)
    {
        testcase("OfferCreate domain validations");

        // test preflight
        {
            Env env(*this, features - featurePermissionedDEX);
            PermissionedDEX permDex(env);
            auto const gw = permDex.gw;
            auto const domainOwner = permDex.domainOwner;
            auto const alice = permDex.alice;
            auto const bob = permDex.bob;
            auto const USD = permDex.USD;
            auto const domainID = permDex.domainID;
            auto const credType = permDex.credType;

            env(offer(bob, XRP(10), USD(10)),
                domain(domainID),
                ter(temDISABLED));
            env.close();

            // // enable only featureFlowCross but not featurePermissionedDEX
            // env.enableFeature(featureFlowCross);
            // env.close();
            // env(offer(bob, XRP(10), USD(10)),
            //     domain(domainID),
            //     ter(temDISABLED));
            // env.close();

            // // enabled only featurePermissionedDEX but not featureFlowCross
            // env.disableFeature(featureFlowCross);
            // env.close();
            // env.enableFeature(featurePermissionedDEX);
            // env.close();
            // env(offer(bob, XRP(10), USD(10)),
            //     domain(domainID),
            //     ter(temDISABLED));
            // env.close();

            env.enableFeature(featurePermissionedDEX);
            env.close();
            env(offer(bob, XRP(10), USD(10)), domain(domainID));
            env.close();
        }

        // // preclaim
        // {
        //     // create devin account who is not part of the domain
        //     Account devin("devin");
        //     env.fund(XRP(1000), devin);
        //     env.close();
        //     env.trust(USD(1000), devin);
        //     env.close();
        //     env(pay(gw, devin, USD(100)));
        //     env.close();

        //     env(offer(devin, XRP(10), USD(10)),
        //         domain(domainID),
        //         ter(tecNO_PERMISSION));
        //     env.close();

        //     // domain owner also issues a credential for devin
        //     env(credentials::create(devin, domainOwner, credType));
        //     env.close();
        // }
    }

    void
    testSimpleBookStep(FeatureBitset features)
    {
        testcase("Simple book step");

        Env env(*this, features);
        PermissionedDEX permDex(env);
        auto const gw = permDex.gw;
        auto const domainOwner = permDex.domainOwner;
        auto const alice = permDex.alice;
        auto const bob = permDex.bob;
        auto const carol = permDex.carol;
        auto const USD = permDex.USD;
        auto const domainID = permDex.domainID;

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

        // domain direct is empty
        BEAST_EXPECT(checkDirectorySize(env, domainDirKey->key, 0));
    }

public:
    void
    run() override
    {
        FeatureBitset const all{
            jtx::supported_amendments() | featurePermissionedDomains |
            featureCredentials | featurePermissionedDEX};

        testOfferCreateDomainValidations(all);
        testSimpleBookStep(all);
    }
};

BEAST_DEFINE_TESTSUITE(PermissionedDEX, app, ripple);

}  // namespace test
}  // namespace ripple
