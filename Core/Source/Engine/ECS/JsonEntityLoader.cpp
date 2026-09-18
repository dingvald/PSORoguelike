#include "Engine/ECS/JsonEntityLoader.h"

#include "Engine/ECS/EntitySchemaEmitter.h"
#include "Engine/ECS/NameIdRegistry.h"

#include <entt/entt.hpp>

#include <string>

namespace psr {

using namespace entt::literals;

namespace {

    // Top-level "schema_version" the entities document must carry (M0's explicit
    // no-migration policy, enforced by ReadJsonFile).
    constexpr int kEntitySchemaVersion = 1;

} // namespace

JsonEntityLoader::JsonEntityLoader(entt::meta_ctx& ctx, const EntitySchemaModel* schema) : m_ctx(ctx), m_schema(schema)
{
}

bool JsonEntityLoader::Load(std::filesystem::path path)
{
    m_entries = LoadJsonDirectory(path, kEntitySchemaVersion);
    if (m_schema)
        for (const JsonDirectoryEntry& entry : m_entries)
        {
            try
            {
                ValidateEntityDocument(entry.document, *m_schema);
            }
            catch (const EntityLoaderError& error)
            {
                throw EntityLoaderError(entry.path.string() + ": " + error.what());
            }
        }
    return true;
}

void JsonEntityLoader::Populate(entt::registry& prefab_registry,
                                std::unordered_map<std::uint32_t, entt::entity>& out_prefab_ids)
{
    for (const JsonDirectoryEntry& entry : m_entries)
    {
        std::uint32_t prefab_id = entt::hashed_string::value(entry.id.c_str());
        NameIdRegistry::Register(prefab_id, entry.id);

        if (out_prefab_ids.contains(prefab_id))
            throw EntityLoaderError("JsonEntityLoader: duplicate prefab id for '" + entry.id + "'");
        if (!entry.document.IsObject())
            throw EntityLoaderError("JsonEntityLoader: entity '" + entry.id + "' must be an object");

        entt::entity prefab = prefab_registry.create();

        auto components = entry.document.FindMember("components");
        if (components != entry.document.MemberEnd())
        {
            if (!components->value.IsObject())
                throw EntityLoaderError("JsonEntityLoader: 'components' of '" + entry.id + "' must be an object");

            for (auto component = components->value.MemberBegin(); component != components->value.MemberEnd();
                 ++component)
            {
                entt::meta_type type = entt::resolve(m_ctx, entt::hashed_string::value(component->name.GetString()));
                if (!type)
                    throw EntityLoaderError(std::string("JsonEntityLoader: unknown component '") +
                                            component->name.GetString() + "' in '" + entry.id + "'");
                EmplaceComponentFromJson(type, prefab_registry, prefab, component->value, m_ctx);
            }
        }

        out_prefab_ids.emplace(prefab_id, prefab);
    }
}

} // namespace psr
