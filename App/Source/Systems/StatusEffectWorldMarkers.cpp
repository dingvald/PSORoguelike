#include "Systems/StatusEffectWorldMarkers.h"

#include "Combat/StatusEffect.h"
#include "Combat/StatusEffectEvent.h"
#include "Combat/StatusEffectLibrary.h"
#include "Components/RenderableComponent.h"
#include "Components/StatusEffectComponent.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/World/Grid.h"

#include <entt/core/hashed_string.hpp>

#include <algorithm>

namespace psr {

namespace {
    constexpr const char* kMarkerPrefabId = "ui.status_effect_marker";

    // A tint per StatusEffectType, matching HudLayer's own status-chip
    // colors (hud.rcss's .status-chip.status-<class> rules) so the world
    // marker and the HUD chip read as the same ailment at a glance.
    Color MarkerColor(StatusEffectType type)
    {
        switch (type)
        {
        case StatusEffectType::Poison:
            return Color{0x7a, 0x3f, 0xd4};
        case StatusEffectType::Burn:
            return Color{0xd4, 0x57, 0x3f};
        case StatusEffectType::Freeze:
            return Color{0x3f, 0x9f, 0xd4};
        case StatusEffectType::Shock:
            return Color{0xd4, 0xc9, 0x3f};
        case StatusEffectType::Confuse:
            return Color{0xd4, 0x3f, 0xa0};
        }
        return Color{0xff, 0xff, 0xff}; // unreachable for a valid enum value
    }
} // namespace

StatusEffectWorldMarkers::StatusEffectWorldMarkers(Registry& registry, Grid& grid,
                                                   const StatusEffectLibrary& status_effects)
    : m_registry(&registry), m_grid(&grid), m_status_effects(&status_effects)
{
}

StatusEffectWorldMarkers::~StatusEffectWorldMarkers() { ClearMarkers(); }

void StatusEffectWorldMarkers::Subscribe(Entity tracked)
{
    m_tracked = tracked.Handle();
    EventHandlerComponent& events = tracked.GetOrEmplace<EventHandlerComponent>();
    events.Subscribe<AfterStatusEffectsChangedEvent, StatusEffectWorldMarkers>(
        [this](Entity actor, AfterStatusEffectsChangedEvent& event) { OnStatusEffectsChanged(actor, event); });
}

void StatusEffectWorldMarkers::ClearMarkers()
{
    for (entt::entity marker : m_markers)
    {
        m_grid->RemoveEntity(m_marker_tile, marker);
        m_registry->DestroyEntity(marker);
    }
    m_markers.clear();
}

void StatusEffectWorldMarkers::OnStatusEffectsChanged(Entity actor, AfterStatusEffectsChangedEvent& /*event*/)
{
    if (actor.Handle() != m_tracked)
        return;

    ClearMarkers();

    if (!actor.IsValid())
        return; // the tick that produced this event just destroyed actor (lethal DoT)

    const StatusEffectComponent* status = actor.TryGet<StatusEffectComponent>();
    if (!status || status->active.empty())
        return;

    std::vector<StatusEffectType> distinct_types;
    for (const StatusEffectStack& stack : status->active)
    {
        const StatusEffect* effect = m_status_effects->Find(stack.status_effect_id);
        if (!effect)
            continue;
        if (std::find(distinct_types.begin(), distinct_types.end(), effect->type) == distinct_types.end())
            distinct_types.push_back(effect->type);
    }
    if (distinct_types.empty())
        return;

    m_marker_tile = actor.Get<Position>().tile;
    for (StatusEffectType type : distinct_types)
    {
        const entt::entity marker = m_registry->CreateEntity(entt::hashed_string::value(kMarkerPrefabId));
        if (marker == entt::null)
            continue;
        if (RenderableComponent* renderable = m_registry->TryGetComponent<RenderableComponent>(marker))
        {
            const Color tint = MarkerColor(type);
            renderable->color_1 = tint;
            renderable->color_2 = tint;
        }
        m_grid->AddEntity(m_marker_tile, marker);
        m_markers.push_back(marker);
    }
}

} // namespace psr
