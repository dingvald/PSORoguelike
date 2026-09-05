#include "Systems/ExperienceSystem.h"

#include "Components/ExperienceValueComponent.h"
#include "Components/LevelComponent.h"
#include "Components/StatsComponent.h"
#include "Components/TPComponent.h"
#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/Messages/MessageBus.h"
#include "Messages/CombatLogEntryMessage.h"
#include "Messages/PlayerStatusMessage.h"
#include "Progression/GrowthCurve.h"

#include <string>

namespace psr {

ExperienceSystem::ExperienceSystem(MessageBus& message_bus, const GrowthCurve& growth_curve)
    : m_message_bus(&message_bus), m_growth_curve(&growth_curve)
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

    const ExperienceValueComponent* xp_value = event.target.TryGet<ExperienceValueComponent>();
    if (!xp_value)
        return;

    LevelComponent* level_ptr = player.TryGet<LevelComponent>();
    if (!level_ptr)
        return;
    LevelComponent& level = *level_ptr;

    level.xp += xp_value->xp;
    m_message_bus->Publish(
        CombatLogEntryMessage{"Player gained [c=#f6470a]" + std::to_string(xp_value->xp) + "[/c] XP"});

    bool leveled_up = false;
    while (const GrowthCurveLevel* next = m_growth_curve->Find(level.level + 1))
    {
        if (level.xp < next->xp_to_next)
            break;

        level.xp -= next->xp_to_next;
        level.level = next->level;
        leveled_up = true;

        ApplyLevelUp(player, *next);
        m_message_bus->Publish(
            CombatLogEntryMessage{"[b][c=#f6470a]Player reached level " + std::to_string(level.level) + "![/c][/b]"});
    }

    if (leveled_up)
        PublishPlayerStatus(player);
}

void ExperienceSystem::ApplyLevelUp(Entity player, const GrowthCurveLevel& level_data)
{
    if (HealthComponent* health = player.TryGet<HealthComponent>())
    {
        health->max_hp = level_data.max_hp;
        health->current_hp = level_data.max_hp;
    }

    if (TPComponent* tp = player.TryGet<TPComponent>())
    {
        tp->max_tp = level_data.max_tp;
        tp->current_tp = level_data.max_tp;
    }

    if (StatsComponent* stats = player.TryGet<StatsComponent>())
        *stats = level_data.stats;
}

void ExperienceSystem::PublishPlayerStatus(Entity player)
{
    PlayerStatusMessage status;
    if (const HealthComponent* health = player.TryGet<HealthComponent>())
    {
        status.current_hp = health->current_hp;
        status.max_hp = health->max_hp;
    }

    if (const TPComponent* tp = player.TryGet<TPComponent>())
    {
        status.has_secondary = true;
        status.current_secondary = tp->current_tp;
        status.max_secondary = tp->max_tp;
    }

    if (const LevelComponent* level = player.TryGet<LevelComponent>())
        status.level = level->level;

    m_message_bus->Publish(status);
}

} // namespace psr
