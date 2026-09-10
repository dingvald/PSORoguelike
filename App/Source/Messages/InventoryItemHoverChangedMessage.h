#pragma once

namespace psr {

// Published by HudLayer whenever the Character screen's "preview target"
// inventory row changes -- the row currently under the mouse, or (if nothing
// is hovered) the keyboard-focused Inventory row, whichever is an equippable
// item; see HudLayer::UpdateStatPreview. GameplayLayer responds with
// CharacterScreenStatPreviewMessage. -1 means "no preview" (nothing hovered/
// focused, focus isn't on the Inventory panel, or the row isn't equippable).
struct InventoryItemHoverChangedMessage
{
    int inventory_index = -1;
};

} // namespace psr
