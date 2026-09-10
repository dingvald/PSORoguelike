#include "Systems/HealEffectSystem.h"

#include "Engine/Combat/HealEvent.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/Math/Easing.h"
#include "Engine/Render/VisualEffectSystem.h"

#include <entt/core/hashed_string.hpp>

namespace psr {

namespace {
    constexpr const char* kHealEffectPrefabId = "vfx.heal_glow";
    constexpr float kHealEffectDuration = 0.6f; // seconds
} // namespace

HealEffectSystem::HealEffectSystem(VisualEffectSystem& visual_effects) : m_visual_effects(&visual_effects) {}

void HealEffectSystem::Subscribe(Entity actor)
{
    EventHandlerComponent& events = actor.GetOrEmplace<EventHandlerComponent>();
    events.Subscribe<AfterHealEvent, HealEffectSystem>([this](Entity entity, AfterHealEvent& event)
                                                        { OnHeal(entity, event); });
}

void HealEffectSystem::OnHeal(Entity /*actor*/, AfterHealEvent& event)
{
    if (event.amount <= 0)
        return;

    const Position* position = event.target.TryGet<Position>();
    if (!position)
        return;

    m_visual_effects->Spawn(entt::hashed_string::value(kHealEffectPrefabId), position->tile, kHealEffectDuration,
                            EasingCurve::EaseOutQuad);
}

} // namespace psr
