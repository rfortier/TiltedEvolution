#include <TiltedOnlinePCH.h>

#include <Games/References.h>

#include <Forms/BGSAction.h>
#include <Forms/TESIdleForm.h>

#include <Structs/ActionEvent.h>

#include <Games/Animation/ActorMediator.h>
#include <Games/Animation/TESActionData.h>

#include <Misc/BSFixedString.h>

#include <World.h>

#include <BSAnimationGraphManager.h>
#include <TiltedCore/Hash.hpp>
#include <BSCore/BSTHashMap.h>

using StringBSTHashMap = creation::BSTHashMap<BSFixedString, void*>;
using TComputeCrc64 = int64_t (*)(int64_t* outComputedCrc64, const char* value);

TP_THIS_FUNCTION(TPerformAction, uint8_t, ActorMediator, TESActionData* apAction);
TP_THIS_FUNCTION(TAccessAnimationBSTHashMap, int64_t, StringBSTHashMap, BSFixedString* apKey,
                 volatile signed __int32** apOutUnk);
static TPerformAction* RealPerformAction;
static TAccessAnimationBSTHashMap* RealAccessAnimationBSTHashMap;

// TODO: make scoped override
thread_local bool g_forceAnimation = false;

uint8_t TP_MAKE_THISCALL(HookPerformAction, ActorMediator, TESActionData* apAction)
{
    auto pActor = apAction->actor;
    const auto pExtension = pActor->GetExtension();

    if (!pExtension->IsRemote() || g_forceAnimation)
    {
        ActionEvent action;
        action.State1 = pActor->actorState.flags1;
        action.State2 = pActor->actorState.flags2;
        action.Type = apAction->unkInput | (apAction->someFlag ? 0x4 : 0);
        action.Tick = World::Get().GetTick();
        action.ActorId = pActor->formID;
        action.ActionId = apAction->action->formID;
        action.TargetId = apAction->target ? apAction->target->formID : 0;

        pActor->SaveAnimationVariables(action.Variables);

        const auto res = TiltedPhoques::ThisCall(RealPerformAction, apThis, apAction);

        // spdlog::debug("Action event name: {}, target name: {}", apAction->eventName.AsAscii(), apAction->targetEventName.AsAscii());

        // This is a weird case where it gets spammed and doesn't do much, not sure if it still needs to be sent over the network
        if (apAction->someFlag == 1 || g_forceAnimation)
            return res;

        action.EventName = apAction->eventName.AsAscii();
        action.TargetEventName = apAction->targetEventName.AsAscii();
        action.IdleId = apAction->idleForm ? apAction->idleForm->formID : 0;

        // Save for later
        if (res)
            pExtension->LatestAnimation = action;

        World::Get().GetRunner().Trigger(action);

        return res;
    }

#if TP_FALLOUT4
    // Let the ActionInstantInitializeGraphToBaseState event go through
    if (apAction->action->formID == 0x5704c)
        return TiltedPhoques::ThisCall(RealPerformAction, apThis, apAction);
#endif

    return 0;
}

ActorMediator* ActorMediator::Get() noexcept
{
    POINTER_FALLOUT4(ActorMediator*, s_actorMediator, 1358859);
    POINTER_SKYRIMSE(ActorMediator*, s_actorMediator, 403567);

    return *(s_actorMediator.Get());
}

bool ActorMediator::PerformAction(TESActionData* apAction) noexcept
{
    if (apAction->actor->formID == 0x13482)
    {
        /*static Set<uint32_t> s_ids;

        spdlog::error("New frame");
        for(auto i = 0; i < action.Variables.size(); ++i)
        {
            auto& oldVars = pExtension->LatestVariables.Variables;
            auto& newVars = action.Variables;
            if(oldVars[i] != newVars[i] && s_ids.count(i) == 0)
            {
                //s_ids.insert(i);
                spdlog::info("Var {} changed from {} to {}", i, oldVars[i], newVars[i]);
            }
        }*/
        // spdlog::info("Play animation name: {} with idle {:X} and target {:X} and unk {:X}", apAction->action->keyword.AsAscii(), (apAction->idleForm ? apAction->idleForm->formID : 0), (apAction->target ? apAction->target->formID : 0), apAction->unkInput);
    }

    const auto res = TiltedPhoques::ThisCall(RealPerformAction, this, apAction);
    // const auto res = RePerformAction(apAction, aValue);

    if (res && apAction->actor->formID == 0x13482)
    {
        //    spdlog::info("Passed !");
    }

    return res != 0;
}

