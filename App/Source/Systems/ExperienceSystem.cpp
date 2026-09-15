#include "Systems/ExperienceSystem.h"

#include "Components/ExperienceValueComponent.h"
#include "Components/LevelComponent.h"
#include "Components/StatsComponent.h"
#include "Components/TPComponent.h"
#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/Math/Color.h"
#include "Engine/Math/Vec2f.h"
#include "Engine/Messages/MessageBus.h"
#include "Engine/Render/FloatingTextSystem.h"
#include "Messages/CombatLogEntryMessage.h"
#include "Messages/PlayerStatusMessage.h"
#include "Progression/GrowthCurve.h"

#include <string>
#include <utility>

namespace psr {

namespace {
    constexpr Vec2f kLevelUpTextDirection{0.0f, -1.0f}; // up -- +Y is down, matching Vec2/TileToPixel
    constexpr float kLevelUpTextSpeed = 0.6f;           // tiles per second -- slower than a damage number
    constexpr float kLevelUpTextDuration = 1.5f;        // seconds -- lingers longer than a damage number

    // Appends "<label> +[c=#7ee787]<delta>[/c]" to `text` (comma-separated
    // from anything already there), or leaves `text` untouched when delta
    // isn't positive -- a stat the growth curve didn't move for this level
    // stays out of the message rather than showing "+0".
    std::string AppendDelta(std::string text, const char* label, int delta)
    {
        if (delta <= 0)
            return text;

        if (!text.empty())
            text += ", ";
        text += std::string(label) + " +[c=#7ee787]" + std::to_string(delta) + "[/c]";
        return text;
    }
} // namespace

ExperienceSystem::ExperienceSystem(MessageBus& message_bus, const GrowthCurve& growth_curve,
                                   FloatingTextSystem& floating_text, std::mt19937& rng)
    : m_message_bus(&message_bus), m_growth_curve(&growth_curve), m_floating_text(&floating_text), m_rng(&rng)
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
    level.total_xp += xp_value->xp;
    m_message_bus->Publish(
        CombatLogEntryMessage{"Player gained [c=#f6470a]" + std::to_string(xp_value->xp) + "[/c] XP"});

    bool leveled_up = false;
    for (GrowthCurveLevel next = m_growth_curve->Evaluate(level.level + 1); level.xp >= next.xp_to_next;
        next = m_growth_curve->Evaluate(level.level + 1))
    {
        level.xp -= next.xp_to_next;
        level.level = next.level;
        leveled_up = true;

        const GrowthCurveLevel gain = m_growth_curve->EvaluateRandomGain(level.level, *m_rng);
        const std::string deltas = ApplyLevelUp(player, gain);
        std::string log_line = "[b][c=#f6470a]Player reached level " + std::to_string(level.level) + "![/c][/b]";
        if (!deltas.empty())
            log_line += " " + deltas;
        m_message_bus->Publish(CombatLogEntryMessage{log_line});
    }

    if (leveled_up)
    {
        PublishPlayerStatus(player);
        if (const Position* position = player.TryGet<Position>())
            m_floating_text->Spawn(position->tile, "LEVEL UP!", Color{"#f6470a"}, kLevelUpTextDirection,
                                   kLevelUpTextSpeed, kLevelUpTextDuration);
    }
}

std::string ExperienceSystem::ApplyLevelUp(Entity player, const GrowthCurveLevel& gain)
{
    std::string deltas;

    if (HealthComponent* health = player.TryGet<HealthComponent>())
    {
        deltas = AppendDelta(std::move(deltas), "HP", gain.max_hp);
        health->max_hp += gain.max_hp;
        health->current_hp = health->max_hp;
    }

    if (TPComponent* tp = player.TryGet<TPComponent>())
    {
        deltas = AppendDelta(std::move(deltas), "TP", gain.max_tp);
        tp->max_tp += gain.max_tp;
        tp->current_tp = tp->max_tp;
    }

    if (StatsComponent* stats = player.TryGet<StatsComponent>())
    {
        deltas = AppendDelta(std::move(deltas), "ATP", gain.stats.atp);
        stats->atp += gain.stats.atp;
        deltas = AppendDelta(std::move(deltas), "ATA", gain.stats.ata);
        stats->ata += gain.stats.ata;
        deltas = AppendDelta(std::move(deltas), "MST", gain.stats.mst);
        stats->mst += gain.stats.mst;
        deltas = AppendDelta(std::move(deltas), "DFP", gain.stats.dfp);
        stats->dfp += gain.stats.dfp;
        deltas = AppendDelta(std::move(deltas), "EVP", gain.stats.evp);
        stats->evp += gain.stats.evp;
        deltas = AppendDelta(std::move(deltas), "LCK", gain.stats.lck);
        stats->lck += gain.stats.lck;
    }

    return deltas;
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
