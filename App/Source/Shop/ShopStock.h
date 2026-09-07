#pragma once

#include <string>
#include <vector>

namespace psr {

// One entry in the hub shop's fixed catalog -- a plain prefab-id string
// (resolved via entt::hashed_string::value at use time, same convention
// kDungeonId/kPlayerPrefabId already use), not a NameId round-tripped
// through NameIdRegistry -- this is a small, hand-curated flat list, not a
// per-item content library, so there's no separate id/id_string split to
// maintain. buy_price is independently authored from the item's own
// ValueComponent::base_price (its sell value) -- see ValueComponent.h's own
// doc comment for why.
struct ShopStockEntry
{
    std::string prefab_id_string;
    int buy_price = 0;
};

// The hub shop's fixed catalog -- a single hand-authored document
// (App/Assets/Data/shop_stock.json), not a per-item content library, same
// shape as GrowthCurve.h's growth curve. Unlike growth_curve.json, this one
// has an editor (Editor/Source/Layers/ShopStockEditorLayer) since it's a
// growing flat list a content author edits routinely.
struct ShopStock
{
    std::vector<ShopStockEntry> entries;
};

} // namespace psr
