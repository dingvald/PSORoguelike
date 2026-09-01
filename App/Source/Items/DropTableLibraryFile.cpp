#include "Items/DropTableLibraryFile.h"

#include "Engine/ECS/NameIdRegistry.h"
#include "Engine/Persistence/JsonDirectoryLoader.h"
#include "Engine/Persistence/JsonFile.h"
#include "Items/DropTableError.h"
#include "Items/DropTableSchema.h"
#include "Items/DropTableSchemaEmitter.h"
#include "Items/SectionId.h"

#include <entt/core/hashed_string.hpp>

#include <optional>
#include <string_view>
#include <utility>

namespace psr {

namespace {

    std::string ReadString(const rapidjson::Value& object, const char* key, const std::string& fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsString())
            throw DropTableError(std::string("drop table file: '") + key + "' must be a string");
        return it->value.GetString();
    }

    int ReadInt(const rapidjson::Value& object, const char* key, int fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsInt())
            throw DropTableError(std::string("drop table file: '") + key + "' must be an integer");
        return it->value.GetInt();
    }

    float ReadFloat(const rapidjson::Value& object, const char* key, float fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsNumber())
            throw DropTableError(std::string("drop table file: '") + key + "' must be a number");
        return it->value.GetFloat();
    }

    // Mirrors DungeonLibraryFile.cpp's ReadNameId: a hashed-string id authored
    // as either a JSON number or a name string, registering the source string
    // into NameIdRegistry so WriteEntry/WriteGuaranteedIds can recover a label
    // later.
    std::uint32_t ReadNameId(const rapidjson::Value& object, const char* key, std::uint32_t fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (it->value.IsString())
        {
            const std::uint32_t hash = entt::hashed_string::value(it->value.GetString());
            NameIdRegistry::Register(hash, it->value.GetString());
            return hash;
        }
        if (it->value.IsUint())
            return it->value.GetUint();
        if (it->value.IsInt())
            return static_cast<std::uint32_t>(it->value.GetInt());
        throw DropTableError(std::string("drop table file: '") + key + "' must be a name string or id");
    }

    std::uint32_t ReadNameIdValue(const rapidjson::Value& value)
    {
        if (value.IsString())
        {
            const std::uint32_t hash = entt::hashed_string::value(value.GetString());
            NameIdRegistry::Register(hash, value.GetString());
            return hash;
        }
        if (value.IsUint())
            return value.GetUint();
        if (value.IsInt())
            return static_cast<std::uint32_t>(value.GetInt());
        throw DropTableError("drop table file: guaranteed_item_ids entries must be a name string or id");
    }

    std::array<float, kSectionIdCount> ReadSectionWeights(const rapidjson::Value& entry)
    {
        std::array<float, kSectionIdCount> weights;
        weights.fill(1.0f);

        auto it = entry.FindMember("section_id_weights");
        if (it == entry.MemberEnd())
            return weights;
        if (!it->value.IsObject())
            throw DropTableError("drop table file: 'section_id_weights' must be an object");

        for (auto member = it->value.MemberBegin(); member != it->value.MemberEnd(); ++member)
        {
            const std::string_view key = member->name.GetString();
            bool matched = false;
            for (const auto& [name, section_id] : EnumNames<SectionId>::kValues)
            {
                if (name != key)
                    continue;
                if (!member->value.IsNumber())
                    throw DropTableError("drop table file: section_id_weights values must be numbers");
                weights[static_cast<std::size_t>(section_id)] = member->value.GetFloat();
                matched = true;
                break;
            }
            if (!matched)
                throw DropTableError(std::string("drop table file: unknown section id '") + std::string(key) + "'");
        }

        return weights;
    }

    DropTableEntry ReadEntry(const rapidjson::Value& entry)
    {
        if (!entry.IsObject())
            throw DropTableError("drop table file: each entry must be an object");
        DropTableEntry result;
        result.item_prefab_id = ReadNameId(entry, "item_prefab_id", 0);
        result.weight = ReadFloat(entry, "weight", result.weight);
        result.section_id_weights = ReadSectionWeights(entry);
        return result;
    }

    std::vector<DropTableEntry> ReadEntries(const rapidjson::Value& drop_table_def, const char* key)
    {
        std::vector<DropTableEntry> entries;
        auto it = drop_table_def.FindMember(key);
        if (it == drop_table_def.MemberEnd())
            return entries;
        if (!it->value.IsArray())
            throw DropTableError(std::string("drop table file: '") + key + "' must be an array");
        for (const auto& entry : it->value.GetArray())
            entries.push_back(ReadEntry(entry));
        return entries;
    }

    std::vector<std::uint32_t> ReadGuaranteedIds(const rapidjson::Value& drop_table_def)
    {
        std::vector<std::uint32_t> ids;
        auto it = drop_table_def.FindMember("guaranteed_item_ids");
        if (it == drop_table_def.MemberEnd())
            return ids;
        if (!it->value.IsArray())
            throw DropTableError("drop table file: 'guaranteed_item_ids' must be an array");
        for (const auto& entry : it->value.GetArray())
            ids.push_back(ReadNameIdValue(entry));
        return ids;
    }

    rapidjson::Value StringValue(const std::string& text, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value value;
        value.SetString(text.c_str(), static_cast<rapidjson::SizeType>(text.size()), allocator);
        return value;
    }

    rapidjson::Value WriteNameId(std::uint32_t id, rapidjson::Document::AllocatorType& allocator)
    {
        if (std::optional<std::string> label = NameIdRegistry::Find(id))
            return StringValue(*label, allocator);
        rapidjson::Value value;
        value.SetUint(id);
        return value;
    }

    rapidjson::Value WriteEntry(const DropTableEntry& entry, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value object(rapidjson::kObjectType);
        object.AddMember("item_prefab_id", WriteNameId(entry.item_prefab_id, allocator), allocator);
        object.AddMember("weight", entry.weight, allocator);

        rapidjson::Value weights(rapidjson::kObjectType);
        for (const auto& [name, section_id] : EnumNames<SectionId>::kValues)
        {
            const float multiplier = entry.section_id_weights[static_cast<std::size_t>(section_id)];
            if (multiplier == 1.0f)
                continue; // sparse: omit no-favoritism entries
            weights.AddMember(StringValue(std::string(name), allocator), rapidjson::Value(multiplier), allocator);
        }
        object.AddMember("section_id_weights", weights, allocator);

        return object;
    }

    rapidjson::Value WriteEntries(const std::vector<DropTableEntry>& entries,
                                  rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value array(rapidjson::kArrayType);
        for (const DropTableEntry& entry : entries)
            array.PushBack(WriteEntry(entry, allocator), allocator);
        return array;
    }

    rapidjson::Value WriteGuaranteedIds(const std::vector<std::uint32_t>& ids,
                                        rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value array(rapidjson::kArrayType);
        for (std::uint32_t id : ids)
            array.PushBack(WriteNameId(id, allocator), allocator);
        return array;
    }

} // namespace

