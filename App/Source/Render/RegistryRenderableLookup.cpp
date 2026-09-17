#include "Render/RegistryRenderableLookup.h"

#include "Components/MagComponent.h"
#include "Components/RenderableComponent.h"
#include "Components/TweenComponent.h"
#include "Engine/Math/Easing.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace psr {

namespace {
    // The mag's idle float: +-2px at 1x zoom, one full up-down-up cycle
    // every kMagBobPeriodSeconds. Expressed in tile-fraction units (like
    // every other GetRenderOffset contribution), so it scales with zoom for
    // free once TileRenderer multiplies by the current zoomed tile size --
    // see TileVertexMath.h's TileToPixel. kTileHeight duplicates
    // GameplayLayer.cpp's own file-local constant of the same value (the
    // engine's fixed tile pixel height); keep the two in sync if either
    // changes.
    constexpr int kTileHeight = 24;
    constexpr float kMagBobAmplitudePixels = 2.0f;
    constexpr float kMagBobAmplitudeTiles = kMagBobAmplitudePixels / static_cast<float>(kTileHeight);
    constexpr float kMagBobPeriodSeconds = 2.0f;
    constexpr float kMagBobAngularFrequency = 2.0f * std::numbers::pi_v<float> / kMagBobPeriodSeconds;
} // namespace

RegistryRenderableLookup::RegistryRenderableLookup(Registry& registry, AnimationClock& animation_clock)
    : m_registry(&registry), m_animation_clock(&animation_clock)
{
    registry.OnDestroy<RenderableComponent, &RegistryRenderableLookup::OnRenderableDestroyed>(*this);
}

RegistryRenderableLookup::~RegistryRenderableLookup()
{
    m_registry->DisconnectComponentLifecycle<RenderableComponent>(*this);
}

void RegistryRenderableLookup::OnRenderableDestroyed(entt::registry&, entt::entity entity)
{
    m_animation_clock->Forget(entity);
}

std::optional<RenderableTile> RegistryRenderableLookup::GetRenderableTile(entt::entity entity) const
{
    const RenderableComponent* component = m_registry->TryGetComponent<RenderableComponent>(entity);
    if (!component)
        return std::nullopt;

    Vec2 uv = component->uv;
    if (component->frames > 1)
    {
        uv.x += component->is_synced ? m_animation_clock->GetFrameIndex(component->frame_time, component->frames)
                                      : m_animation_clock->GetFrameIndex(entity, component->frame_time, component->frames);
    }

    return RenderableTile{component->texture_id, component->texture_size, uv,
                          component->color_1,    component->color_2,      component->render_layer};
}

Vec2f RegistryRenderableLookup::GetRenderOffset(entt::entity entity) const
{
    Vec2f offset;

    if (const TweenComponent* tween_component = m_registry->TryGetComponent<TweenComponent>(entity);
        tween_component && !tween_component->queue.empty())
    {
        const Tween& active = tween_component->queue.front();
        const float progress =
            active.duration > 0.0f ? std::clamp(active.elapsed / active.duration, 0.0f, 1.0f) : 1.0f;
        offset = active.start_offset + (active.end_offset - active.start_offset) * EaseOutQuad(progress);
    }

    if (const MagComponent* mag = m_registry->TryGetComponent<MagComponent>(entity))
        offset.y += std::sin(mag->bob_elapsed * kMagBobAngularFrequency) * kMagBobAmplitudeTiles;

    return offset;
}

} // namespace psr
