#pragma once

#include <entt/entt.hpp>

namespace psr {

class Registry;

// Puts mag_entity (the same entity that was just equipped into
// EquipmentComponent::mag -- see Equip.cpp) into the world at a tile
// trailing actor, and adds it to the current Grid (via Registry::GetGrid)
// so TileRenderer draws it. mag_entity never gains BlocksMovementComponent
// or HealthComponent, so it's automatically excluded from every movement-
// blocking, line-of-sight, and targeting check with no special-casing
// elsewhere. A no-op if actor has no Position yet.
void OnMagEquipped(Registry& registry, entt::entity actor, entt::entity mag_entity);

// Removes mag_entity from the Grid and its Position -- the unequip
// counterpart to OnMagEquipped, so TileRenderer stops drawing it once it's
// back in the inventory.
void OnMagUnequipped(Registry& registry, entt::entity mag_entity);

// Called once per frame (GameplayLayer::OnUpdate) regardless of whether
// actor currently has an equipped mag. Repositions the equipped mag one
// tile behind actor -- opposite whatever LastDirectionComponent currently
// holds (defaulting to south if actor has none) -- and advances its idle-
// bob clock (MagComponent::bob_elapsed). A no-op if actor has no
// EquipmentComponent or no mag equipped.
void UpdateMagCompanion(Registry& registry, entt::entity actor, float delta_time);

} // namespace psr
