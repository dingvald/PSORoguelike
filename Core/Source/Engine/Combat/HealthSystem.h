#pragma once

#include <entt/entt.hpp>

namespace psr {

// Reacts to IncomingDamageEvent dispatched at any HealthComponent-bearing
// entity (Entity::Dispatch, i.e. dispatched at the target itself -- unlike
// Before/AfterDamageEvent's actor-side dispatch, see DamageEvent.h) by
// applying the resolved amount to that entity's own HealthComponent,
// dispatching AfterDamageEvent back at the event's source on the target's
// behalf, and then dispatching DeathEvent (also at the target) if that
// reduced current_hp to 0. The sole writer of HealthComponent::current_hp --
// AttackAction/PhotonArtAction/TechniqueAction/StatusEffectApplication all
// resolve damage down to an amount and dispatch IncomingDamageEvent at the
// target rather than touching HealthComponent themselves.
//
// Also reacts to IncomingHealEvent (see HealEvent.h) the same way, in the
// opposite direction: clamps current_hp up to max_hp and dispatches
// AfterHealEvent back at the source. UseItemAction is the only source so
// far -- it routes a Monomate-style item's HP restore through here rather
// than writing HealthComponent directly, keeping this the one place
// current_hp is ever mutated.
//
// AfterDamageEvent must be dispatched here, before DeathEvent, rather than
// left to the caller: a self-target hit has source == target, so once
// DeathEvent's DeathSystem handler destroys the entity, dispatching
// anything else at it would be invalid -- doing both from inside this one
// handler is what guarantees the ordering stays safe.
//
// Stateless, like TPComponent/EquipmentComponent/StatusEffectComponent's own
// AttachHandlers -- wired to HealthComponent's construct/destroy lifecycle
// via Registry::BindSystemEvents<HealthComponent, HealthSystem>() rather
// than BindComponentEvents, since this behavior belongs to HealthComponent's
// lifecycle without being HealthComponent's own static methods (DeathSystem
// is the sibling system reacting to the same lifecycle, see DeathSystem.h).
class HealthSystem
{
public:
    static void AttachHandlers(entt::registry& registry, entt::entity entity);
    static void DetachHandlers(entt::registry& registry, entt::entity entity);
};

} // namespace psr
