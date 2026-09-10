#pragma once

namespace psr {

// GameplayLayer's response to InventoryItemHoverChangedMessage -- the six
// core-stat deltas (EquipPreview.h's ComputeEquipStatDelta) from
// hypothetically equipping the requested inventory item. !active means the
// requested row wasn't equippable (or nothing was requested); HudLayer's
// RenderStatsPanel falls back to plain stat rows in that case.
struct CharacterScreenStatPreviewMessage
{
    bool active = false;
    int atp_delta = 0;
    int ata_delta = 0;
    int mst_delta = 0;
    int dfp_delta = 0;
    int evp_delta = 0;
    int lck_delta = 0;
};

} // namespace psr
