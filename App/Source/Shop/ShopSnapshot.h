#pragma once

#include "Messages/ShopMessage.h"

#include <entt/entt.hpp>

namespace psr {

class Registry;
class AffixLibrary;
struct ShopStock;

// Resolves stock's catalog plus player's InventoryComponent into a
// ShopMessage -- display names via FormatItemDisplayName (stock rows read
// through Registry::GetPrefabEntity, the same template entity
// Registry::CreateEntity(prefab_id) would clone from), affordable flipped
// against player's current CurrencyComponent::meseta.
ShopMessage BuildShopMessage(Registry& registry, entt::entity player, const ShopStock& stock,
                              const AffixLibrary& affixes);

} // namespace psr
