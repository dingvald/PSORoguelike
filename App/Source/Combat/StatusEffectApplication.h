#pragma once

#include "Engine/ECS/Entity.h"

#include <cstdint>

namespace psr {

class StatusEffectLibrary;

// Applies (or, if already active, re-applies) one authored status effect to
// target: finds/creates the matching StatusEffectStack on target's
// StatusEffectComponent (emplaced on demand via Entity::GetOrEmplace, mirrors
// MoveAction's own GetOrEmplace<TweenComponent> precedent -- most entities
// never need this component), increments its stack count, and refreshes
// remaining_duration to the freshly-applied effect's authored duration (the
// user's explicit stacking answer: "stack count up, refresh duration").
// No-ops silently if status_effect_id doesn't resolve in library (an
// authoring error, not a runtime condition worth asserting on -- content
// tooling validates ids at save time, see StatusEffectEditorLayer). Dispatches
// AfterStatusEffectsChangedEvent to target if a stack was actually applied.
void ApplyStatusEffect(Entity target, const StatusEffectLibrary& library, std::uint32_t status_effect_id);

// Ticks every active stack on actor by one turn: Poison/Burn stacks deal
// their authored magnitude (times stack count) as self-inflicted damage (via
// the same BeforeDamageEvent/AfterDamageEvent dispatch TechniqueAction's
// self-target branch already uses -- actor is both attacker and target
// here); Freeze/Shock/Confuse stacks are presence-based and carry no
// per-tick effect of their own (TurnCoordinator/StatusEffectComponent's
// Before-event handlers already react to their mere presence, see
// StatusEffectQueries.h). Every stack's remaining_duration is decremented by
// 1 regardless of type; a stack that reaches 0 is removed (the natural
// "cure" ROADMAP.md's M7.3 bullet calls for -- no Cure item/spell content is
// authored by this pass, see the plan's own scope note). A lethal Poison/
// Burn tick can destroy actor -- callers (TurnCoordinator) must check
// Entity::IsValid() afterward, same as after resolving any other action.
// Dispatches AfterStatusEffectsChangedEvent to actor if it's still valid
// once ticking completes.
void TickStatusEffects(Entity actor, const StatusEffectLibrary& library);

} // namespace psr
