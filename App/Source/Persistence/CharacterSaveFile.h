#pragma once

#include "Engine/ECS/Registry.h"
#include "Items/SectionId.h"
#include "Missions/RunProgress.h"
#include "Progression/CharacterClass.h"

#include <entt/entt.hpp>

#include <rapidjson/document.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace psr {

// Thrown for a structurally valid save file whose *content* doesn't match the
// shape this module expects -- one error type per subsystem, mirroring
// JsonFileError/EntityLoaderError/AreaError. A malformed/unreadable file or a
// schema_version mismatch surfaces as JsonFileError from ReadJsonFile instead.
class CharacterSaveError : public std::runtime_error
{
public:
    explicit CharacterSaveError(const std::string& message) : std::runtime_error(message) {}
};

// Fixed slot count for "character save slots" -- each an independent,
// still-living character (see docs/GDD.md's meta-progression note: nothing
// carries over between characters, so slots don't share any state). No
// delete-save UI exists yet (M14.3's own future work per ConfirmAction's doc
// comment), so a full slot set means New Character has nowhere to go until
// one frees up.
inline constexpr int kMaxCharacterSaveSlots = 4;

// Cheap summary for the slot-select screen -- everything it displays, without
// touching inventory/equipment.
struct CharacterSaveSummary
{
    std::string name;
    ClassId class_id = ClassId::Hunter;
    SectionId section_id = SectionId::Viridia;
    int level = 1;
    int meseta = 0;
};

// One saved item instance (an inventory entry or an equipped slot). Its
// prefab_id_string re-creates the item's base template via
// Registry::CreateEntity; quantity (deliberately outside components -- see
// ItemComponent.h -- since it's never authorable) and components (whatever
// Registry::SerializeEntityComponents produced for this item: a dropped
// weapon's rolled grind_level/element/race_bonuses, a mag's fed progress, an
// armor's rolled stats, ...) restore what actually varies per-instance on top
// of that clone -- see InstantiateSavedItem.
struct SavedItem
{
    std::string prefab_id_string;
    int quantity = 1;
    rapidjson::Document components; // owns its own allocator; {} if nothing else needs restoring
};

// The six fixed EquipmentComponent slots -- nullopt means empty.
struct SavedEquipment
{
    std::optional<SavedItem> weapon;
    std::optional<SavedItem> head;
    std::optional<SavedItem> torso;
    std::optional<SavedItem> hands;
    std::optional<SavedItem> legs;
    std::optional<SavedItem> mag;
};

// One learned Technique entry (see KnownTechniquesComponent.h) -- kept
// outside `components` since KnownTechniquesComponent is deliberately not
// schema-registered (std::vector<KnownTechniqueEntry> has no FieldKind
// mapping), same reasoning as Inventory/Equipment/Storage.
struct SavedKnownTechnique
{
    std::string technique_id_string;
    int tier = 1;
};

// Everything GameplayLayer needs to restore a saved character onto a freshly
// spawned player entity -- see GameplayLayer::RestoreCharacterFromSave.
// player_components is exactly what Registry::SerializeEntityComponents wrote
// for the player entity (class/section_id/currency/health/tp/stats/... --
// whichever authorable components the player happens to carry): restoring it
// is a single Registry::ApplyEntityComponentsJson call, so this struct only
// hand-lists what's deliberately outside that generic component snapshot
// (name, level/xp, known Techniques, inventory/equipment, area unlocks).
struct LoadedCharacterData
{
    std::string name;
    int level = 1;
    int xp = 0;
    int total_xp = 0;
    std::vector<SavedKnownTechnique> known_techniques;
    rapidjson::Document player_components;
    std::vector<SavedItem> inventory;
    SavedEquipment equipment;
    std::unordered_set<std::uint32_t> completed_dungeon_ids;
};

// True iff slot (0-indexed, must be < kMaxCharacterSaveSlots) has a save file.
bool SaveSlotOccupied(int slot);

// The first slot in [0, kMaxCharacterSaveSlots) with no save file, or nullopt
// if every slot is occupied.
std::optional<int> FindFirstEmptySaveSlot();

// Reads just enough of slot's save file for the slot-select screen. nullopt if
// the slot is empty. Throws JsonFileError/CharacterSaveError for a malformed
// save (should not happen for a file this module itself wrote).
std::optional<CharacterSaveSummary> ReadCharacterSaveSummary(int slot);

// Reads the whole of slot's save file. nullopt if the slot is empty.
std::optional<LoadedCharacterData> LoadCharacterSaveData(int slot);

// Recovers class_id/section_id from a LoadedCharacterData's player_components
// (as written by Registry::SerializeEntityComponents for ClassComponent/
// SectionIdComponent) -- needed by GameplayLayer's Continue constructor
// before it can even pick a ClassDefinition to load, and by
// ReadCharacterSaveSummary. Fall back to Hunter/Viridia if the component is
// somehow missing (should not happen for a file this module itself wrote).
ClassId ReadSavedClassId(const LoadedCharacterData& data);
SectionId ReadSavedSectionId(const LoadedCharacterData& data);

// Writes slot's whole save file from player's current live state in registry.
// schema (RegisterComponents' returned model) drives the player's own
// component snapshot and every inventory/equipment item's; run_progress
// supplies completed_dungeon_ids. Overwrites whatever was previously in slot.
void SaveCharacter(int slot, const Registry& registry, entt::entity player, const EntitySchemaModel& schema,
                    const RunProgress& run_progress);

// Instantiates one saved item as a live entity: Registry::CreateEntity for
// its prefab_id_string, Registry::ApplyEntityComponentsJson to restore
// whatever varied per-instance, then ItemComponent::quantity directly (see
// SavedItem's own doc comment for why that one field is handled separately).
entt::entity InstantiateSavedItem(Registry& registry, const SavedItem& saved);

} // namespace psr
