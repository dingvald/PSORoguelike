#include "Engine/Dungeon/DungeonLibraryFile.h"

#include "Engine/Dungeon/DungeonError.h"
#include "Engine/Dungeon/DungeonSchema.h"
#include "Engine/Dungeon/DungeonSchemaEmitter.h"
#include "Engine/ECS/NameIdRegistry.h"
#include "Engine/Persistence/JsonDirectoryLoader.h"
#include "Engine/Persistence/JsonFile.h"

#include <entt/core/hashed_string.hpp>

#include <optional>
#include <utility>

namespace psr {

namespace {

    std::string ReadString(const rapidjson::Value& object, const char* key, const std::string& fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsString())
            throw DungeonError(std::string("dungeon file: '") + key + "' must be a string");
        return it->value.GetString();
    }

    int ReadInt(const rapidjson::Value& object, const char* key, int fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsInt())
            throw DungeonError(std::string("dungeon file: '") + key + "' must be an integer");
        return it->value.GetInt();
    }

    float ReadFloat(const rapidjson::Value& object, const char* key, float fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsNumber())
            throw DungeonError(std::string("dungeon file: '") + key + "' must be a number");
        return it->value.GetFloat();
    }

    // Mirrors PieceLibraryFile.cpp's ReadNameId: a hashed-string id authored
    // as either a JSON number or a name string, registering the source
    // string into NameIdRegistry so WritePieceRef can recover a label later.
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
        throw DungeonError(std::string("dungeon file: '") + key + "' must be a name string or id");
    }

    DungeonPieceRef ReadPieceRef(const rapidjson::Value& entry)
    {
        if (!entry.IsObject())
            throw DungeonError("dungeon file: each piece ref must be an object");
        DungeonPieceRef ref;
        ref.piece_id = ReadNameId(entry, "piece_id", 0);
        ref.weight = ReadFloat(entry, "weight", ref.weight);
        ref.max_occurrences = ReadInt(entry, "max_occurrences", ref.max_occurrences);
        return ref;
    }

    std::vector<DungeonPieceRef> ReadPieceRefs(const rapidjson::Value& dungeon_def)
    {
        std::vector<DungeonPieceRef> refs;
        auto it = dungeon_def.FindMember("pieces");
        if (it == dungeon_def.MemberEnd())
            return refs;
        if (!it->value.IsArray())
            throw DungeonError("dungeon file: 'pieces' must be an array");
        for (const auto& entry : it->value.GetArray())
            refs.push_back(ReadPieceRef(entry));
        return refs;
    }

    rapidjson::Value StringValue(const std::string& text, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value value;
        value.SetString(text.c_str(), static_cast<rapidjson::SizeType>(text.size()), allocator);
        return value;
    }

    rapidjson::Value WritePieceRef(const DungeonPieceRef& ref, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value object(rapidjson::kObjectType);
        if (std::optional<std::string> label = NameIdRegistry::Find(ref.piece_id))
            object.AddMember("piece_id", StringValue(*label, allocator), allocator);
        else
            object.AddMember("piece_id", ref.piece_id, allocator);
        object.AddMember("weight", ref.weight, allocator);
        object.AddMember("max_occurrences", ref.max_occurrences, allocator);
        return object;
    }

    rapidjson::Value WritePieceRefs(const std::vector<DungeonPieceRef>& refs,
                                    rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value array(rapidjson::kArrayType);
        for (const DungeonPieceRef& ref : refs)
            array.PushBack(WritePieceRef(ref, allocator), allocator);
        return array;
    }

    // Mirrors PieceLibraryFile.cpp's AddNameIdMember: label from
    // NameIdRegistry when known, falling back to the raw id otherwise.
    void AddNameIdMember(rapidjson::Value& object, const char* key, std::uint32_t id,
                         rapidjson::Document::AllocatorType& allocator)
    {
        if (std::optional<std::string> label = NameIdRegistry::Find(id))
            object.AddMember(rapidjson::StringRef(key), StringValue(*label, allocator), allocator);
        else
            object.AddMember(rapidjson::StringRef(key), id, allocator);
    }

} // namespace

