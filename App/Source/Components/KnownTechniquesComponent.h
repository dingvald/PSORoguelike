#pragma once

#include <cstdint>
#include <vector>

namespace psr {

// One learned Technique: technique_id (NameId into TechniqueLibrary) plus the
// highest tier taught so far -- see Items/TechniqueLearning.h's LearnTechnique.
struct KnownTechniqueEntry
{
    std::uint32_t technique_id = 0;
    int tier = 1;
};

// Which Techniques an actor has learned by consuming a teach_technique
// ConsumableComponent item (see UseItemAction) -- replaces the old
// weapon-granted model (WeaponComponent no longer carries technique_ids).
// Runtime-only player state, same "deliberately not meta-registered"
// precedent as EquipmentComponent/InventoryComponent (std::vector<...> has no
// FieldKind mapping, and this is never hand-authored in a prefab). Populated
// via GetOrEmplace by LearnTechnique; never emplaced upfront in
// GameplayLayer::LoadNewGame, since nothing is known at spawn.
struct KnownTechniquesComponent
{
    std::vector<KnownTechniqueEntry> known;
};

} // namespace psr
