#include "Systems/OnHitEffectSystem.h"

#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/Math/Easing.h"
#include "Engine/Render/VisualEffectSystem.h"

namespace psr {

OnHitEffectSystem::OnHitEffectSystem(VisualEffectSystem& visual_effects) : m_visual_effects(&visual_effects) {}

void OnHitEffectSystem::Subscribe(Entity actor)
{
    EventHandlerComponent& events = actor.GetOrEmplace<EventHandlerComponent>();
    events.Subscribe<AfterDamageEvent, OnHitEffectSystem>([this](Entity entity, AfterDamageEvent& event)
                                                          { OnHit(entity, event); });
}

void OnHitEffectSystem::OnHit(Entity /*actor*/, AfterDamageEvent& event)
{
    if (event.hit_effect_prefab_id == 0)
        return;

    const Position* position = event.target.TryGet<Position>();
    if (!position)
        return;

    m_visual_effects->Spawn(event.hit_effect_prefab_id, position->tile, event.hit_effect_duration, EasingCurve::Linear,
                            255, 255);
}

} // namespace psr