Dungeon ReadDungeonBody(const rapidjson::Value& dungeon_def)
{
    Dungeon dungeon;
    dungeon.name = ReadString(dungeon_def, "name", dungeon.name);
    dungeon.area_tag = ReadString(dungeon_def, "area_tag", dungeon.area_tag);
    dungeon.room_count_min = ReadInt(dungeon_def, "room_count_min", dungeon.room_count_min);
    dungeon.room_count_max = ReadInt(dungeon_def, "room_count_max", dungeon.room_count_max);
    dungeon.loopback_count_min = ReadInt(dungeon_def, "loopback_count_min", dungeon.loopback_count_min);
    dungeon.loopback_count_max = ReadInt(dungeon_def, "loopback_count_max", dungeon.loopback_count_max);
    dungeon.pieces = ReadPieceRefs(dungeon_def);
    dungeon.lock_count = ReadInt(dungeon_def, "lock_count", dungeon.lock_count);
    dungeon.unlocked_door_prefab_id = ReadNameId(dungeon_def, "unlocked_door_prefab_id", 0);
    dungeon.locked_door_prefab_id = ReadNameId(dungeon_def, "locked_door_prefab_id", 0);
    dungeon.switch_prefab_id = ReadNameId(dungeon_def, "switch_prefab_id", 0);

    if (dungeon.room_count_min > dungeon.room_count_max)
        throw DungeonError("dungeon file: 'room_count_min' must be <= 'room_count_max'");
    if (dungeon.loopback_count_min > dungeon.loopback_count_max)
        throw DungeonError("dungeon file: 'loopback_count_min' must be <= 'loopback_count_max'");

    return dungeon;
}

rapidjson::Value WriteDungeonBody(const Dungeon& dungeon, rapidjson::Document::AllocatorType& allocator)
{
    rapidjson::Value object(rapidjson::kObjectType);
    object.AddMember("name", StringValue(dungeon.name, allocator), allocator);
    object.AddMember("area_tag", StringValue(dungeon.area_tag, allocator), allocator);
    object.AddMember("room_count_min", dungeon.room_count_min, allocator);
    object.AddMember("room_count_max", dungeon.room_count_max, allocator);
    object.AddMember("loopback_count_min", dungeon.loopback_count_min, allocator);
    object.AddMember("loopback_count_max", dungeon.loopback_count_max, allocator);
    object.AddMember("pieces", WritePieceRefs(dungeon.pieces, allocator), allocator);
    object.AddMember("lock_count", dungeon.lock_count, allocator);
    AddNameIdMember(object, "unlocked_door_prefab_id", dungeon.unlocked_door_prefab_id, allocator);
    AddNameIdMember(object, "locked_door_prefab_id", dungeon.locked_door_prefab_id, allocator);
    AddNameIdMember(object, "switch_prefab_id", dungeon.switch_prefab_id, allocator);
    return object;
}

DungeonLibrary LoadDungeonLibrary(const std::filesystem::path& directory)
{
    const std::vector<JsonDirectoryEntry> entries = LoadJsonDirectory(directory, kDungeonLibraryVersion);
    const DungeonSchemaModel schema = BuildDungeonSchemaModel();

    std::vector<Dungeon> dungeons;
    for (const JsonDirectoryEntry& entry : entries)
    {
        try
        {
            ValidateDungeonDocument(entry.document, schema);

            if (!entry.document.IsObject())
                throw DungeonError("dungeon file: must be an object");

            Dungeon dungeon = ReadDungeonBody(entry.document);
            dungeon.id_string = entry.id;
            dungeon.id = entt::hashed_string::value(dungeon.id_string.c_str());
            if (dungeon.name.empty())
                dungeon.name = dungeon.id_string;

            dungeons.push_back(std::move(dungeon));
        }
        catch (const DungeonError& error)
        {
            throw DungeonError(entry.path.string() + ": " + error.what());
        }
    }

    return DungeonLibrary{std::move(dungeons)};
}

void SaveDungeon(const std::filesystem::path& path, const Dungeon& dungeon)
{
    rapidjson::Document document;
    document.SetObject();
    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

    document.AddMember("schema_version", kDungeonLibraryVersion, allocator);
    rapidjson::Value body = WriteDungeonBody(dungeon, allocator);

    // Round-trip through ReadDungeonBody so a save can never produce a file
    // LoadDungeonLibrary would reject on content grounds.
    ReadDungeonBody(body);

    for (auto member = body.MemberBegin(); member != body.MemberEnd(); ++member)
        document.AddMember(member->name, member->value, allocator);

    ValidateDungeonDocument(document, BuildDungeonSchemaModel());

    WriteJsonFile(path, document);
}

} // namespace psr
