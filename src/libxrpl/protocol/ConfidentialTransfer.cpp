//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2025 Ripple Labs Inc.

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

#include <xrpl/protocol/ConfidentialTransfer.h>
#include <xrpl/protocol/Protocol.h>
#include <xrpl/protocol/TER.h>

namespace ripple {

// breaks a 66-byte encrypted amount into two 33-byte components
// then parses each 33-byte component into 64-byte secp256k1_pubkey format
bool
makeEcPair(Slice const& buffer, secp256k1_pubkey& out1, secp256k1_pubkey& out2)
{
    auto parsePubKey = [](Slice const& slice, secp256k1_pubkey& out) {
        return secp256k1_ec_pubkey_parse(
            secp256k1Context(),
            &out,
            reinterpret_cast<unsigned char const*>(slice.data()),
            slice.length());
    };

    Slice s1{buffer.data(), ecGamalEncryptedLength};
    Slice s2{buffer.data() + ecGamalEncryptedLength, ecGamalEncryptedLength};

    int const ret1 = parsePubKey(s1, out1);
    int const ret2 = parsePubKey(s2, out2);

    return ret1 == 1 && ret2 == 1;
}

// serialize two secp256k1_pubkey components back into compressed 66-byte form
bool
serializeEcPair(
    secp256k1_pubkey const& in1,
    secp256k1_pubkey const& in2,
    Buffer& buffer)
{
    auto serializePubKey = [](secp256k1_pubkey const& pub, unsigned char* out) {
        size_t outLen = ecGamalEncryptedLength;  // 33 bytes
        int const ret = secp256k1_ec_pubkey_serialize(
            secp256k1Context(), out, &outLen, &pub, SECP256K1_EC_COMPRESSED);
        return ret == 1 && outLen == ecGamalEncryptedLength;
    };

    unsigned char* ptr = buffer.data();
    bool const res1 = serializePubKey(in1, ptr);
    bool const res2 = serializePubKey(in2, ptr + ecGamalEncryptedLength);

    return res1 && res2;
}

TER
homomorphicAdd(Slice const& a, Slice const& b, Buffer& out)
{
    if (a.length() != ecGamalEncryptedTotalLength ||
        b.length() != ecGamalEncryptedTotalLength)
        return tecINTERNAL;

    secp256k1_pubkey a_c1;
    secp256k1_pubkey a_c2;
    secp256k1_pubkey b_c1;
    secp256k1_pubkey b_c2;

    if (!makeEcPair(a, a_c1, a_c2) || !!makeEcPair(b, b_c1, b_c2))
        return tecINTERNAL;

    secp256k1_pubkey sum_c1;
    secp256k1_pubkey sum_c2;

    // todo:: support addition after it's supported
    // if (secp256k1_elgamal_add(
    //         secp256k1Context(), &sum_c1, &sum_c2, a_c1, a_c2, b_c1, b_c2_) !=
    //         1)
    //     return tecINTERNAL;

    if (!serializeEcPair(sum_c1, sum_c2, out))
        return tecINTERNAL;

    return tesSUCCESS;
}

}  // namespace ripple
