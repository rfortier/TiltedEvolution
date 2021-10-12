#include <Messages/SpellCastRequest.h>

void SpellCastRequest::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, CasterId);
    Serialization::WriteVarInt(aWriter, SpellFormId);
    Serialization::WriteVarInt(aWriter, CastingSource);
    Serialization::WriteBool(aWriter, IsDualCasting);
    Serialization::WriteVarInt(aWriter, DesiredTarget);
}

void SpellCastRequest::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);

    CasterId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    SpellFormId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    CastingSource = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    IsDualCasting = Serialization::ReadBool(aReader);
    DesiredTarget = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
}
