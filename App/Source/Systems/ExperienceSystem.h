#pragma once

#include "Engine/ECS/Entity.h"

namespace psr {

class MessageBus;
struct GrowthCurve;
struct GrowthCurveLevel;
struct AfterDamageEvent;

// Awards XP and applies level-ups when the player lands a killing blow --
// same shape as LootDropSystem: subscribed only on the player entity
// (AfterDamageEvent is dispatched at the attacker, see DamageEvent.h, so
// subscribing anyone else would also catch an enemy defeating the player or
// another enemy). Reads the defeated entity's own ExperienceValueComponent
// (authored directly on the prefab, no separate library lookup, same
// contract as DropTableComponent) and banks it on the player's
// LevelComponent, looping through GrowthCurve::Find(level+1) to apply every
// level-up a single kill's XP crosses (carrying any remainder xp forward),
// overwriting HealthComponent::max_hp/TPComponent::max_tp/StatsComponent
// from the growth curve's absolute per-level values and fully restoring
// current HP/TP. No-ops silently if the defeated entity carries no
// ExperienceValueComponent (most enemies grant nothing yet, same as loot).
class ExperienceSystem
{
public:
    ExperienceSystem(MessageBus& message_bus, const GrowthCurve& growth_curve);

    // Subscribed handler captures this instance's address -- neither copying
    // nor moving would keep it valid, same rationale as LootDropSystem's
    // identical restriction.
    ExperienceSystem(const ExperienceSystem&) = delete;
    ExperienceSystem& operator=(const ExperienceSystem&) = delete;
    ExperienceSystem(ExperienceSystem&&) = delete;
    ExperienceSystem& operator=(ExperienceSystem&&) = delete;

    // Wires the player's EventHandlerComponent to this instance. Call once,
    // for the player only -- see the class doc comment for why.
    void Subscribe(Entity player);

private:
    void OnDamage(Entity player, AfterDamageEvent& event);
    void ApplyLevelUp(Entity player, const GrowthCurveLevel& level_data);

    // Duplicates CombatLogBridge::PublishPlayerStatus's ~10-line body rather
    // than taking a CombatLogBridge& dependency this system otherwise has no
    // need for -- small, self-contained duplication over a new cross-system
    // coupling, same call this codebase already makes for e.g.
    // KeyCodeToHotbarSlot.
    void PublishPlayerStatus(Entity player);

    MessageBus* m_message_bus;
    const GrowthCurve* m_growth_curve;
};

} // namespace psr
