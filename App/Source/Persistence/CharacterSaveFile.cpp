#include "Persistence/CharacterSaveFile.h"

#include "ApplicationFilepaths.h"
#include "Components/EquipmentComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/KnownTechniquesComponent.h"
#include "Components/LevelComponent.h"
#include "Components/NameComponent.h"
#include "Engine/ECS/ItemComponent.h"
#include "Engine/ECS/NameIdRegistry.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/Persistence/JsonFile.h"

#include <entt/core/hashed_string.hpp>

#include <filesystem>
#include <utility>

namespace psr {

namespace {

    constexpr int kCharacterSaveSchemaVersion = 1;

    std::string ReadStringField(const rapidjson::Value& object, const char* key, const std::string& fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsString())
            throw CharacterSaveError(std::string("character save: '") + key + "' must be a string");
        return it->value.GetString();
    }

    int ReadIntField(const rapidjson::Value& object, const char* key, int fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsInt())
            throw CharacterSaveError(std::string("character save: '") + key + "' must be an integer");
        return it->value.GetInt();
    }

    template <typename E> E ReadEnumField(const rapidjson::Value& object, const char* key, E fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsString())
            throw CharacterSaveError(std::string("character save: '") + key + "' must be a string");
        const std::string_view name = it->value.GetString();
        for (const auto& [text, value] : EnumNames<E>::kValues)
            if (text == name)
                return value;
        throw CharacterSaveError(std::string("character save: unknown '") + key + "' value '" + std::string(name) +
                                 "'");
    }

    SavedItem ReadSavedItem(const rapidjson::Value& entry)
    {
        SavedItem item;
        item.prefab_id_string = ReadStringField(entry, "prefab_id", "");
        item.quantity = ReadIntField(entry, "quantity", 1);
        item.components.SetObject();
        if (auto components = entry.FindMember("components");
            components != entry.MemberEnd() && components->value.IsObject())
            item.components.CopyFrom(components->value, item.components.GetAllocator());
        return item;
    }

    std::optional<SavedItem> ReadOptionalSavedItem(const rapidjson::Value& object, const char* key)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd() || it->value.IsNull())
            return std::nullopt;
        if (!it->value.IsObject())
            throw CharacterSaveError(std::string("character save: '") + key + "' must be an object or null");
        return ReadSavedItem(it->value);
    }

    std::uint32_t ReadDungeonIdEntry(const rapidjson::Value& entry)
    {
        if (entry.IsString())
        {
            const std::uint32_t hash = entt::hashed_string::value(entry.GetString());
            NameIdRegistry::Register(hash, entry.GetString());
            return hash;
        }
        if (entry.IsUint())
            return entry.GetUint();
        throw CharacterSaveError("character save: 'completed_dungeon_ids' entries must be strings or numbers");
    }

    rapidjson::Value BuildSavedItemJson(const Registry& registry, entt::entity item, const EntitySchemaModel& schema,
                                       rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value item_json(rapidjson::kObjectType);

        std::string prefab_id_string;
        if (const PrefabIdComponent* prefab_id = registry.TryGetComponent<PrefabIdComponent>(item))
            if (std::optional<std::string> label = NameIdRegistry::Find(prefab_id->value))
                prefab_id_string = *label;
        item_json.AddMember("prefab_id", rapidjson::Value(prefab_id_string.c_str(), allocator), allocator);

        const ItemComponent* item_component = registry.TryGetComponent<ItemComponent>(item);
        item_json.AddMember("quantity", item_component ? item_component->quantity : 1, allocator);

        item_json.AddMember("components", registry.SerializeEntityComponents(item, schema, allocator), allocator);
        return item_json;
    }

    rapidjson::Value BuildOptionalItemJson(const Registry& registry, entt::entity item, const EntitySchemaModel& schema,
                                          rapidjson::Document::AllocatorType& allocator)
    {
        if (item == entt::null || !registry.IsValid(item))
            return rapidjson::Value(rapidjson::kNullType);
        return BuildSavedItemJson(registry, item, schema, allocator);
    }

} // namespace

