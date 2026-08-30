#include "Combat/StatusEffectLibraryFile.h"

#include "Combat/StatusEffectError.h"
#include "Combat/StatusEffectSchema.h"
#include "Combat/StatusEffectSchemaEmitter.h"
#include "Engine/Persistence/JsonDirectoryLoader.h"
#include "Engine/Persistence/JsonFile.h"

#include <entt/core/hashed_string.hpp>

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
            throw StatusEffectError(std::string("status effect file: '") + key + "' must be a string");
        return it->value.GetString();
    }

    int ReadInt(const rapidjson::Value& object, const char* key, int fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsInt())
            throw StatusEffectError(std::string("status effect file: '") + key + "' must be an integer");
        return it->value.GetInt();
    }

    template <typename E>
    E ReadEnum(const rapidjson::Value& object, const char* key, E fallback, const char* error_label)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsString())
            throw StatusEffectError(std::string("status effect file: '") + key + "' must be a " + error_label +
                                    " name");
        const std::string_view name = it->value.GetString();
        for (const auto& [text, value] : EnumNames<E>::kValues)
            if (text == name)
                return value;
        throw StatusEffectError(std::string("status effect file: unknown ") + error_label + " '" + std::string(name) +
                                "'");
    }

    rapidjson::Value StringValue(const std::string& text, rapidjson::Document::AllocatorType& allocator)
    {
        rapidjson::Value value;
        value.SetString(text.c_str(), static_cast<rapidjson::SizeType>(text.size()), allocator);
        return value;
    }

    template <typename E> std::string_view EnumName(E value)
    {
        for (const auto& [text, candidate] : EnumNames<E>::kValues)
            if (candidate == value)
                return text;
        return EnumNames<E>::kValues.front().first; // unreachable for a valid enum value
    }

} // namespace

StatusEffect ReadStatusEffectBody(const rapidjson::Value& status_effect_def)
{
    StatusEffect status_effect;
    status_effect.name = ReadString(status_effect_def, "name", status_effect.name);
    status_effect.type = ReadEnum<StatusEffectType>(status_effect_def, "type", status_effect.type, "type");
    status_effect.magnitude = ReadInt(status_effect_def, "magnitude", status_effect.magnitude);
    status_effect.duration = ReadInt(status_effect_def, "duration", status_effect.duration);
    return status_effect;
}

rapidjson::Value WriteStatusEffectBody(const StatusEffect& status_effect, rapidjson::Document::AllocatorType& allocator)
{
    rapidjson::Value object(rapidjson::kObjectType);
    object.AddMember("name", StringValue(status_effect.name, allocator), allocator);
    object.AddMember("type", StringValue(std::string{EnumName(status_effect.type)}, allocator), allocator);
    object.AddMember("magnitude", status_effect.magnitude, allocator);
    object.AddMember("duration", status_effect.duration, allocator);
    return object;
}

StatusEffectLibrary LoadStatusEffectLibrary(const std::filesystem::path& directory)
{
    const std::vector<JsonDirectoryEntry> entries = LoadJsonDirectory(directory, kStatusEffectLibraryVersion);
    const StatusEffectSchemaModel schema = BuildStatusEffectSchemaModel();

    std::vector<StatusEffect> status_effects;
    for (const JsonDirectoryEntry& entry : entries)
    {
        try
        {
            ValidateStatusEffectDocument(entry.document, schema);

            if (!entry.document.IsObject())
                throw StatusEffectError("status effect file: must be an object");

            StatusEffect status_effect = ReadStatusEffectBody(entry.document);
            status_effect.id_string = entry.id;
            status_effect.id = entt::hashed_string::value(status_effect.id_string.c_str());
            if (status_effect.name.empty())
                status_effect.name = status_effect.id_string;

            status_effects.push_back(std::move(status_effect));
        }
        catch (const StatusEffectError& error)
        {
            throw StatusEffectError(entry.path.string() + ": " + error.what());
        }
    }

    return StatusEffectLibrary{std::move(status_effects)};
}

void SaveStatusEffect(const std::filesystem::path& path, const StatusEffect& status_effect)
{
    rapidjson::Document document;
    document.SetObject();
    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

    document.AddMember("schema_version", kStatusEffectLibraryVersion, allocator);
    rapidjson::Value body = WriteStatusEffectBody(status_effect, allocator);

    // Round-trip through ReadStatusEffectBody so a save can never produce a
    // file LoadStatusEffectLibrary would reject on content grounds.
    ReadStatusEffectBody(body);

    for (auto member = body.MemberBegin(); member != body.MemberEnd(); ++member)
        document.AddMember(member->name, member->value, allocator);

    ValidateStatusEffectDocument(document, BuildStatusEffectSchemaModel());

    WriteJsonFile(path, document);
}

} // namespace psr
