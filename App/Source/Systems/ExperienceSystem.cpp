#include "Systems/ExperienceSystem.h"

#include "Combat/LevelingMath.h"
#include "Components/ExperienceRewardComponent.h"
#include "Components/LevelComponent.h"
#include "Components/StatsComponent.h"
#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Math/Color.h"
#include "Engine/Math/Vec2f.h"
#include "Engine/Messages/MessageBus.h"
#include "Engine/Render/FloatingTextSystem.h"
#include "Messages/CombatLogEntryMessage.h"

#include <string>

namespace psr {

namespace {
    constexpr Vec2f kFloatingTextDirection{0.0f, -1.0f}; // up -- matches DamageTextSystem's own convention
    constexpr float kFloatingTextSpeed = 1.0f;           // tiles per second
    constexpr float kFloatingTextDuration = 1.0f;        // seconds

    // Same purple/yellow already used for Poison/Shock status markers (see
    // StatusEffectWorldMarkers.cpp) -- no dedicated palette exists yet, so
    // this reuses the closest existing precedent rather than picking new
    // arbitrary RGB literals.
    constexpr Color kExpTextColor{0x7a, 0x3f, 0xd4};
    constexpr Color kLevelUpTextColor{0xd4, 0xc9, 0x3f};
} // namespace

ExperienceSystem::ExperienceSystem(Registry& registry, MessageBus& message_bus, FloatingTextSystem& floating_text,
                                   const LevelingConfig& config)
    : m_registry(&registry), m_message_bus(&message_bus), m_floating_text(&floating_text), m_config(&config)
{
}

void ExperienceSystem::Subscribe(Entity player)
{
    EventHandlerComponent& events = player.GetOrEmplace<EventHandlerComponent>();
    events.Subscribe<AfterDamageEvent, ExperienceSystem>([this](Entity actor, AfterDamageEvent& event)
                                                          { OnDamage(actor, event); });
}

void ExperienceSystem::OnDamage(Entity player, AfterDamageEvent& event)
{
    if (!event.target_defeated)
        return;

    const ExperienceRewardComponent* reward = event.target.TryGet<ExperienceRewardComponent>();
    if (!reward || reward->exp_reward <= 0)
        return;

    GrantExperience(player, reward->exp_reward);
}

void ExperienceSystem::GrantExperience(Entity player, int amount)
{
    LevelComponent& level_component = player.GetOrEmplace<LevelComponent>();
    level_component.current_exp += amount;

    if (const Position* position = player.TryGet<Position>())
        m_floating_text->Spawn(position->tile, "EXP +" + std::to_string(amount), kExpTextColor, kFloatingTextDirection,
                               kFloatingTextSpeed, kFloatingTextDuration);
    m_message_bus->Publish(CombatLogEntryMessage{"Gained " + std::to_string(amount) + " EXP"});

    // A while loop, not an if -- a single big kill (or a low level threshold)
    // can cross more than one level at once.
    while (level_component.current_exp >= ExpRequiredForLevel(level_component.level + 1, *m_config))
        ApplyLevelUp(player, level_component);
}

void ExperienceSystem::ApplyLevelUp(Entity player, LevelComponent& level_component)
{
    const int old_level = level_component.level;
    ++level_component.level;

    const StatsComponent& growth = m_config->stat_growth_per_level;
    StatsComponent& stats = player.GetOrEmplace<StatsComponent>();
    stats.atp += growth.atp;
    stats.ata += growth.ata;
    stats.mst += growth.mst;
    stats.dfp += growth.dfp;
    stats.evp += growth.evp;
    stats.lck += growth.lck;

    if (const Position* position = player.TryGet<Position>())
        m_floating_text->Spawn(position->tile, "LEVEL UP", kLevelUpTextColor, kFloatingTextDirection,
                               kFloatingTextSpeed, kFloatingTextDuration);

    m_message_bus->Publish(CombatLogEntryMessage{"Level up " + std::to_string(old_level) + " -> " +
                                                 std::to_string(level_component.level)});

    // One line per nonzero stat gained, matching the requested format --
    // never all six, since most authored growth tables leave some at zero.
    if (growth.atp != 0)
        m_message_bus->Publish(CombatLogEntryMessage{"ATP +" + std::to_string(growth.atp)});
    if (growth.ata != 0)
        m_message_bus->Publish(CombatLogEntryMessage{"ATA +" + std::to_string(growth.ata)});
    if (growth.mst != 0)
        m_message_bus->Publish(CombatLogEntryMessage{"MST +" + std::to_string(growth.mst)});
    if (growth.dfp != 0)
        m_message_bus->Publish(CombatLogEntryMessage{"DFP +" + std::to_string(growth.dfp)});
    if (growth.evp != 0)
        m_message_bus->Publish(CombatLogEntryMessage{"EVP +" + std::to_string(growth.evp)});
    if (growth.lck != 0)
        m_message_bus->Publish(CombatLogEntryMessage{"LCK +" + std::to_string(growth.lck)});
}

} // namespace psr