bool SaveSlotOccupied(int slot) { return std::filesystem::exists(ApplicationFilepaths::SaveSlotPath(slot)); }

std::optional<int> FindFirstEmptySaveSlot()
{
    for (int slot = 0; slot < kMaxCharacterSaveSlots; ++slot)
        if (!SaveSlotOccupied(slot))
            return slot;
    return std::nullopt;
}

std::optional<LoadedCharacterData> LoadCharacterSaveData(int slot)
{
    const std::filesystem::path path = ApplicationFilepaths::SaveSlotPath(slot);
    if (!std::filesystem::exists(path))
        return std::nullopt;

    rapidjson::Document document = ReadJsonFile(path, kCharacterSaveSchemaVersion);

    LoadedCharacterData data;
    data.name = ReadStringField(document, "name", "");
    data.level = ReadIntField(document, "level", 1);
    data.xp = ReadIntField(document, "xp", 0);
    data.total_xp = ReadIntField(document, "total_xp", 0);

    if (auto known = document.FindMember("known_techniques"); known != document.MemberEnd() && known->value.IsArray())
        for (const auto& entry : known->value.GetArray())
            data.known_techniques.push_back(
                SavedKnownTechnique{ReadStringField(entry, "technique_id", ""), ReadIntField(entry, "tier", 1)});

    data.player_components.SetObject();
    if (auto components = document.FindMember("components");
        components != document.MemberEnd() && components->value.IsObject())
        data.player_components.CopyFrom(components->value, data.player_components.GetAllocator());

    if (auto inventory = document.FindMember("inventory"); inventory != document.MemberEnd() && inventory->value.IsArray())
        for (const auto& entry : inventory->value.GetArray())
            data.inventory.push_back(ReadSavedItem(entry));

    if (auto equipment = document.FindMember("equipment");
        equipment != document.MemberEnd() && equipment->value.IsObject())
    {
        data.equipment.weapon = ReadOptionalSavedItem(equipment->value, "weapon");
        data.equipment.head = ReadOptionalSavedItem(equipment->value, "head");
        data.equipment.torso = ReadOptionalSavedItem(equipment->value, "torso");
        data.equipment.hands = ReadOptionalSavedItem(equipment->value, "hands");
        data.equipment.legs = ReadOptionalSavedItem(equipment->value, "legs");
        data.equipment.mag = ReadOptionalSavedItem(equipment->value, "mag");
    }

    if (auto dungeons = document.FindMember("completed_dungeon_ids");
        dungeons != document.MemberEnd() && dungeons->value.IsArray())
        for (const auto& entry : dungeons->value.GetArray())
            data.completed_dungeon_ids.insert(ReadDungeonIdEntry(entry));

    return data;
}

ClassId ReadSavedClassId(const LoadedCharacterData& data)
{
    auto class_member = data.player_components.FindMember("class");
    if (class_member == data.player_components.MemberEnd())
        return ClassId::Hunter;
    return ReadEnumField<ClassId>(class_member->value, "class_id", ClassId::Hunter);
}

SectionId ReadSavedSectionId(const LoadedCharacterData& data)
{
    auto section_member = data.player_components.FindMember("section_id");
    if (section_member == data.player_components.MemberEnd())
        return SectionId::Viridia;
    return ReadEnumField<SectionId>(section_member->value, "section_id", SectionId::Viridia);
}

std::optional<CharacterSaveSummary> ReadCharacterSaveSummary(int slot)
{
    std::optional<LoadedCharacterData> data = LoadCharacterSaveData(slot);
    if (!data)
        return std::nullopt;

    CharacterSaveSummary summary;
    summary.name = data->name;
    summary.level = data->level;
    summary.class_id = ReadSavedClassId(*data);
    summary.section_id = ReadSavedSectionId(*data);

    if (auto currency_member = data->player_components.FindMember("currency");
        currency_member != data->player_components.MemberEnd())
        summary.meseta = ReadIntField(currency_member->value, "meseta", 0);

    return summary;
}

