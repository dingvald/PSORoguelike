#pragma once

#include <cstdint>
#include <entt/entt.hpp>
#include <vector>

namespace psr {

// One applied {status_effect_id, stacks, remaining_duration} instance on an
// entity. status_effect_id resolves into a StatusEffect (type/magnitude/
// duration) via StatusEffectLibrary::Find -- this struct only carries the
// per-instance runtime state (how many stacks, how many turns left), not the
// authored definition itself.
struct StatusEffectStack
{
    std::uint32_t status_effect_id = 0;
    int stacks = 0;
    int remaining_duration = 0;
};

// Accumulated runtime status-effect state for one entity. Deliberately not
// meta-registered (mirrors EquipmentComponent/TweenComponent's "never
// hand-authored in a prefab" precedent) -- this is state ApplyStatusEffect
// builds up over play, never authored content. Added on demand via
// Entity::GetOrEmplace (see StatusEffectApplication.h's ApplyStatusEffect),
// not stamped on every entity up front.
struct StatusEffectComponent
{
    std::vector<StatusEffectStack> active;

    // Reacts to status-affecting event dispatches on this same entity:
    // Confuse redirects BeforeMoveEvent's chosen direction to a random
    // cardinal one; Shock cancels BeforeAttackEvent/BeforePhotonArtCastEvent/
    // BeforeTechniqueCastEvent outright (movement still works); AfterTurnEvent
    // (dispatched once per resolved turn by TurnCoordinator) drives
    // TickStatusEffects -- duration countdown and Poison/Burn damage. Freeze
    // is handled one level up, in TurnCoordinator itself -- it must pre-empt
    // action selection entirely (skip Move too), not just cancel one action
    // type, so it can't be expressed as one of these per-event handlers.
    // Wired via Registry::BindComponentEvents<StatusEffectComponent>() in
    // RegisterComponents.cpp.
    static void AttachHandlers(entt::registry& registry, entt::entity entity);
    static void DetachHandlers(entt::registry& registry, entt::entity entity);
};

} // namespace psr
