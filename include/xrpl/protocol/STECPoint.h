

#ifndef RIPPLE_PROTOCOL_STECPOINTS_H_INCLUDED
#define RIPPLE_PROTOCOL_STECPOINTS_H_INCLUDED

#include <xrpl/basics/Buffer.h>
#include <xrpl/basics/CountedObject.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/STBase.h>
#include <xrpl/protocol/SerialIter.h>

#include <stdexcept>
#include <utility>

namespace ripple {

/** Represents two 33-byte compressed elliptic curve points.
    This is often used for cryptographic schemes like EC ElGamal, where
    a ciphertext consists of two distinct components (points on the curve).
    This class stores them as two separate buffers internally but serializes
    them as a single, contiguous 66-byte block.
*/
class STECPoints : public STBase, public CountedObject<STECPoints>
{
public:
    // The total size of the serialized object in bytes.
    constexpr static std::size_t size_ = 66;
    // The size of a single EC point in bytes.
    constexpr static std::size_t point_size_ = 33;

private:
    Buffer point1_;
    Buffer point2_;

public:
    STECPoints() = default;
    STECPoints(SField const& f);

    // Constructor for in-memory creation from two points.
    STECPoints(SField const& f, Buffer&& p1, Buffer&& p2);

    // Constructor from a single 66-byte slice (e.g., from a test).
    STECPoints(SField const& f, Slice const& slice);

    // Constructor from raw bits on the wire (deserialization).
    STECPoints(SerialIter& sit, SField const& name = sfGeneric);

    SerializedTypeID
    getSType() const override;

    std::string
    getText() const override;

    // Transforms the object into a single 66-byte block for serialization.
    void
    add(Serializer& s) const override;

    bool
    isEquivalent(STBase const& t) const override;

    bool
    isDefault() const noexcept override;

    // Accessors for the two points
    Slice
    getPointA() const noexcept;
    Slice
    getPointB() const noexcept;

private:
    STBase*
    copy(std::size_t n, void* buf) const override;
    STBase*
    move(std::size_t n, void* buf) override;
};

}  // namespace ripple

#endif
