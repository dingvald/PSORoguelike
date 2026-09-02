#pragma once

#include "Combat/LevelingConfig.h"
#include "Engine/ECS/Entity.h"

namespace psr {

class FloatingTextSystem;
class MessageBus;
struct AfterDamageEvent;
struct LevelComponent;

// Grants EXP when the player lands a killing blow, and levels the player up
// (growing their base StatsComponent, per LevelingConfig::stat_growth_per_level)
// whenever accumulated EXP crosses the next level's threshold -- subscribed
// only on the player entity, same reasoning as LootDropSystem: AfterDamageEvent
// is dispatched at the attacker, so subscribing anyone else would also catch
// an enemy defeating the player or another enemy. No-ops silently if the
// defeated entity carries no ExperienceRewardComponent (most enemies, until
// authored otherwise).
class ExperienceSystem
{
public:
    ExperienceSystem(Registry& registry, MessageBus& message_bus, FloatingTextSystem& floating_text,
                     const LevelingConfig& config);

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
    void GrantExperience(Entity player, int amount);
    void ApplyLevelUp(Entity player, LevelComponent& level_component);

    Registry* m_registry;
    MessageBus* m_message_bus;
    FloatingTextSystem* m_floating_text;
    const LevelingConfig* m_config;
};

} // namespace psr