DropTable ReadDropTableBody(const rapidjson::Value& drop_table_def)
{
    DropTable drop_table;
    drop_table.name = ReadString(drop_table_def, "name", drop_table.name);
    drop_table.common_entries = ReadEntries(drop_table_def, "common_entries");
    drop_table.rare_entries = ReadEntries(drop_table_def, "rare_entries");
    drop_table.guaranteed_item_ids = ReadGuaranteedIds(drop_table_def);
    drop_table.rare_roll_chance_percent =
        ReadFloat(drop_table_def, "rare_roll_chance_percent", drop_table.rare_roll_chance_percent);
    drop_table.meseta_min = ReadInt(drop_table_def, "meseta_min", drop_table.meseta_min);
    drop_table.meseta_max = ReadInt(drop_table_def, "meseta_max", drop_table.meseta_max);

    if (drop_table.meseta_min > drop_table.meseta_max)
        throw DropTableError("drop table file: 'meseta_min' must be <= 'meseta_max'");

    return drop_table;
}

rapidjson::Value WriteDropTableBody(const DropTable& drop_table, rapidjson::Document::AllocatorType& allocator)
{
    rapidjson::Value object(rapidjson::kObjectType);
    object.AddMember("name", StringValue(drop_table.name, allocator), allocator);
    object.AddMember("common_entries", WriteEntries(drop_table.common_entries, allocator), allocator);
    object.AddMember("rare_entries", WriteEntries(drop_table.rare_entries, allocator), allocator);
    object.AddMember("guaranteed_item_ids", WriteGuaranteedIds(drop_table.guaranteed_item_ids, allocator), allocator);
    object.AddMember("rare_roll_chance_percent", drop_table.rare_roll_chance_percent, allocator);
    object.AddMember("meseta_min", drop_table.meseta_min, allocator);
    object.AddMember("meseta_max", drop_table.meseta_max, allocator);
    return object;
}

DropTableLibrary LoadDropTableLibrary(const std::filesystem::path& directory)
{
    const std::vector<JsonDirectoryEntry> entries = LoadJsonDirectory(directory, kDropTableLibraryVersion);
    const DropTableSchemaModel schema = BuildDropTableSchemaModel();

    std::vector<DropTable> tables;
    for (const JsonDirectoryEntry& entry : entries)
    {
        try
        {
            ValidateDropTableDocument(entry.document, schema);

            if (!entry.document.IsObject())
                throw DropTableError("drop table file: must be an object");

            DropTable table = ReadDropTableBody(entry.document);
            table.id_string = entry.id;
            table.id = entt::hashed_string::value(table.id_string.c_str());
            if (table.name.empty())
                table.name = table.id_string;

            tables.push_back(std::move(table));
        }
        catch (const DropTableError& error)
        {
            throw DropTableError(entry.path.string() + ": " + error.what());
        }
    }

    return DropTableLibrary{std::move(tables)};
}

void SaveDropTable(const std::filesystem::path& path, const DropTable& drop_table)
{
    rapidjson::Document document;
    document.SetObject();
    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

    document.AddMember("schema_version", kDropTableLibraryVersion, allocator);
    rapidjson::Value body = WriteDropTableBody(drop_table, allocator);

    // Round-trip through ReadDropTableBody so a save can never produce a file
    // LoadDropTableLibrary would reject on content grounds.
    ReadDropTableBody(body);

    for (auto member = body.MemberBegin(); member != body.MemberEnd(); ++member)
        document.AddMember(member->name, member->value, allocator);

    ValidateDropTableDocument(document, BuildDropTableSchemaModel());

    WriteJsonFile(path, document);
}

} // namespace psr
