#include "Render/FogOfWarRenderableLookup.h"

#include "Components/AiComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Engine/ECS/Position.h"

#include <algorithm>
#include <cstdint>

namespace psr {

namespace {
    constexpr float kExploredDarkenFactor = 0.35f;

    Color Darken(Color color, float factor)
    {
        const auto scale = [factor](std::uint8_t channel)
        { return static_cast<std::uint8_t>(std::clamp(static_cast<float>(channel) * factor, 0.0f, 255.0f)); };
        return Color{scale(color.r), scale(color.g), scale(color.b), color.a};
    }
} // namespace

FogOfWarRenderableLookup::FogOfWarRenderableLookup(Registry& registry, const RoomMap& room_map,
                                                   const RoomVisibilityTracker& visibility,
                                                   const IRenderableLookup& inner)
    : m_registry(&registry), m_room_map(&room_map), m_visibility(&visibility), m_inner(&inner)
{
}

std::optional<RenderableTile> FogOfWarRenderableLookup::GetRenderableTile(entt::entity entity) const
{
    const Position* position = m_registry->TryGetComponent<Position>(entity);
    if (!position)
        return m_inner->GetRenderableTile(entity); // no room to gate by (e.g. a status marker/target cursor)

    const RoomVisibility room_visibility = m_visibility->GetVisibility(m_room_map->GetRoom(position->tile));
    if (room_visibility == RoomVisibility::Hidden)
        return std::nullopt;

    if (room_visibility == RoomVisibility::Explored &&
        (m_registry->HasComponent<AiComponent>(entity) || m_registry->HasComponent<PlayerControlledComponent>(entity)))
        return std::nullopt;

    std::optional<RenderableTile> tile = m_inner->GetRenderableTile(entity);
    if (tile && room_visibility == RoomVisibility::Explored)
    {
        tile->color_1 = Darken(tile->color_1, kExploredDarkenFactor);
        tile->color_2 = Darken(tile->color_2, kExploredDarkenFactor);
    }
    return tile;
}

Vec2f FogOfWarRenderableLookup::GetRenderOffset(entt::entity entity) const { return m_inner->GetRenderOffset(entity); }

} // namespace psr
