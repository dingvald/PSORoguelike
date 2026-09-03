#include "Systems/DamageTextSystem.h"

#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/Math/Color.h"
#include "Engine/Math/Vec2f.h"
#include "Engine/Render/FloatingTextSystem.h"

#include <string>

namespace psr {

namespace {
    constexpr Vec2f kDamageTextDirection{0.0f, -1.0f}; // up -- +Y is down, matching Vec2/TileToPixel
    constexpr float kDamageTextSpeed = 1.0f;           // tiles per second
    constexpr float kDamageTextDuration = 1.0f;        // seconds
    constexpr Color kMissTextColor{180, 180, 180};
} // namespace

DamageTextSystem::DamageTextSystem(FloatingTextSystem& floating_text) : m_floating_text(&floating_text) {}

void DamageTextSystem::Subscribe(Entity actor)
{
    EventHandlerComponent& events = actor.GetOrEmplace<EventHandlerComponent>();
    events.Subscribe<AfterDamageEvent, DamageTextSystem>([this](Entity entity, AfterDamageEvent& event)
                                                         { OnDamage(entity, event); });
    events.Subscribe<AttackMissEvent, DamageTextSystem>([this](Entity entity, AttackMissEvent& event)
                                                        { OnMiss(entity, event); });
}

void DamageTextSystem::OnDamage(Entity /*actor*/, AfterDamageEvent& event)
{
    const Position* position = event.target.TryGet<Position>();
    if (!position)
        return;

    const Color text_color = event.is_critical ? Color{255, 255, 0} : Color{255, 255, 255};
    const float text_speed = event.is_critical ? kDamageTextSpeed * 0.8f : kDamageTextSpeed;
    const float text_duration = event.is_critical ? kDamageTextDuration * 1.2f : kDamageTextDuration;
    std::string damage_text = std::to_string(event.amount);
    if (event.is_critical)
        damage_text += '!';

    m_floating_text->Spawn(position->tile, damage_text, text_color, kDamageTextDirection,
                           text_speed, text_duration);
}

void DamageTextSystem::OnMiss(Entity /*actor*/, AttackMissEvent& event)
{
    const Position* position = event.target.TryGet<Position>();
    if (!position)
        return;

    if (!event.target.Has<PlayerControlledComponent>())
        m_floating_text->Spawn(position->tile, "Miss!", kMissTextColor, kDamageTextDirection, kDamageTextSpeed,
                           kDamageTextDuration);
}

} // namespace psr