// Map field offsets
// public:
//  uint64_t _pad00{0};    // 00
//  uint32_t _pad08{0};    // 08
//  uint32_t _capacity{0}; // 0C
//  uint32_t _free{0};     // 10
//  uint32_t _good{0};     // 14
#pragma optimize("", off)
int64_t TP_MAKE_THISCALL(HookAccessAnimationBSTHashMap, StringBSTHashMap, BSFixedString* apKey,
                         volatile signed __int32** apOutUnk)
{
    // Setup
    POINTER_SKYRIMSE(TComputeCrc64, crc64, 68221);
    auto crc64fn = static_cast<TComputeCrc64>(crc64.GetPtr());

    // // Hash
    // int64_t crc64ForKey;
    // crc64fn(&crc64ForKey, apKey->data);
    // Map fields
    int64_t* mapEntriesPtr = reinterpret_cast<int64_t*>((char*)apThis + 0x28);
    uint32_t capacity = *reinterpret_cast<uint32_t*>((char*)apThis + 0x0C);
    uint32_t free = *reinterpret_cast<uint32_t*>((char*)apThis + 0x10);
    uint32_t good = *reinterpret_cast<uint32_t*>((char*)apThis + 0x14);
    bool isHashMapEmpty = ((capacity - free) == 0);

    //if (capacity == 0)
    //{
    //    spdlog::error("[!] Hash map {:X} capacity is 0 for key \"{}\". Returning 0! HashMap state: [capacity={}, free={}, good={}, empty={}, mapEntriesPtr={:X}]",
    //                  (uintptr_t)apThis, apKey->AsAscii(), capacity, free,
    //                  good, isHashMapEmpty ? "Yes" : "No", (uintptr_t)mapEntriesPtr);
    //    return 0;
    //}

    if (capacity > 1024 || free > 1024 || good > 1024)
    {
        spdlog::error("[***] Hash map {:X} is likely CORRUPTED for key \"{}\". Returning 0! HashMap state: [capacity={}, "
                      "free={}, good={}, empty={}, mapEntriesPtr={:X}]",
                      (uintptr_t)apThis, apKey->AsAscii(), capacity, free,
                      good, isHashMapEmpty ? "Yes" : "No", (uintptr_t)mapEntriesPtr);
        //return 0;
    }

    spdlog::info("[!] Accessing \"{}\" in map {:X}. HashMap state: [capacity={}, free={}, good={}, empty={}, mapEntriesPtr={:X}]", apKey->AsAscii(),
                 (uintptr_t)apThis, capacity, free, good, isHashMapEmpty ? "Yes" : "No", (uintptr_t)mapEntriesPtr);

    // Copied code from disassembly
    /*if (mapEntriesPtr)
    {
        int64_t hashKey = crc64ForKey & (uint32_t)(capacity - 1);
        uint64_t* asd = reinterpret_cast<uint64_t*>(mapEntriesPtr + 24 * hashKey + 16);
        spdlog::info(
            "Accessing key \"{}\" in map {:X}, asd is {:X}. Map info: Map info: mapEntries is nullptr? {}, "
            "capacity={}, free={}, good={}, empty={}",
            apKey->AsAscii(), (uintptr_t)apThis, (uintptr_t)asd, mapEntriesPtr == nullptr ? "Yes" : "No",
            capacity, free, good, isHashMapEmpty ? "Yes" : "No");
    }*/

    return RealAccessAnimationBSTHashMap(apThis, apKey, apOutUnk);
}
#pragma optimize("", on)

bool ActorMediator::ForceAction(TESActionData* apAction) noexcept
{
    TP_THIS_FUNCTION(TAnimationStep, uint8_t, ActorMediator, TESActionData*);
    using TApplyAnimationVariables = void*(void*, TESActionData*);

    POINTER_SKYRIMSE(TApplyAnimationVariables, ApplyAnimationVariables, 39004);
    POINTER_SKYRIMSE(TAnimationStep, PerformComplexAction, 38953);
    POINTER_SKYRIMSE(void*, qword_142F271B8, 403566);

    POINTER_FALLOUT4(TAnimationStep, PerformComplexAction, 1445653);
    uint8_t result = 0;

    auto pActor = static_cast<Actor*>(apAction->actor);
    if (!pActor || pActor->animationGraphHolder.IsReady())
    {
        result = TiltedPhoques::ThisCall(PerformComplexAction, this, apAction);

#if TP_SKYRIM64
        ApplyAnimationVariables(*qword_142F271B8.Get(), apAction);
#endif
    }

    return result;
}

ActionInput::ActionInput(uint32_t aParam1, Actor* apActor, BGSAction* apAction, TESObjectREFR* apTarget)
{
    // skip vtable as we never use this directly
    actor = apActor;
    target = apTarget;
    action = apAction;
    unkInput = aParam1;
}

void ActionInput::Release()
{
    actor.Release();
    target.Release();
}

ActionOutput::ActionOutput()
    : eventName("")
    , targetEventName("")
{
    // skip vtable as we never use this directly

    result = 0;
    targetIdleForm = nullptr;
    idleForm = nullptr;
    unk1 = 0;
}

void ActionOutput::Release()
{
    eventName.Release();
    targetEventName.Release();
}

BGSActionData::BGSActionData(uint32_t aParam1, Actor* apActor, BGSAction* apAction, TESObjectREFR* apTarget)
    : ActionInput(aParam1, apActor, apAction, apTarget)
{
    // skip vtable as we never use this directly
    someFlag = 0;
}

TESActionData::TESActionData(uint32_t aParam1, Actor* apActor, BGSAction* apAction, TESObjectREFR* apTarget)
    : BGSActionData(aParam1, apActor, apAction, apTarget)
{
    POINTER_FALLOUT4(void*, s_vtbl, 780201);
    POINTER_SKYRIMSE(void*, s_vtbl, 188603);

    someFlag = false;

    *reinterpret_cast<void**>(this) = s_vtbl.Get();
}

TESActionData::~TESActionData()
{
    ActionOutput::Release();
    ActionInput::Release();
}

static TiltedPhoques::Initializer s_animationHook(
    []()
    {
        POINTER_FALLOUT4(TPerformAction, performAction, 502377);
        POINTER_SKYRIMSE(TPerformAction, performAction, 38949);
#if TP_SKYRIM64
   POINTER_SKYRIMSE(TAccessAnimationBSTHashMap, accessAnimationBSTHashMap, 27420);
#endif

        RealPerformAction = performAction.Get();
#if TP_SKYRIM64
   RealAccessAnimationBSTHashMap = accessAnimationBSTHashMap.Get();
#endif

        TP_HOOK(&RealPerformAction, HookPerformAction);
#if TP_SKYRIM64
   TP_HOOK(&RealAccessAnimationBSTHashMap, HookAccessAnimationBSTHashMap);
#endif
    });
