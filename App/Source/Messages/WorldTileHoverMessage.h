#pragma once

#include <string>

namespace psr {

// GameplayLayer's response to a WorldMouseMoveMessage -- see
// GameplayLayer::OnWorldMouseMove and HudLayer::OnWorldTileHover. label is
// the hovered tile's first occupant's DisplayName; has_content is false for
// an empty tile (or no player entity to resolve against yet), in which case
// label is meaningless and HudLayer hides its tooltip. Deliberately
// name-only for now -- a richer stat-card tooltip is a natural follow-up,
// same relationship InventoryItemHoverChangedMessage has to
// CharacterScreenStatPreviewMessage's fuller payload.
struct WorldTileHoverMessage
{
    bool has_content = false;
    std::string label;
};

} // namespace psr
