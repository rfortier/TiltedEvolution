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
    chars.clear();
    chars.resize((bools.size() + 7) >> 3, 0);

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

// Read a VarInt of how many changed Boolean vars there are (we treat all the Booleans as changed),
// then an array of Booleans indicating which ones have changed (possibly empty), then
// the changed values (for Ints and Floats).
void AnimationVariables::ApplyDiff(TiltedPhoques::Buffer::Reader& aReader)
{
    const auto cBooleanSize = TiltedPhoques::Serialization::ReadVarInt(aReader);
    if (Booleans.size() != cBooleanSize)
        Booleans.assign(cBooleanSize, false);    // Support resize
    TiltedPhoques::String chars((Booleans.size() + 7) >> 3, 0);
    chars = TiltedPhoques::Serialization::ReadString(aReader);
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
