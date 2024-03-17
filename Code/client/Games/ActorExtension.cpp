#include <TiltedOnlinePCH.h>

#include <Games/ActorExtension.h>
#include "RandomServices.h"

bool ActorExtension::IsRemote() const noexcept
{
    return onlineFlags & kRemote;
}

bool ActorExtension::IsLocal() const noexcept
{
    return !IsRemote();
}

bool ActorExtension::IsPlayer() const noexcept
{
    return onlineFlags & kPlayer;
}

bool ActorExtension::IsRemotePlayer() const noexcept
{
    return IsRemote() && IsPlayer();
}

bool ActorExtension::IsLocalPlayer() const noexcept
{
    return IsLocal() && IsPlayer();
}

void ActorExtension::SetRemote(bool aSet) noexcept
{
    if (aSet)
    {
        onlineFlags |= kRemote;
        ActorClaimedTimeout = RandomServices::Get().NullTime(); // Reset deadline for checks after claimed.
    }

    else
    {
        onlineFlags &= ~kRemote;

        // Related to cosideci TODO on CharacterService::TakeOwnership,
        // fights over ownership can occur, with a resulting concurrency
        // bug of NPCs not being equipped. If we can keep ownership for a random
        // duration, make sure they are equipped. Randomized
        // to smooth out spawn / equip storms.
        ActorClaimedTimeout = RandomServices::Get().RandomizedDeadline();
    }
}

void ActorExtension::SetPlayer(bool aSet) noexcept
{
    if (aSet)
        onlineFlags |= kPlayer;
    else
        onlineFlags &= ~kPlayer;
}
