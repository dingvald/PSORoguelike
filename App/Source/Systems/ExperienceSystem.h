#pragma once

#include "Engine/ECS/Entity.h"

#include <random>
#include <string>

namespace psr {

class MessageBus;
class FloatingTextSystem;
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
// LevelComponent, looping through GrowthCurve::Evaluate(level+1) to apply
// every level-up a single kill's XP crosses (carrying any remainder xp forward),
// adding GrowthCurve::EvaluateRandomGain's randomized HP/TP/stat gains onto
// the player's current totals (see ApplyLevelUp) and fully restoring current
// HP/TP. No-ops silently if the defeated entity carries no
// ExperienceValueComponent (most enemies grant nothing yet, same as loot).
// Spawns one "LEVEL UP!" floating text via FloatingTextSystem regardless of
// how many levels a single kill's XP crosses (see the leveled_up flag in
// OnDamage) -- same "act once after the loop" shape PublishPlayerStatus
// already uses there.
class ExperienceSystem
{
public:
    ExperienceSystem(MessageBus& message_bus, const GrowthCurve& growth_curve, FloatingTextSystem& floating_text,
                     std::mt19937& rng);

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

    // Adds `gain`'s (already-randomized, see GrowthCurve::EvaluateRandomGain)
    // HP/TP/stat amounts onto HealthComponent::max_hp/TPComponent::max_tp/
    // StatsComponent, restoring current HP/TP to the new max, and returns the
    // gains as inline log markup (e.g. "HP +[c=#7ee787]20[/c], ATP
    // +[c=#7ee787]3[/c]"), omitting any stat gain that isn't positive. Empty
    // string if `gain` grants nothing this level.
    std::string ApplyLevelUp(Entity player, const GrowthCurveLevel& gain);

    // Duplicates CombatLogBridge::PublishPlayerStatus's ~10-line body rather
    // than taking a CombatLogBridge& dependency this system otherwise has no
    // need for -- small, self-contained duplication over a new cross-system
    // coupling, same call this codebase already makes for e.g.
    // KeyCodeToHotbarSlot.
    void PublishPlayerStatus(Entity player);

    MessageBus* m_message_bus;
    const GrowthCurve* m_growth_curve;
    FloatingTextSystem* m_floating_text;
    std::mt19937* m_rng;
};

} // namespace psr
