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

#include <test/jtx.h>
#include <test/jtx/confidentialTransfer.h>
#include <test/jtx/trust.h>

#include <xrpl/protocol/ConfidentialTransfer.h>

#include <openssl/rand.h>

namespace ripple {

class ConfidentialTransfer_test : public beast::unit_test::suite
{
    void
    testConvert(FeatureBitset features)
    {
        testcase("test convert");
        using namespace test::jtx;
        Env env{*this, features};
        Account const alice("alice");
        Account const bob("bob");
        MPTTester mptAlice(env, alice, {.holders = {bob}});

        mptAlice.create(
            {.ownerCount = 1,
             .holderCount = 0,
             .flags = tfMPTCanTransfer | tfMPTCanLock});

        mptAlice.authorize({.account = bob});
        env.close();
        mptAlice.pay(alice, bob, 100);
        env.close();

        // std::uint8_t const rawData[] = {
        //     0xB9, 0xE8, 0xC7, 0xD6, 0xA5, 0xF4, 0xB3, 0xA2, 0xC1, 0xD0, 0xE9,
        //     0xF8, 0xB7, 0xA6, 0xC5, 0xD4, 0xE3, 0xF2, 0xA1, 0xB0, 0xC9, 0xD8,
        //     0xE7, 0xF6, 0xA5, 0xB4, 0xC3, 0xD2, 0xE1, 0xF0, 0xB9, 0xA8, 0xC7,
        //     0xD6, 0xE5, 0xF4, 0xB3, 0xA2, 0xC1, 0xD0, 0xB9, 0xE8, 0xA7, 0xF6,
        //     0xC5, 0xD4, 0xB3, 0xA2, 0xE1, 0xF0, 0xB9, 0xC8, 0xD7, 0xA6, 0xE5,
        //     0xF4, 0xB3, 0xA2, 0xC1, 0xD0, 0xB9, 0xE8, 0xA7, 0xF6, 0xB5,
        //     0xA4};

        // Slice encAmt{rawData, ecGamalEncryptedTotalLength};
        // std::string data =
        //     "B9E8C7D6A5F4B3A2C1D0E9F8B7A6C5D4E3F2A1B0C9D8E7F6A5B4C3D2E1F0B9A8C7"
        //     "D6E5F4B3A2C1D0B9E8A7F6C5D4B3A2E1F0B9C8D7A6E5F4B3A2C1D0B9E8A7F6B5A"
        //     "4";
        unsigned char issuerPrivkey[32];
        secp256k1_pubkey issuerPubkey;
        BEAST_EXPECT(secp256k1_elgamal_generate_keypair(
            secp256k1Context(), issuerPrivkey, &issuerPubkey));

        // mptAlice.set({.account = alice, .flags = tfMPTLock});

        mptAlice.set(
            {.account = alice, .pubKey = strHex(Slice{issuerPubkey.data, 64})});

        unsigned char holderPrivkey[32];
        secp256k1_pubkey holderPubkey;
        BEAST_EXPECT(secp256k1_elgamal_generate_keypair(
            secp256k1Context(), holderPrivkey, &holderPubkey));

        auto encryptToBuffer = [](secp256k1_context const* ctx,
                                  secp256k1_pubkey const& recipientPubKey,
                                  uint64_t amount,
                                  Buffer& outBuffer) -> bool {
            // Allocate ciphertext placeholders
            secp256k1_pubkey c1, c2;

            // Prepare a random blinding factor
            unsigned char blinding_factor[32];
            if (RAND_bytes(blinding_factor, 32) != 1)
                return false;

            // Encrypt the amount
            if (!secp256k1_elgamal_encrypt(
                    ctx, &c1, &c2, &recipientPubKey, amount, blinding_factor))
                return false;

            // Serialize the ciphertext pair into the buffer
            if (!serializeEcPair(c1, c2, outBuffer))
                return false;

            return true;
        };

        Buffer holderEncryptedAmt(ecGamalEncryptedTotalLength);
        Buffer issuerEncryptedAmt(ecGamalEncryptedTotalLength);

        // Encrypt holder amount
        BEAST_EXPECT(encryptToBuffer(
            secp256k1Context(), holderPubkey, 10, holderEncryptedAmt));

        // Encrypt issuer amount
        BEAST_EXPECT(encryptToBuffer(
            secp256k1Context(), issuerPubkey, 10, issuerEncryptedAmt));

        mptAlice.convert(
            {.account = bob,
             .amt = 10,
             .proof = "123",
             .holderPubKey = strHex(Slice{holderPubkey.data, 64}),
             .holderEncryptedAmt = strHex(holderEncryptedAmt),
             .issuerEncryptedAmt = strHex(issuerEncryptedAmt)});
        env.close();

        mptAlice.printMPT(bob);
        {
            Slice bobEnc = mptAlice.getEncryptedBalance(bob);

            secp256k1_pubkey c1;
            secp256k1_pubkey c2;

            uint64_t decryptedAmt;
            BEAST_EXPECT(makeEcPair(bobEnc, c1, c2));
            BEAST_EXPECT(secp256k1_elgamal_decrypt(
                secp256k1Context(), &decryptedAmt, &c1, &c2, holderPrivkey));
            std::cout << "\n decrpypted amt is " << decryptedAmt << '\n';
        }

        Buffer holderEncryptedAmt2(ecGamalEncryptedTotalLength);
        Buffer issuerEncryptedAmt2(ecGamalEncryptedTotalLength);

        // Encrypt holder amount
        BEAST_EXPECT(encryptToBuffer(
            secp256k1Context(), holderPubkey, 20, holderEncryptedAmt2));

        // Encrypt issuer amount
        BEAST_EXPECT(encryptToBuffer(
            secp256k1Context(), issuerPubkey, 20, issuerEncryptedAmt2));

        mptAlice.convert(
            {.account = bob,
             .amt = 10,
             .proof = "123",
             .holderEncryptedAmt = strHex(holderEncryptedAmt2),
             .issuerEncryptedAmt = strHex(issuerEncryptedAmt2)});
        env.close();

        {
            Slice bobEnc = mptAlice.getEncryptedBalance(bob);

            secp256k1_pubkey c1;
            secp256k1_pubkey c2;

            uint64_t decryptedAmt;
            BEAST_EXPECT(makeEcPair(bobEnc, c1, c2));
            BEAST_EXPECT(secp256k1_elgamal_decrypt(
                secp256k1Context(), &decryptedAmt, &c1, &c2, holderPrivkey));
            std::cout << "\n decrpypted amt is " << decryptedAmt << '\n';
        }
    }

    void
    testWithFeats(FeatureBitset features)
    {
        testConvert(features);
    }

public:
    void
    run() override
    {
        using namespace test::jtx;
        FeatureBitset const all{testable_amendments()};

        testWithFeats(all);
    }
};

BEAST_DEFINE_TESTSUITE(ConfidentialTransfer, app, ripple);
}  // namespace ripple
