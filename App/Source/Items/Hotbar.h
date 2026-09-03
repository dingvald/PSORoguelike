#pragma once

#include "Components/HotbarComponent.h"
#include "Engine/ECS/Entity.h"

#include <cstdint>

namespace psr {

// Binds actor's InventoryComponent::items[inventory_index] into their Item
// hotbar slot hotbar_slot, replacing whatever was previously bound there --
// bound by the item's PrefabIdComponent NameId, same "prefab id, not
// inventory index" style GameplayLayer::TryActivateSlot's Item case already
// resolves at activation time (an index would go stale as the inventory
// reshuffles). A missing InventoryComponent/HotbarComponent, an
// out-of-range inventory_index/hotbar_slot, or an item without
// ConsumableComponent (nothing else resolves through an Item hotbar slot)
// is a no-op. Free/instant, same reasoning as EquipItem/UnequipSlot.
// Returns whether anything changed.
bool AssignItemToHotbarSlot(Entity actor, int inventory_index, int hotbar_slot);

// Binds id (a Technique or Photon Art NameId) into actor's Technique/PhotonArt
// hotbar slot hotbar_slot -- published from the Techniques/Photon Arts screen
// (see TechniquesScreenSlotAssignedMessage), same "id is the key, not an
// index" shape that screen's rows already have (see
// TechniquesScreenMessage.h's own doc comment). Validates server-side rather
// than trusting the message's content: type must be Technique (and actor's
// KnownTechniquesComponent must actually contain id) or PhotonArt (and the
// equipped weapon's WeaponComponent::photon_art_ids must contain id) --
// anything else (Item/Empty, or an id the actor doesn't know/isn't granted)
// is a no-op. Free/instant, same reasoning as AssignItemToHotbarSlot. Returns
// whether anything changed.
bool AssignAbilityToHotbarSlot(Entity actor, HotbarSlotType type, std::uint32_t id, int hotbar_slot);

} // namespace psr
