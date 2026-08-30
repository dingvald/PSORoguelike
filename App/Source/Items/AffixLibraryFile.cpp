#include "Items/AffixLibraryFile.h"

#include "Engine/Persistence/JsonDirectoryLoader.h"
#include "Engine/Persistence/JsonFile.h"
#include "Items/AffixError.h"
#include "Items/AffixSchema.h"
#include "Items/AffixSchemaEmitter.h"

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
            throw AffixError(std::string("affix file: '") + key + "' must be a string");
        return it->value.GetString();
    }

    int ReadInt(const rapidjson::Value& object, const char* key, int fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsInt())
            throw AffixError(std::string("affix file: '") + key + "' must be an integer");
        return it->value.GetInt();
    }

    template <typename E>
    E ReadEnum(const rapidjson::Value& object, const char* key, E fallback, const char* error_label)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsString())
            throw AffixError(std::string("affix file: '") + key + "' must be a " + error_label + " name");
        const std::string_view name = it->value.GetString();
        for (const auto& [text, value] : EnumNames<E>::kValues)
            if (text == name)
                return value;
        throw AffixError(std::string("affix file: unknown ") + error_label + " '" + std::string(name) + "'");
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

Affix ReadAffixBody(const rapidjson::Value& affix_def)
{
    Affix affix;
    affix.name = ReadString(affix_def, "name", affix.name);
    affix.kind = ReadEnum<AffixKind>(affix_def, "kind", affix.kind, "kind");
    affix.stat = ReadEnum<AffixStat>(affix_def, "stat", affix.stat, "stat");
    affix.amount = ReadInt(affix_def, "amount", affix.amount);
    return affix;
}

rapidjson::Value WriteAffixBody(const Affix& affix, rapidjson::Document::AllocatorType& allocator)
{
    rapidjson::Value object(rapidjson::kObjectType);
    object.AddMember("name", StringValue(affix.name, allocator), allocator);
    object.AddMember("kind", StringValue(std::string{EnumName(affix.kind)}, allocator), allocator);
    object.AddMember("stat", StringValue(std::string{EnumName(affix.stat)}, allocator), allocator);
    object.AddMember("amount", affix.amount, allocator);
    return object;
}

AffixLibrary LoadAffixLibrary(const std::filesystem::path& directory)
{
    const std::vector<JsonDirectoryEntry> entries = LoadJsonDirectory(directory, kAffixLibraryVersion);
    const AffixSchemaModel schema = BuildAffixSchemaModel();

    std::vector<Affix> affixes;
    for (const JsonDirectoryEntry& entry : entries)
    {
        try
        {
            ValidateAffixDocument(entry.document, schema);

            if (!entry.document.IsObject())
                throw AffixError("affix file: must be an object");

            Affix affix = ReadAffixBody(entry.document);
            affix.id_string = entry.id;
            affix.id = entt::hashed_string::value(affix.id_string.c_str());
            if (affix.name.empty())
                affix.name = affix.id_string;

            affixes.push_back(std::move(affix));
        }
        catch (const AffixError& error)
        {
            throw AffixError(entry.path.string() + ": " + error.what());
        }
    }

    return AffixLibrary{std::move(affixes)};
}

void SaveAffix(const std::filesystem::path& path, const Affix& affix)
{
    rapidjson::Document document;
    document.SetObject();
    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

    document.AddMember("schema_version", kAffixLibraryVersion, allocator);
    rapidjson::Value body = WriteAffixBody(affix, allocator);

    // Round-trip through ReadAffixBody so a save can never produce a file
    // LoadAffixLibrary would reject on content grounds.
    ReadAffixBody(body);

    for (auto member = body.MemberBegin(); member != body.MemberEnd(); ++member)
        document.AddMember(member->name, member->value, allocator);

    ValidateAffixDocument(document, BuildAffixSchemaModel());

    WriteJsonFile(path, document);
}

} // namespace psr
