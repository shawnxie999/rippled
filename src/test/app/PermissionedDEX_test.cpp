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
#include <xrpld/app/tx/detail/PermissionedDomainSet.h>
#include <xrpld/ledger/ApplyViewImpl.h>
#include <xrpl/basics/Blob.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/jss.h>

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
        auto const gw = Account{"gateway"};
        auto const alice = Account{"alice"};
        auto const bob = Account{"bob"};
        auto const carol = Account{"carol"};
        auto const USD = gw["USD"];

        Env env(*this, features);
        env.fund(XRP(1000), alice, bob, carol, gw);
        env.close();
        env.trust(USD(1000), alice);
        env.close();
        env.trust(USD(1000), bob);
        env.close();
        env.trust(USD(1000), carol);
        env.close();

        env(pay(gw, alice, USD(100)));
        env.close();
        env(pay(gw, bob, USD(100)));
        env.close();
        env(pay(gw, carol, USD(100)));
        env.close();

        BEAST_EXPECT(env.ownerCount(alice) == 1);
        BEAST_EXPECT(env.ownerCount(bob) == 1);

        const char credType[] = "abcde";
        pdomain::Credentials credentials{{alice, credType}};
        env(pdomain::setTx(alice, credentials));
        BEAST_EXPECT(env.ownerCount(alice) == 2);
        auto objects = pdomain::getObjects(alice, env);
        BEAST_EXPECT(objects.size() == 1);
        // Test that account_objects is correct without passing it the type
        BEAST_EXPECT(objects == pdomain::getObjects(alice, env, false));
        auto const domainID = objects.begin()->first;

        // alice also issues a credential for bob
        env(credentials::create(bob, alice, credType));
        env.close();

        BEAST_EXPECT(env.ownerCount(alice) == 3);

        env(offer(bob, XRP(10), USD(10)),
            require(offers(bob, 1)),
            domain(domainID));

        BEAST_EXPECT(env.ownerCount(bob) == 2);
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
