#include "UI/AssetRenameCascade.h"

#include "Engine/Persistence/JsonFile.h"

#include <EditorFilepaths.h>

#include <SDL3/SDL.h>

#include <entt/core/hashed_string.hpp>

#include <rapidjson/document.h>

#include <algorithm>
#include <filesystem>

namespace psr {

namespace {

    // A NameId reference field (see Core/Engine/ECS/ComponentSchema.h) round-trips
    // as a name string only when NameIdRegistry happened to have already seen that
    // exact string this session (see NameIdRegistry.h) -- otherwise it's saved as
    // the raw hashed id instead. Real content on disk is a mix of both forms for
    // the same logical reference, so a rename has to match either: a string equal
    // to old_id, or a number equal to old_id's hash. Either match is rewritten to
    // the new id as a string, which is always resolvable without depending on
    // NameIdRegistry's session-local state.
    bool RewriteReferences(rapidjson::Value& value, rapidjson::Document::AllocatorType& allocator,
                           const std::vector<std::string>& fields, const std::string& old_id, std::uint32_t old_hash,
                           const std::string& new_id)
    {
        bool changed = false;
        if (value.IsObject())
        {
            for (auto member = value.MemberBegin(); member != value.MemberEnd(); ++member)
            {
                if (std::find(fields.begin(), fields.end(), member->name.GetString()) != fields.end())
                {
                    const bool string_match = member->value.IsString() && member->value.GetString() == old_id;
                    const bool hash_match = (member->value.IsUint() && member->value.GetUint() == old_hash) ||
                                            (member->value.IsInt() && member->value.GetInt() >= 0 &&
                                             static_cast<std::uint32_t>(member->value.GetInt()) == old_hash);
                    if (string_match || hash_match)
                    {
                        member->value.SetString(new_id.c_str(), static_cast<rapidjson::SizeType>(new_id.size()),
                                                allocator);
                        changed = true;
                        continue;
                    }
                }
                if (RewriteReferences(member->value, allocator, fields, old_id, old_hash, new_id))
                    changed = true;
            }
        }
        else if (value.IsArray())
        {
            for (auto& element : value.GetArray())
                if (RewriteReferences(element, allocator, fields, old_id, old_hash, new_id))
                    changed = true;
        }
        return changed;
    }

} // namespace

int UpdateReferencesOnRename(AssetKind kind, const std::string& old_id, const std::string& new_id)
{
    const std::vector<std::string>& fields = ReferenceFieldNamesFor(kind);
    if (fields.empty())
        return 0;
    const std::uint32_t old_hash = entt::hashed_string::value(old_id.c_str());

    int files_updated = 0;
    std::error_code error_code;
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::recursive_directory_iterator(EditorFilepaths::DataPath, error_code))
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".json")
            continue;

        try
        {
            rapidjson::Document document = ReadJsonFile(entry.path());
            if (RewriteReferences(document, document.GetAllocator(), fields, old_id, old_hash, new_id))
            {
                WriteJsonFile(entry.path(), document);
                ++files_updated;
            }
        }
        catch (const std::exception& error)
        {
            SDL_Log("AssetRenameCascade: skipping '%s': %s", entry.path().string().c_str(), error.what());
        }
    }
    return files_updated;
}

} // namespace psr
