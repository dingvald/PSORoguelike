#include "Engine/Combat/PhotonArtLibraryFile.h"

#include "Engine/Combat/PhotonArtError.h"
#include "Engine/Combat/PhotonArtSchema.h"
#include "Engine/Combat/PhotonArtSchemaEmitter.h"
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
            throw PhotonArtError(std::string("photon art file: '") + key + "' must be a string");
        return it->value.GetString();
    }

    int ReadInt(const rapidjson::Value& object, const char* key, int fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsInt())
            throw PhotonArtError(std::string("photon art file: '") + key + "' must be an integer");
        return it->value.GetInt();
    }

    float ReadFloat(const rapidjson::Value& object, const char* key, float fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsNumber())
            throw PhotonArtError(std::string("photon art file: '") + key + "' must be a number");
        return it->value.GetFloat();
    }

    // Mirrors PieceLibraryFile.cpp/DungeonLibraryFile.cpp's ReadNameId: a
    // hashed-string id authored as either a JSON number or a name string,
    // registering the source string into NameIdRegistry so WritePhotonArtBody
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
        throw PhotonArtError(std::string("photon art file: '") + key + "' must be a name string or id");
    }

    template <typename E>
    E ReadEnum(const rapidjson::Value& object, const char* key, E fallback, const char* error_label)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsString())
            throw PhotonArtError(std::string("photon art file: '") + key + "' must be a " + error_label + " name");
        const std::string_view name = it->value.GetString();
        for (const auto& [text, value] : EnumNames<E>::kValues)
            if (text == name)
                return value;
        throw PhotonArtError(std::string("photon art file: unknown ") + error_label + " '" + std::string(name) + "'");
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

    PhotonArtTier ReadTier(const rapidjson::Value& entry)
    {
        if (!entry.IsObject())
            throw PhotonArtError("photon art file: each tier must be an object");
        PhotonArtTier tier;
        tier.tier = ReadInt(entry, "tier", tier.tier);
        tier.power_multiplier = ReadFloat(entry, "power_multiplier", tier.power_multiplier);
        return tier;
    }

    std::vector<PhotonArtTier> ReadTiers(const rapidjson::Value& photon_art_def)
    {
        std::vector<PhotonArtTier> tiers;
        auto it = photon_art_def.FindMember("tiers");
        if (it == photon_art_def.MemberEnd())
            return tiers;
        if (!it->value.IsArray())
            throw PhotonArtError("photon art file: 'tiers' must be an array");
        for (const auto& entry : it->value.GetArray())
            tiers.push_back(ReadTier(entry));
        return tiers;
    }

    rapidjson::Value WriteTier(const PhotonArtTier& tier, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value object(rapidjson::kObjectType);
        object.AddMember("tier", tier.tier, allocator);
        object.AddMember("power_multiplier", tier.power_multiplier, allocator);
        return object;
    }

    rapidjson::Value WriteTiers(const std::vector<PhotonArtTier>& tiers, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value array(rapidjson::kArrayType);
        for (const PhotonArtTier& tier : tiers)
            array.PushBack(WriteTier(tier, allocator), allocator);
        return array;
    }

} // namespace

PhotonArt ReadPhotonArtBody(const rapidjson::Value& photon_art_def)
{
    PhotonArt photon_art;
    photon_art.name = ReadString(photon_art_def, "name", photon_art.name);
    photon_art.tp_cost = ReadInt(photon_art_def, "tp_cost", photon_art.tp_cost);
    photon_art.targeting_mode =
        ReadEnum<TargetingMode>(photon_art_def, "targeting_mode", photon_art.targeting_mode, "targeting mode");
    photon_art.range_shape =
        ReadEnum<WeaponRangeShape>(photon_art_def, "range_shape", photon_art.range_shape, "range shape");
    photon_art.range = ReadInt(photon_art_def, "range", photon_art.range);
    photon_art.hits_per_turn = ReadInt(photon_art_def, "hits_per_turn", photon_art.hits_per_turn);
    photon_art.effect_family =
        ReadEnum<EffectFamily>(photon_art_def, "effect_family", photon_art.effect_family, "effect family");
    photon_art.drain_percent = ReadInt(photon_art_def, "drain_percent", photon_art.drain_percent);
    photon_art.status_effect_id = ReadNameId(photon_art_def, "status_effect_id", photon_art.status_effect_id);
    photon_art.tiers = ReadTiers(photon_art_def);
    return photon_art;
}

rapidjson::Value WritePhotonArtBody(const PhotonArt& photon_art, rapidjson::Document::AllocatorType& allocator)
{
    rapidjson::Value object(rapidjson::kObjectType);
    object.AddMember("name", StringValue(photon_art.name, allocator), allocator);
    object.AddMember("tp_cost", photon_art.tp_cost, allocator);
    object.AddMember("targeting_mode", StringValue(std::string{EnumName(photon_art.targeting_mode)}, allocator),
                     allocator);
    object.AddMember("range_shape", StringValue(std::string{EnumName(photon_art.range_shape)}, allocator), allocator);
    object.AddMember("range", photon_art.range, allocator);
    object.AddMember("hits_per_turn", photon_art.hits_per_turn, allocator);
    object.AddMember("effect_family", StringValue(std::string{EnumName(photon_art.effect_family)}, allocator),
                     allocator);
    object.AddMember("drain_percent", photon_art.drain_percent, allocator);
    if (std::optional<std::string> label = NameIdRegistry::Find(photon_art.status_effect_id))
        object.AddMember("status_effect_id", StringValue(*label, allocator), allocator);
    else
        object.AddMember("status_effect_id", photon_art.status_effect_id, allocator);
    object.AddMember("tiers", WriteTiers(photon_art.tiers, allocator), allocator);
    return object;
}

PhotonArtLibrary LoadPhotonArtLibrary(const std::filesystem::path& directory)
{
    const std::vector<JsonDirectoryEntry> entries = LoadJsonDirectory(directory, kPhotonArtLibraryVersion);
    const PhotonArtSchemaModel schema = BuildPhotonArtSchemaModel();

    std::vector<PhotonArt> photon_arts;
    for (const JsonDirectoryEntry& entry : entries)
    {
        try
        {
            ValidatePhotonArtDocument(entry.document, schema);

            if (!entry.document.IsObject())
                throw PhotonArtError("photon art file: must be an object");

            PhotonArt photon_art = ReadPhotonArtBody(entry.document);
            photon_art.id_string = entry.id;
            photon_art.id = entt::hashed_string::value(photon_art.id_string.c_str());
            if (photon_art.name.empty())
                photon_art.name = photon_art.id_string;

            photon_arts.push_back(std::move(photon_art));
        }
        catch (const PhotonArtError& error)
        {
            throw PhotonArtError(entry.path.string() + ": " + error.what());
        }
    }

    return PhotonArtLibrary{std::move(photon_arts)};
}

void SavePhotonArt(const std::filesystem::path& path, const PhotonArt& photon_art)
{
    rapidjson::Document document;
    document.SetObject();
    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

    document.AddMember("schema_version", kPhotonArtLibraryVersion, allocator);
    rapidjson::Value body = WritePhotonArtBody(photon_art, allocator);

    // Round-trip through ReadPhotonArtBody so a save can never produce a file
    // LoadPhotonArtLibrary would reject on content grounds.
    ReadPhotonArtBody(body);

    for (auto member = body.MemberBegin(); member != body.MemberEnd(); ++member)
        document.AddMember(member->name, member->value, allocator);

    ValidatePhotonArtDocument(document, BuildPhotonArtSchemaModel());

    WriteJsonFile(path, document);
}

} // namespace psr
