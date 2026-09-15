#include "Progression/ClassDefinitionFile.h"

#include "Engine/ECS/TypeReflection.h"
#include "Engine/Persistence/JsonFile.h"

#include <string>

namespace psr {

namespace {

    int ReadInt(const rapidjson::Value& object, const char* key, int fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsInt())
            throw JsonFileError(std::string("class file: '") + key + "' must be an integer");
        return it->value.GetInt();
    }

    std::string ReadString(const rapidjson::Value& object, const char* key, const std::string& fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsString())
            throw JsonFileError(std::string("class file: '") + key + "' must be a string");
        return it->value.GetString();
    }

    std::vector<std::string> ReadStringArray(const rapidjson::Value& object, const char* key)
    {
        std::vector<std::string> result;
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return result;
        if (!it->value.IsArray())
            throw JsonFileError(std::string("class file: '") + key + "' must be an array");
        for (const rapidjson::Value& entry : it->value.GetArray())
        {
            if (!entry.IsString())
                throw JsonFileError(std::string("class file: '") + key + "' entries must be strings");
            result.emplace_back(entry.GetString());
        }
        return result;
    }

    std::vector<StartingInventoryEntry> ReadStartingInventory(const rapidjson::Value& object)
    {
        std::vector<StartingInventoryEntry> result;
        auto it = object.FindMember("starting_inventory");
        if (it == object.MemberEnd())
            return result;
        if (!it->value.IsArray())
            throw JsonFileError("class file: 'starting_inventory' must be an array");
        for (const rapidjson::Value& entry : it->value.GetArray())
        {
            if (!entry.IsObject())
                throw JsonFileError("class file: 'starting_inventory' entries must be objects");
            StartingInventoryEntry parsed;
            parsed.item_prefab_id = ReadString(entry, "item_prefab_id", "");
            if (parsed.item_prefab_id.empty())
                throw JsonFileError("class file: 'starting_inventory' entry missing 'item_prefab_id'");
            parsed.quantity = ReadInt(entry, "quantity", 1);
            result.push_back(std::move(parsed));
        }
        return result;
    }

    ClassId ReadClassId(const rapidjson::Value& object)
    {
        auto it = object.FindMember("class_id");
        if (it == object.MemberEnd() || !it->value.IsString())
            throw JsonFileError("class file: 'class_id' must be a string");
        const std::string_view name = it->value.GetString();
        for (const auto& [text, value] : EnumNames<ClassId>::kValues)
            if (text == name)
                return value;
        throw JsonFileError(std::string("class file: unknown class_id '") + std::string(name) + "'");
    }

} // namespace

ClassDefinition LoadClassDefinition(const std::filesystem::path& path, ClassId expected)
{
    const rapidjson::Document document = ReadJsonFile(path, kClassDefinitionVersion);
    if (!document.IsObject())
        throw JsonFileError("class file: must be an object");

    ClassDefinition definition;
    definition.class_id = ReadClassId(document);
    if (definition.class_id != expected)
        throw JsonFileError("class file '" + path.string() + "': class_id does not match the expected class");

    definition.name = ReadString(document, "name", definition.name);
    definition.starting_weapon_prefab_id = ReadString(document, "starting_weapon_prefab_id", "");
    definition.starting_technique_id_strings = ReadStringArray(document, "starting_technique_id_strings");
    definition.starting_armor_prefab_id = ReadString(document, "starting_armor_prefab_id", "");
    definition.starting_inventory = ReadStartingInventory(document);
    definition.base_hp = ReadInt(document, "base_hp", definition.base_hp);
    definition.base_tp = ReadInt(document, "base_tp", definition.base_tp);

    auto curves_member = document.FindMember("curves");
    if (curves_member == document.MemberEnd())
        throw JsonFileError("class file: 'curves' must be an object");
    definition.growth = ParseGrowthCurve(curves_member->value);

    return definition;
}

} // namespace psr