void SaveCharacter(int slot, const Registry& registry, entt::entity player, const EntitySchemaModel& schema,
                   const RunProgress& run_progress)
{
    rapidjson::Document document;
    document.SetObject();
    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

    document.AddMember("schema_version", kCharacterSaveSchemaVersion, allocator);

    const NameComponent* name = registry.TryGetComponent<NameComponent>(player);
    document.AddMember("name", rapidjson::Value((name ? name->value : std::string{}).c_str(), allocator), allocator);

    const LevelComponent* level = registry.TryGetComponent<LevelComponent>(player);
    document.AddMember("level", level ? level->level : 1, allocator);
    document.AddMember("xp", level ? level->xp : 0, allocator);
    document.AddMember("total_xp", level ? level->total_xp : 0, allocator);

    rapidjson::Value known_json(rapidjson::kArrayType);
    if (const KnownTechniquesComponent* known = registry.TryGetComponent<KnownTechniquesComponent>(player))
    {
        for (const KnownTechniqueEntry& entry : known->known)
        {
            std::string id_string;
            if (std::optional<std::string> label = NameIdRegistry::Find(entry.technique_id))
                id_string = *label;
            rapidjson::Value entry_json(rapidjson::kObjectType);
            entry_json.AddMember("technique_id", rapidjson::Value(id_string.c_str(), allocator), allocator);
            entry_json.AddMember("tier", entry.tier, allocator);
            known_json.PushBack(entry_json, allocator);
        }
    }
    document.AddMember("known_techniques", known_json, allocator);

    document.AddMember("components", registry.SerializeEntityComponents(player, schema, allocator), allocator);

    rapidjson::Value inventory_json(rapidjson::kArrayType);
    if (const InventoryComponent* inventory = registry.TryGetComponent<InventoryComponent>(player))
        for (entt::entity item : inventory->items)
            inventory_json.PushBack(BuildSavedItemJson(registry, item, schema, allocator), allocator);
    document.AddMember("inventory", inventory_json, allocator);

    rapidjson::Value equipment_json(rapidjson::kObjectType);
    if (const EquipmentComponent* equipment = registry.TryGetComponent<EquipmentComponent>(player))
    {
        equipment_json.AddMember("weapon", BuildOptionalItemJson(registry, equipment->weapon, schema, allocator),
                                 allocator);
        equipment_json.AddMember("head", BuildOptionalItemJson(registry, equipment->head, schema, allocator),
                                 allocator);
        equipment_json.AddMember("torso", BuildOptionalItemJson(registry, equipment->torso, schema, allocator),
                                 allocator);
        equipment_json.AddMember("hands", BuildOptionalItemJson(registry, equipment->hands, schema, allocator),
                                 allocator);
        equipment_json.AddMember("legs", BuildOptionalItemJson(registry, equipment->legs, schema, allocator),
                                 allocator);
        equipment_json.AddMember("mag", BuildOptionalItemJson(registry, equipment->mag, schema, allocator), allocator);
    }
    document.AddMember("equipment", equipment_json, allocator);

    rapidjson::Value dungeons_json(rapidjson::kArrayType);
    for (std::uint32_t id : run_progress.completed_dungeon_ids)
    {
        if (std::optional<std::string> label = NameIdRegistry::Find(id))
            dungeons_json.PushBack(rapidjson::Value(label->c_str(), allocator), allocator);
        else
            dungeons_json.PushBack(rapidjson::Value(id), allocator);
    }
    document.AddMember("completed_dungeon_ids", dungeons_json, allocator);

    WriteJsonFile(ApplicationFilepaths::SaveSlotPath(slot), document);
}

entt::entity InstantiateSavedItem(Registry& registry, const SavedItem& saved)
{
    const std::uint32_t prefab_id = entt::hashed_string::value(saved.prefab_id_string.c_str());
    entt::entity item = registry.CreateEntity(prefab_id);
    registry.ApplyEntityComponentsJson(item, saved.components);
    if (ItemComponent* item_component = registry.TryGetComponent<ItemComponent>(item))
        item_component->quantity = saved.quantity;
    return item;
}

} // namespace psr
