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
    // Related to cosideci TODO on CharacterService::TakeOwnership,
    // fights over ownership can occur, with a resulting concurrency
    // bug of NPCs not being equipped. After ownership is stable for a
    // randomized duration, make sure they are equipped. Randomized
    // to smooth out spawn / equip storms.
    ActorClaimedDeadline = RandomServices::Get().RandomizedDeadline();

    if (aSet)
        onlineFlags |= kRemote;
    else
        onlineFlags &= ~kRemote;
}

void ActorExtension::SetPlayer(bool aSet) noexcept
{
    if (aSet)
        onlineFlags |= kPlayer;
    else
        onlineFlags &= ~kPlayer;
}
