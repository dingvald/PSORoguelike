#include "Areas/AreaLibraryFile.h"

#include "Areas/AreaError.h"
#include "Areas/AreaSchema.h"
#include "Areas/AreaSchemaEmitter.h"
#include "Engine/ECS/NameIdRegistry.h"
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
            throw AreaError(std::string("area file: '") + key + "' must be a string");
        return it->value.GetString();
    }

    // Mirrors DungeonLibraryFile.cpp's ReadNameId: a hashed-string id
    // authored as either a JSON number or a name string, registering the
    // source string into NameIdRegistry so WriteAreaBody can recover a
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
        throw AreaError(std::string("area file: '") + key + "' must be a name string or id");
    }

    HazardType ReadHazard(const rapidjson::Value& object, const char* key, HazardType fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsString())
            throw AreaError(std::string("area file: '") + key + "' must be a hazard name");
        const std::string_view name = it->value.GetString();
        for (const auto& [text, value] : EnumNames<HazardType>::kValues)
            if (text == name)
                return value;
        throw AreaError(std::string("area file: unknown hazard '") + std::string(name) + "'");
    }

    rapidjson::Value StringValue(const std::string& text, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value value;
        value.SetString(text.c_str(), static_cast<rapidjson::SizeType>(text.size()), allocator);
        return value;
    }

    std::vector<std::string> ReadStringArray(const rapidjson::Value& object, const char* key)
    {
        std::vector<std::string> result;
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return result;
        if (!it->value.IsArray())
            throw AreaError(std::string("area file: '") + key + "' must be an array");
        for (const auto& entry : it->value.GetArray())
        {
            if (!entry.IsString())
                throw AreaError(std::string("area file: '") + key + "' entries must be strings");
            result.emplace_back(entry.GetString());
        }
        return result;
    }

    rapidjson::Value WriteStringArray(const std::vector<std::string>& values,
                                      rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value array(rapidjson::kArrayType);
        for (const std::string& value : values)
            array.PushBack(StringValue(value, allocator), allocator);
        return array;
    }

    rapidjson::Value NameIdValue(std::uint32_t id, rapidjson::Document::AllocatorType& allocator)
    {
        if (std::optional<std::string> label = NameIdRegistry::Find(id))
            return StringValue(*label, allocator);
        rapidjson::Value value;
        value.SetUint(id);
        return value;
    }

    std::string_view HazardName(HazardType hazard)
    {
        for (const auto& [text, candidate] : EnumNames<HazardType>::kValues)
            if (candidate == hazard)
                return text;
        return EnumNames<HazardType>::kValues.front().first; // unreachable for a valid enum value
    }

} // namespace

Area ReadAreaBody(const rapidjson::Value& area_def)
{
    Area area;
    area.name = ReadString(area_def, "name", area.name);
    area.tag = ReadString(area_def, "tag", area.tag);
    area.race_id = ReadNameId(area_def, "race_id", area.race_id);
    area.hazard = ReadHazard(area_def, "hazard", area.hazard);
    area.floor_texture_id = ReadNameId(area_def, "floor_texture_id", area.floor_texture_id);
    area.wall_texture_id = ReadNameId(area_def, "wall_texture_id", area.wall_texture_id);
    area.accent_texture_id = ReadNameId(area_def, "accent_texture_id", area.accent_texture_id);
    area.unlock_predecessor_tag = ReadString(area_def, "unlock_predecessor_tag", area.unlock_predecessor_tag);
    area.dungeon_id_strings = ReadStringArray(area_def, "dungeon_id_strings");
    return area;
}

rapidjson::Value WriteAreaBody(const Area& area, rapidjson::Document::AllocatorType& allocator)
{
    rapidjson::Value object(rapidjson::kObjectType);
    object.AddMember("name", StringValue(area.name, allocator), allocator);
    object.AddMember("tag", StringValue(area.tag, allocator), allocator);
    object.AddMember("race_id", NameIdValue(area.race_id, allocator), allocator);
    object.AddMember("hazard", StringValue(std::string{HazardName(area.hazard)}, allocator), allocator);
    object.AddMember("floor_texture_id", NameIdValue(area.floor_texture_id, allocator), allocator);
    object.AddMember("wall_texture_id", NameIdValue(area.wall_texture_id, allocator), allocator);
    object.AddMember("accent_texture_id", NameIdValue(area.accent_texture_id, allocator), allocator);
    object.AddMember("unlock_predecessor_tag", StringValue(area.unlock_predecessor_tag, allocator), allocator);
    object.AddMember("dungeon_id_strings", WriteStringArray(area.dungeon_id_strings, allocator), allocator);
    return object;
}

AreaLibrary LoadAreaLibrary(const std::filesystem::path& directory)
{
    const std::vector<JsonDirectoryEntry> entries = LoadJsonDirectory(directory, kAreaLibraryVersion);
    const AreaSchemaModel schema = BuildAreaSchemaModel();

    std::vector<Area> areas;
    for (const JsonDirectoryEntry& entry : entries)
    {
        try
        {
            ValidateAreaDocument(entry.document, schema);

            if (!entry.document.IsObject())
                throw AreaError("area file: must be an object");

            Area area = ReadAreaBody(entry.document);
            area.id_string = entry.id;
            area.id = entt::hashed_string::value(area.id_string.c_str());
            if (area.name.empty())
                area.name = area.id_string;
            if (area.tag.empty())
                area.tag = area.id_string;

            areas.push_back(std::move(area));
        }
        catch (const AreaError& error)
        {
            throw AreaError(entry.path.string() + ": " + error.what());
        }
    }

    return AreaLibrary{std::move(areas)};
}

void SaveArea(const std::filesystem::path& path, const Area& area)
{
    rapidjson::Document document;
    document.SetObject();
    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

    document.AddMember("schema_version", kAreaLibraryVersion, allocator);
    rapidjson::Value body = WriteAreaBody(area, allocator);

    // Round-trip through ReadAreaBody so a save can never produce a file
    // LoadAreaLibrary would reject on content grounds.
    ReadAreaBody(body);

    for (auto member = body.MemberBegin(); member != body.MemberEnd(); ++member)
        document.AddMember(member->name, member->value, allocator);

    ValidateAreaDocument(document, BuildAreaSchemaModel());

    WriteJsonFile(path, document);
}

} // namespace psr
