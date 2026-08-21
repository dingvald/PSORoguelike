#pragma once

#include <entt/entt.hpp>

#include <cstdint>
#include <filesystem>
#include <unordered_map>

namespace psr {

// Pure interface for authoring prefab entities into a Registry's
// prefab_registry. "I"-prefixed: this project reserves the I prefix for
// pure-abstract interfaces (contrast Layer, which has default method
// bodies and stays unprefixed).
class IEntityLoader
{
public:
    IEntityLoader() = default;
    virtual ~IEntityLoader() = default;

    IEntityLoader(const IEntityLoader&) = delete;
    IEntityLoader& operator=(const IEntityLoader&) = delete;
    IEntityLoader(IEntityLoader&&) = delete;
    IEntityLoader& operator=(IEntityLoader&&) = delete;

    // Reads prefab definitions from path, ready for a subsequent Populate()
    // call. What "reading" means is loader-specific (e.g. JsonEntityLoader
    // scans a directory of JSON fragments).
    virtual bool Load(std::filesystem::path path) = 0;

    // Fills prefab_registry with prefab entities, recording each one's id
    // into out_prefab_ids in the same step so the id-to-entity mapping
    // can never fall out of sync with what was actually created.
    virtual void Populate(entt::registry& prefab_registry,
                          std::unordered_map<std::uint32_t, entt::entity>& out_prefab_ids) = 0;
};

} // namespace psr
