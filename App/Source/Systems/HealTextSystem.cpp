#include "Systems/HealTextSystem.h"

#include "Engine/Combat/HealEvent.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/Math/Color.h"
#include "Engine/Math/Vec2f.h"
#include "Engine/Render/FloatingTextSystem.h"

#include <string>

namespace psr {

namespace {
    constexpr Vec2f kHealTextDirection{0.0f, -1.0f}; // up -- +Y is down, matching Vec2/TileToPixel
    constexpr float kHealTextSpeed = 1.0f;           // tiles per second
    constexpr float kHealTextDuration = 1.0f;        // seconds
    constexpr Color kHealTextColor{0, 255, 0};
} // namespace

HealTextSystem::HealTextSystem(FloatingTextSystem& floating_text) : m_floating_text(&floating_text) {}

void HealTextSystem::Subscribe(Entity actor)
{
    EventHandlerComponent& events = actor.GetOrEmplace<EventHandlerComponent>();
    events.Subscribe<AfterHealEvent, HealTextSystem>([this](Entity entity, AfterHealEvent& event)
                                                      { OnHeal(entity, event); });
}

void HealTextSystem::OnHeal(Entity /*actor*/, AfterHealEvent& event)
{
    if (event.amount <= 0)
        return;

    const Position* position = event.target.TryGet<Position>();
    if (!position)
        return;

    m_floating_text->Spawn(position->tile, "+" + std::to_string(event.amount), kHealTextColor, kHealTextDirection,
                           kHealTextSpeed, kHealTextDuration);
}

} // namespace psr
