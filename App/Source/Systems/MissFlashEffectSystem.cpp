#include "Systems/MissFlashEffectSystem.h"

#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/Math/Easing.h"
#include "Engine/Render/VisualEffectSystem.h"

#include <entt/core/hashed_string.hpp>

namespace psr {

namespace {
    constexpr const char* kMissFlashPrefabId = "vfx.miss_flash";
    constexpr float kMissFlashDuration = 0.9f; // seconds
} // namespace

MissFlashEffectSystem::MissFlashEffectSystem(VisualEffectSystem& visual_effects, entt::entity player)
    : m_visual_effects(&visual_effects), m_player(player)
{
}

void MissFlashEffectSystem::Subscribe(Entity actor)
{
    EventHandlerComponent& events = actor.GetOrEmplace<EventHandlerComponent>();
    events.Subscribe<AttackMissEvent, MissFlashEffectSystem>([this](Entity entity, AttackMissEvent& event)
                                                             { OnMiss(entity, event); });
}

void MissFlashEffectSystem::OnMiss(Entity /*actor*/, AttackMissEvent& event)
{
    if (event.target.Handle() != m_player)
        return;

    const Position* position = event.target.TryGet<Position>();
    if (!position)
        return;

    m_visual_effects->Spawn(entt::hashed_string::value(kMissFlashPrefabId), position->tile, kMissFlashDuration,
                            EasingCurve::EaseOutQuad);
}

} // namespace psr
