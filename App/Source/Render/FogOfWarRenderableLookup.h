#pragma once

#include "Engine/Dungeon/RoomMap.h"
#include "Engine/Dungeon/RoomVisibilityTracker.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Render/IRenderableLookup.h"

namespace psr {

// Decorates another IRenderableLookup with room-granularity fog of war:
// entities in a never-visited room don't render at all; entities in a
// previously-visited-but-not-current room render via `inner`, then dimmed,
// with actors (AiComponent/PlayerControlledComponent) hidden entirely;
// entities in the player's current room render via `inner` unchanged. Kept
// separate from RegistryRenderableLookup (rather than added to it) because
// PieceEditorLayer/DungeonEditorLayer construct RegistryRenderableLookup
// directly for authoring-time previews, which must keep showing everything
// unconditionally.
class FogOfWarRenderableLookup : public IRenderableLookup
{
public:
    FogOfWarRenderableLookup(Registry& registry, const RoomMap& room_map, const RoomVisibilityTracker& visibility,
                             const IRenderableLookup& inner);

    std::optional<RenderableTile> GetRenderableTile(entt::entity entity) const override;
    Vec2f GetRenderOffset(entt::entity entity) const override;

private:
    Registry* m_registry;
    const RoomMap* m_room_map;
    const RoomVisibilityTracker* m_visibility;
    const IRenderableLookup* m_inner;
};

} // namespace psr
