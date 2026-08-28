#include "Engine/Combat/TechniqueLibraryFile.h"

#include "Engine/Combat/TechniqueError.h"
#include "Engine/Combat/TechniqueSchema.h"
#include "Engine/Combat/TechniqueSchemaEmitter.h"
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
            throw TechniqueError(std::string("technique file: '") + key + "' must be a string");
        return it->value.GetString();
    }

    int ReadInt(const rapidjson::Value& object, const char* key, int fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsInt())
            throw TechniqueError(std::string("technique file: '") + key + "' must be an integer");
        return it->value.GetInt();
    }

    float ReadFloat(const rapidjson::Value& object, const char* key, float fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsNumber())
            throw TechniqueError(std::string("technique file: '") + key + "' must be a number");
        return it->value.GetFloat();
    }

    // Mirrors PieceLibraryFile.cpp/DungeonLibraryFile.cpp's ReadNameId: a
    // hashed-string id authored as either a JSON number or a name string,
    // registering the source string into NameIdRegistry so WriteTechniqueBody
    // can recover a label later.
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
        throw TechniqueError(std::string("technique file: '") + key + "' must be a name string or id");
    }

    template <typename E>
    E ReadEnum(const rapidjson::Value& object, const char* key, E fallback, const char* error_label)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsString())
            throw TechniqueError(std::string("technique file: '") + key + "' must be a " + error_label + " name");
        const std::string_view name = it->value.GetString();
        for (const auto& [text, value] : EnumNames<E>::kValues)
            if (text == name)
                return value;
        throw TechniqueError(std::string("technique file: unknown ") + error_label + " '" + std::string(name) + "'");
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

    TechniqueTier ReadTier(const rapidjson::Value& entry)
    {
        if (!entry.IsObject())
            throw TechniqueError("technique file: each tier must be an object");
        TechniqueTier tier;
        tier.tier = ReadInt(entry, "tier", tier.tier);
        tier.power_multiplier = ReadFloat(entry, "power_multiplier", tier.power_multiplier);
        return tier;
    }

    std::vector<TechniqueTier> ReadTiers(const rapidjson::Value& technique_def)
    {
        std::vector<TechniqueTier> tiers;
        auto it = technique_def.FindMember("tiers");
        if (it == technique_def.MemberEnd())
            return tiers;
        if (!it->value.IsArray())
            throw TechniqueError("technique file: 'tiers' must be an array");
        for (const auto& entry : it->value.GetArray())
            tiers.push_back(ReadTier(entry));
        return tiers;
    }

    rapidjson::Value WriteTier(const TechniqueTier& tier, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value object(rapidjson::kObjectType);
        object.AddMember("tier", tier.tier, allocator);
        object.AddMember("power_multiplier", tier.power_multiplier, allocator);
        return object;
    }

    rapidjson::Value WriteTiers(const std::vector<TechniqueTier>& tiers, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value array(rapidjson::kArrayType);
        for (const TechniqueTier& tier : tiers)
            array.PushBack(WriteTier(tier, allocator), allocator);
        return array;
    }

} // namespace

Technique ReadTechniqueBody(const rapidjson::Value& technique_def)
{
    Technique technique;
    technique.name = ReadString(technique_def, "name", technique.name);
    technique.tp_cost = ReadInt(technique_def, "tp_cost", technique.tp_cost);
    technique.element = ReadEnum<Element>(technique_def, "element", technique.element, "element");
    technique.targeting_mode =
        ReadEnum<TargetingMode>(technique_def, "targeting_mode", technique.targeting_mode, "targeting mode");
    technique.range_shape =
        ReadEnum<WeaponRangeShape>(technique_def, "range_shape", technique.range_shape, "range shape");
    technique.range = ReadInt(technique_def, "range", technique.range);
    technique.effect_family =
        ReadEnum<EffectFamily>(technique_def, "effect_family", technique.effect_family, "effect family");
    technique.status_effect_id = ReadNameId(technique_def, "status_effect_id", technique.status_effect_id);
    technique.status_chance_percent = ReadInt(technique_def, "status_chance_percent", technique.status_chance_percent);
    technique.tiers = ReadTiers(technique_def);
    return technique;
}

rapidjson::Value WriteTechniqueBody(const Technique& technique, rapidjson::Document::AllocatorType& allocator)
{
    rapidjson::Value object(rapidjson::kObjectType);
    object.AddMember("name", StringValue(technique.name, allocator), allocator);
    object.AddMember("tp_cost", technique.tp_cost, allocator);
    object.AddMember("element", StringValue(std::string{EnumName(technique.element)}, allocator), allocator);
    object.AddMember("targeting_mode", StringValue(std::string{EnumName(technique.targeting_mode)}, allocator),
                     allocator);
    object.AddMember("range_shape", StringValue(std::string{EnumName(technique.range_shape)}, allocator), allocator);
    object.AddMember("range", technique.range, allocator);
    object.AddMember("effect_family", StringValue(std::string{EnumName(technique.effect_family)}, allocator),
                     allocator);
    if (std::optional<std::string> label = NameIdRegistry::Find(technique.status_effect_id))
        object.AddMember("status_effect_id", StringValue(*label, allocator), allocator);
    else
        object.AddMember("status_effect_id", technique.status_effect_id, allocator);
    object.AddMember("status_chance_percent", technique.status_chance_percent, allocator);
    object.AddMember("tiers", WriteTiers(technique.tiers, allocator), allocator);
    return object;
}

TechniqueLibrary LoadTechniqueLibrary(const std::filesystem::path& directory)
{
    const std::vector<JsonDirectoryEntry> entries = LoadJsonDirectory(directory, kTechniqueLibraryVersion);
    const TechniqueSchemaModel schema = BuildTechniqueSchemaModel();

    std::vector<Technique> techniques;
    for (const JsonDirectoryEntry& entry : entries)
    {
        try
        {
            ValidateTechniqueDocument(entry.document, schema);

            if (!entry.document.IsObject())
                throw TechniqueError("technique file: must be an object");

            Technique technique = ReadTechniqueBody(entry.document);
            technique.id_string = entry.id;
            technique.id = entt::hashed_string::value(technique.id_string.c_str());
            if (technique.name.empty())
                technique.name = technique.id_string;

            techniques.push_back(std::move(technique));
        }
        catch (const TechniqueError& error)
        {
            throw TechniqueError(entry.path.string() + ": " + error.what());
        }
    }

    return TechniqueLibrary{std::move(techniques)};
}

void SaveTechnique(const std::filesystem::path& path, const Technique& technique)
{
    rapidjson::Document document;
    document.SetObject();
    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

    document.AddMember("schema_version", kTechniqueLibraryVersion, allocator);
    rapidjson::Value body = WriteTechniqueBody(technique, allocator);

    // Round-trip through ReadTechniqueBody so a save can never produce a file
    // LoadTechniqueLibrary would reject on content grounds.
    ReadTechniqueBody(body);

    for (auto member = body.MemberBegin(); member != body.MemberEnd(); ++member)
        document.AddMember(member->name, member->value, allocator);

    ValidateTechniqueDocument(document, BuildTechniqueSchemaModel());

    WriteJsonFile(path, document);
}

} // namespace psr
