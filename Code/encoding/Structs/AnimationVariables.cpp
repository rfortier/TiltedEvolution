#include <Structs/AnimationVariables.h>
#include <TiltedCore/Serialization.hpp>
#include <iostream>

bool AnimationVariables::operator==(const AnimationVariables& acRhs) const noexcept
{
    return Booleans == acRhs.Booleans && Integers == acRhs.Integers && Floats == acRhs.Floats;
}

bool AnimationVariables::operator!=(const AnimationVariables& acRhs) const noexcept
{
    return !this->operator==(acRhs);
}

// std::vector<bool> implementation is unspecified, but often packed reasonably.
// The spec does not guarantee contiguous memory, though, so somewhat laborious 
// translation needed. Should be better than winding down several layers to 
// TiltedPhoques::Serialization::WriteBool, though.
//
void AnimationVariables::VectorBool_to_String(const Vector<bool>& bools, TiltedPhoques::String& chars) const
{
    chars.assign((bools.size() + 7) >> 3, 0);

    auto citer = chars.begin();
    auto biter = bools.begin();
    for (uint32_t mask = 1; biter < bools.end(); mask = 1, citer++)
        for (; mask < 0x100 && biter < bools.end(); mask <<= 1)
            *citer |= *biter++ ? mask : 0;
}

// The Vector<bool> must be the correct size when called.
//
void AnimationVariables::String_to_VectorBool(const TiltedPhoques::String& chars, Vector<bool>& bools)
{
    bools.assign(bools.size(), false);

    auto citer = chars.begin();
    auto biter = bools.begin();
    for (uint32_t mask = 1; biter < bools.end(); mask = 1, citer++)
        for (; mask < 0x100 && biter < bools.end(); mask <<= 1)
            *biter++ = (*citer & mask) ? true : false;
}


void AnimationVariables::Load(std::istream& aInput)
{
    // Booleans are bitpacked and a bit different, not guaranteed contiguous.
   TiltedPhoques::String chars((Booleans.size() + 7) >> 3, 0);

    aInput.read(reinterpret_cast<char*>(chars.data()), chars.size());
    String_to_VectorBool(chars, Booleans);
    aInput.read(reinterpret_cast<char*>(Integers.data()), Integers.size() * sizeof(uint32_t));
    aInput.read(reinterpret_cast<char*>(Floats.data()), Floats.size() * sizeof(float));
}

void AnimationVariables::Save(std::ostream& aOutput) const
{
    // Booleans bitpacked and not guaranteed contiguous.
    TiltedPhoques::String chars;
    VectorBool_to_String(Booleans, chars);
 
    aOutput.write(reinterpret_cast<const char*>(chars.data()), chars.size());
    aOutput.write(reinterpret_cast<const char*>(Integers.data()), Integers.size() * sizeof(uint32_t));
    aOutput.write(reinterpret_cast<const char*>(Floats.data()), Floats.size() * sizeof(float));
}

