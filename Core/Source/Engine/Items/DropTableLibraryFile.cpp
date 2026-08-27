#include "Engine/Items/DropTableLibraryFile.h"

#include "Engine/ECS/NameIdRegistry.h"
#include "Engine/Items/DropTableError.h"
#include "Engine/Items/DropTableSchema.h"
#include "Engine/Items/DropTableSchemaEmitter.h"
#include "Engine/Persistence/JsonDirectoryLoader.h"
#include "Engine/Persistence/JsonFile.h"

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

    bool ReadBool(const rapidjson::Value& object, const char* key, bool fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsBool())
            throw DropTableError(std::string("drop table file: '") + key + "' must be a boolean");
        return it->value.GetBool();
    }

    // Mirrors PhotonArtLibraryFile.cpp's ReadNameId: a hashed-string id
    // authored as either a JSON number or a name string, registering the
    // source string into NameIdRegistry so WriteDropTableBody can recover a
    // label later.
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

    template <typename E>
    E ReadEnum(const rapidjson::Value& object, const char* key, E fallback, const char* error_label)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsString())
            throw DropTableError(std::string("drop table file: '") + key + "' must be a " + error_label + " name");
        const std::string_view name = it->value.GetString();
        for (const auto& [text, value] : EnumNames<E>::kValues)
            if (text == name)
                return value;
        throw DropTableError(std::string("drop table file: unknown ") + error_label + " '" + std::string(name) + "'");
    }

    template <typename E> std::string_view EnumName(E value)
    {
        for (const auto& [text, candidate] : EnumNames<E>::kValues)
            if (candidate == value)
                return text;
        return EnumNames<E>::kValues.front().first; // unreachable for a valid enum value
    }

    rapidjson::Value StringValue(const std::string& text, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value value;
        value.SetString(text.c_str(), static_cast<rapidjson::SizeType>(text.size()), allocator);
        return value;
    }

    DropEntry ReadDropEntry(const rapidjson::Value& entry)
    {
        if (!entry.IsObject())
            throw DropTableError("drop table file: each entry must be an object");
        DropEntry drop_entry;
        drop_entry.item_prefab_id = ReadNameId(entry, "item_prefab_id", drop_entry.item_prefab_id);
        drop_entry.weight = ReadInt(entry, "weight", drop_entry.weight);
        drop_entry.favored_section_id =
            ReadEnum<SectionId>(entry, "favored_section_id", drop_entry.favored_section_id, "section id");
        return drop_entry;
    }

    std::vector<DropEntry> ReadDropEntries(const rapidjson::Value& drop_table_def, const char* key)
    {
        std::vector<DropEntry> entries;
        auto it = drop_table_def.FindMember(key);
        if (it == drop_table_def.MemberEnd())
            return entries;
        if (!it->value.IsArray())
            throw DropTableError(std::string("drop table file: '") + key + "' must be an array");
        for (const auto& entry : it->value.GetArray())
            entries.push_back(ReadDropEntry(entry));
        return entries;
    }

    rapidjson::Value WriteDropEntry(const DropEntry& drop_entry, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value object(rapidjson::kObjectType);
        if (std::optional<std::string> label = NameIdRegistry::Find(drop_entry.item_prefab_id))
            object.AddMember("item_prefab_id", StringValue(*label, allocator), allocator);
        else
            object.AddMember("item_prefab_id", drop_entry.item_prefab_id, allocator);
        object.AddMember("weight", drop_entry.weight, allocator);
        object.AddMember("favored_section_id", StringValue(std::string{EnumName(drop_entry.favored_section_id)}, allocator),
                         allocator);
        return object;
    }

    rapidjson::Value WriteDropEntries(const std::vector<DropEntry>& entries, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value array(rapidjson::kArrayType);
        for (const DropEntry& entry : entries)
            array.PushBack(WriteDropEntry(entry, allocator), allocator);
        return array;
    }

} // namespace

DropTable ReadDropTableBody(const rapidjson::Value& drop_table_def)
{
    DropTable drop_table;
    drop_table.name = ReadString(drop_table_def, "name", drop_table.name);
    drop_table.common_entries = ReadDropEntries(drop_table_def, "common_entries");
    drop_table.rare_entries = ReadDropEntries(drop_table_def, "rare_entries");
    drop_table.rare_chance_percent = ReadInt(drop_table_def, "rare_chance_percent", drop_table.rare_chance_percent);
    drop_table.boss_guaranteed_rare = ReadBool(drop_table_def, "boss_guaranteed_rare", drop_table.boss_guaranteed_rare);
    drop_table.meseta_min = ReadInt(drop_table_def, "meseta_min", drop_table.meseta_min);
    drop_table.meseta_max = ReadInt(drop_table_def, "meseta_max", drop_table.meseta_max);
    return drop_table;
}

rapidjson::Value WriteDropTableBody(const DropTable& drop_table, rapidjson::Document::AllocatorType& allocator)
{
    rapidjson::Value object(rapidjson::kObjectType);
    object.AddMember("name", StringValue(drop_table.name, allocator), allocator);
    object.AddMember("common_entries", WriteDropEntries(drop_table.common_entries, allocator), allocator);
    object.AddMember("rare_entries", WriteDropEntries(drop_table.rare_entries, allocator), allocator);
    object.AddMember("rare_chance_percent", drop_table.rare_chance_percent, allocator);
    object.AddMember("boss_guaranteed_rare", drop_table.boss_guaranteed_rare, allocator);
    object.AddMember("meseta_min", drop_table.meseta_min, allocator);
    object.AddMember("meseta_max", drop_table.meseta_max, allocator);
    return object;
}

DropTableLibrary LoadDropTableLibrary(const std::filesystem::path& directory)
{
    const std::vector<JsonDirectoryEntry> entries = LoadJsonDirectory(directory, kDropTableLibraryVersion);
    const DropTableSchemaModel schema = BuildDropTableSchemaModel();

    std::vector<DropTable> drop_tables;
    for (const JsonDirectoryEntry& entry : entries)
    {
        try
        {
            ValidateDropTableDocument(entry.document, schema);

            if (!entry.document.IsObject())
                throw DropTableError("drop table file: must be an object");

            DropTable drop_table = ReadDropTableBody(entry.document);
            drop_table.id_string = entry.id;
            drop_table.id = entt::hashed_string::value(drop_table.id_string.c_str());
            if (drop_table.name.empty())
                drop_table.name = drop_table.id_string;

            drop_tables.push_back(std::move(drop_table));
        }
        catch (const DropTableError& error)
        {
            throw DropTableError(entry.path.string() + ": " + error.what());
        }
    }

    return DropTableLibrary{std::move(drop_tables)};
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
