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
#include <xrpl/protocol/jss.h>

#include "xrpl/beast/unit_test/suite.h"
#include "xrpl/protocol/TER.h"
#include <exception>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ripple {
namespace test {

using namespace jtx;


class PermissionedDEX_test : public beast::unit_test::suite
{
    void
    testOffer(FeatureBitset features)
    {
        testcase("Offer");

        Env env(*this, features);
        PermissionedDEX permDex(env);
        auto const gw = permDex.gw;
        auto const domainOwner = permDex.domainOwner;
        auto const alice = permDex.alice;
        auto const bob = permDex.bob;
        auto const carol = permDex.carol;
        auto const USD = permDex.USD;
        auto const domainID = permDex.domainID;

        auto const regularOfferID{env.seq(bob)};
        env(offer(bob, XRP(10), USD(10)),
            require(offers(bob, 1)));
        env.close();

        env(pay(alice, carol, USD(10)), path(~USD), sendmax(XRP(10)));
        env.close();
        BEAST_EXPECT(expectOffers(env, bob, 0));

        // if trying to make permissioned payment with a valid offer, it fails
        env(pay(alice, carol, USD(10)),
            path(~USD),
            sendmax(XRP(10)),
            domain(domainID), ter(tecPATH_PARTIAL));
        env.close();

        auto const domainOfferID{env.seq(bob)};
        env(offer(bob, XRP(10), USD(10)),
            require(offers(bob, 1)),
            domain(domainID));
        env.close();

        env(pay(alice, carol, USD(10)),path(~USD), sendmax(XRP(10)), domain(domainID));
    }


public:
    
    void run() override{
        FeatureBitset const all{
            jtx::supported_amendments() | featurePermissionedDomains |
            featureCredentials | featurePermissionedDEX};
        testOffer(all);
    }
};

    BEAST_DEFINE_TESTSUITE(PermissionedDEX, app, ripple);

}  // namespace test
}  // namespace ripple
