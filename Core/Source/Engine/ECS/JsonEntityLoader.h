#pragma once

#include "Engine/ECS/ComponentSchema.h"
#include "Engine/ECS/IEntityLoader.h"
#include "Engine/Persistence/JsonDirectoryLoader.h"

#include <rapidjson/document.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace psr {

// Thrown for a structurally valid JSON document whose *content* can't be turned
// into prefabs -- an unknown component or field name, a duplicate prefab id, a
// value whose JSON shape doesn't match the target field type, etc. A malformed
// or unreadable file (or a schema_version mismatch) surfaces as JsonFileError
// from ReadJsonFile instead. One error type per subsystem, mirroring
// JsonFileError.
class EntityLoaderError : public std::runtime_error
{
public:
    explicit EntityLoaderError(const std::string& message) : std::runtime_error(message) {}
};

// Data-driven IEntityLoader: reads prefab definitions from a directory of JSON
// files (one entity per file, scanned recursively via LoadJsonDirectory) and
// authors them into a Registry's prefab_registry entirely through entt::meta,
// with no compile-time knowledge of any concrete component type.
//
// Each file's shape:
//
//   {
//     "schema_version": 1,
//     "components": {
//       "<component-name>": { "<field>": <value>, ... },
//       "<tag-component>": {}
//     }
//   }
//
// A prefab's id is its path relative to the loaded directory, with '/'
// replaced by '.' and ".json" stripped -- e.g. "terrain/floor.json" becomes
// id string "terrain.floor" -> entt::hashed_string::value("terrain.floor") --
// so a caller instantiates it with Registry::CreateEntity("terrain.floor"_hs).
// Component names resolve against
// the friendly meta id each component declares via .type("name"_hs) in its
// Register(); field names resolve against the component's .data("name"_hs)
// members. Field values follow these conventions:
//   * numbers / bools  -> the matching arithmetic field (auto-converted);
//   * a *string* on a numeric field -> entt::hashed_string of that string,
//     for name-as-id fields like a texture_id member
//     (e.g. "texture_id": "floor");
//   * Color            -> "#RRGGBB[AA]" hex string, or { "r","g","b","a" };
//   * Vec2             -> { "x", "y" } (omitted axes default to 0).
class JsonEntityLoader : public IEntityLoader
{
public:
    // ctx must be the same entt::meta_ctx the components were registered into
    // (Registry::GetMetaContext()); it's where component/field names resolve.
    // When schema is non-null, Load() validates the document against it (built
    // from the same registration, so it can never go stale) before returning;
    // pass RegisterComponents()'s returned model. It must outlive this loader.
    explicit JsonEntityLoader(entt::meta_ctx& ctx, const EntitySchemaModel* schema = nullptr);

    // Recursively scans path for *.json files (via LoadJsonDirectory, so a
    // missing directory, a malformed fragment, or a schema_version mismatch
    // throws JsonFileError). If a schema was supplied, additionally validates
    // every fragment against it, throwing EntityLoaderError on any violation.
    // Always returns true on success; the bool is IEntityLoader's contract,
    // failures throw.
    bool Load(std::filesystem::path path) override;

    // Creates one prefab entity per file found by Load(), recording each id in
    // out_prefab_ids. Must be called after a successful Load(). Throws
    // EntityLoaderError on any content problem.
    void Populate(entt::registry& prefab_registry,
                  std::unordered_map<std::uint32_t, entt::entity>& out_prefab_ids) override;

private:
    entt::meta_ctx& m_ctx;
    const EntitySchemaModel* m_schema;
    std::vector<JsonDirectoryEntry> m_entries;
};

} // namespace psr