// Wire format description.
// 
// For Booleans, send the Booleans.size() as a VarInt, then the entire Boolean array (not worth optimizing changes).
// Should be SMALLER than the uint64_t for all but a few behaviors such as Master.
// Then, for each of Integers and Floats, send the size as a VarInt, then send the (possibly empty if size is zero)
// Vector<bool> that specifies which (if any) variables changed, then send the (possibly empty) list of changes.
// 
void AnimationVariables::GenerateDiff(const AnimationVariables& aPrevious, TiltedPhoques::Buffer::Writer& aWriter) const
{
    // Watch for the case we think never happens and requires a different wire format.  
    assert(aPrevious.Booleans.size() == Booleans.size() || Booleans.size() != 0);
    assert(aPrevious.Integers.size() == Integers.size() || Integers.size() != 0);
    assert(aPrevious.Floats.size()   == Floats.size()   || Floats.size()   != 0);

    // Write Booleans length and the value vector.
    TiltedPhoques::String chars;
    VectorBool_to_String(Booleans, chars);
    TiltedPhoques::Serialization::WriteVarInt(aWriter, Booleans.size());
    TiltedPhoques::Serialization::WriteString(aWriter, chars);

    // Find changed Integer vars.
    auto changes = aPrevious.Integers.size() != Integers.size();
    Vector<bool> intChanges(Integers.size(), changes); // If size changes (like at startup), send everything.

    if (!changes)
    {
        auto citer = intChanges.begin();
        for (size_t i = 0; i < Integers.size(); ++i)
        {
            auto change = Integers[i] != aPrevious.Integers[i];
            *citer++ = change;
            changes = changes || change;
        }
    }

    TiltedPhoques::Serialization::WriteVarInt(aWriter, changes ? Integers.size() : 0);
    if (changes)
    {
        VectorBool_to_String(intChanges, chars);
        TiltedPhoques::Serialization::WriteString(aWriter, chars);
        for (size_t i = 0; i < Integers.size(); i++)
            if (intChanges[i])
                TiltedPhoques::Serialization::WriteVarInt(aWriter, Integers[i]);
    }

    // Now do the same for floats.
    changes = aPrevious.Floats.size() != Floats.size();
    Vector<bool> floatChanges(Floats.size(), changes); // If size changes (like at startup), send everything.

    if (!changes)
    {
        auto citer = floatChanges.begin();
        for (size_t i = 0; i < Floats.size(); ++i)
        {
            auto change = Floats[i] != aPrevious.Floats[i];
            *citer++ = change;
            changes = changes || change;
        }
    }

    TiltedPhoques::Serialization::WriteVarInt(aWriter, changes ? Floats.size() : 0);
    if (changes)
    {
        VectorBool_to_String(floatChanges, chars);
        TiltedPhoques::Serialization::WriteString(aWriter, chars);
        for (size_t i = 0; i < floatChanges.size(); i++)
            if (floatChanges[i])
                TiltedPhoques::Serialization::WriteFloat(aWriter, Floats[i]);
    }
}

// Read a VarInt (s) of how big the following Vector<bool> of changes is 
// (we treat all the Booleans as changed), then the Vector<bool> bits.
// For Booleans, this is the value array.
// For Integers the (possibly empty) length S Vector<bool>
// indicates which Integers have changed, and is (possibly empty) followed by those Integers.
// Then the same for Floats.
// 
// The size of the change Vector<bool> is either zero (change nothing)
// or the size of the Vector<float, integer> from the source. So we can resize
// the arrays on this side if they changed at the source. If they DO change
// at the source, the source will always send everything and every bit of the
// changes Vector<bool> will be true.
// 
// This fails if the size CHANGES to zero. Have to think about that.
//
void AnimationVariables::ApplyDiff(TiltedPhoques::Buffer::Reader& aReader)
{
    const auto cBooleanSize = TiltedPhoques::Serialization::ReadVarInt(aReader);
    if (Booleans.size() != cBooleanSize)
        Booleans.assign(cBooleanSize, false);    // Support resize
    auto chars = TiltedPhoques::Serialization::ReadString(aReader);
    String_to_VectorBool(chars, Booleans);

    const auto cIntegerSize = TiltedPhoques::Serialization::ReadVarInt(aReader);
    Vector<bool> intChanges(cIntegerSize);
    if (cIntegerSize)
    {
        chars = TiltedPhoques::Serialization::ReadString(aReader);
        String_to_VectorBool(chars, intChanges);

        if (Integers.size() != cIntegerSize)
            Integers.assign(cIntegerSize, 0);      // Resize, and expect everything to be sent.
        for (size_t i = 0; i < cIntegerSize; i++)
            if (intChanges[i])
                Integers[i] = static_cast<uint32_t>(TiltedPhoques::Serialization::ReadVarInt(aReader));
    }

    const auto cFloatSize = TiltedPhoques::Serialization::ReadVarInt(aReader);
    Vector<bool> floatChanges(cFloatSize);
    if (cFloatSize)
    {
        chars = TiltedPhoques::Serialization::ReadString(aReader);
        String_to_VectorBool(chars, floatChanges);

        if (Floats.size() != cFloatSize)
            Floats.assign(cFloatSize, 0.f); // Resize, and expect everything to be sent.
        for (size_t i = 0; i < cFloatSize; i++)
            if (floatChanges[i])
                Floats[i] = (TiltedPhoques::Serialization::ReadFloat(aReader));
    }
}
