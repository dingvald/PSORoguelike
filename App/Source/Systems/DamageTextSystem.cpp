#include "Systems/DamageTextSystem.h"

#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/EventHandlerComponent.h"
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
} // namespace

DamageTextSystem::DamageTextSystem(FloatingTextSystem& floating_text) : m_floating_text(&floating_text) {}

void DamageTextSystem::Subscribe(Entity actor)
{
    EventHandlerComponent& events = actor.GetOrEmplace<EventHandlerComponent>();
    events.Subscribe<AfterDamageEvent, DamageTextSystem>([this](Entity entity, AfterDamageEvent& event)
                                                         { OnDamage(entity, event); });
}

void DamageTextSystem::OnDamage(Entity /*actor*/, AfterDamageEvent& event)
{
    const Position* position = event.target.TryGet<Position>();
    if (!position)
        return;

    m_floating_text->Spawn(position->tile, std::to_string(event.amount), Color{255, 255, 255}, kDamageTextDirection,
                           kDamageTextSpeed, kDamageTextDuration);
}

} // namespace psr
