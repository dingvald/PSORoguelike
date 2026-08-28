#include "Engine/ECS/Registry.h"

#include "Engine/Combat/StatusEffectLibrary.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/Items/AffixLibrary.h"
#include "Engine/World/Grid.h"

#include <cassert>

namespace psr {

using namespace entt::literals;

Registry::Registry()
    : m_runtime_registry(std::make_unique<entt::registry>()), m_prefab_registry(std::make_unique<entt::registry>())
{
    // Lets FromEntt() recover this Registry from the raw entt::registry& that
    // entt's on_construct/on_destroy listeners are invoked with.
    m_runtime_registry->ctx().emplace<Registry*>(this);
}

Registry::~Registry() = default;

Registry::Registry(Registry&& other) noexcept
    : m_meta_ctx(std::move(other.m_meta_ctx)), m_runtime_registry(std::move(other.m_runtime_registry)),
      m_prefab_registry(std::move(other.m_prefab_registry)), m_prefabs(std::move(other.m_prefabs))
{
    // The moved-from Registry's self-pointer in ctx() still points at the
    // old object's address -- the underlying entt::registry heap object
    // itself didn't move, only pointer ownership did, so re-stamp it here.
    if (m_runtime_registry)
        m_runtime_registry->ctx().insert_or_assign<Registry*>(this);
}

Registry& Registry::operator=(Registry&& other) noexcept
{
    if (this == &other)
        return *this;

    m_meta_ctx = std::move(other.m_meta_ctx);
    m_runtime_registry = std::move(other.m_runtime_registry);
    m_prefab_registry = std::move(other.m_prefab_registry);
    m_prefabs = std::move(other.m_prefabs);

    if (m_runtime_registry)
        m_runtime_registry->ctx().insert_or_assign<Registry*>(this);

    return *this;
}

entt::entity Registry::CreateEntity()
{
    entt::entity entity = m_runtime_registry->create();
    m_runtime_registry->emplace<EventHandlerComponent>(entity);
    return entity;
}

bool Registry::HasPrefab(std::uint32_t prefab_id) const { return m_prefabs.contains(prefab_id); }

entt::entity Registry::CreateEntity(std::uint32_t prefab_id)
{
    auto prefab_it = m_prefabs.find(prefab_id);
    assert(prefab_it != m_prefabs.end() &&
           "Registry::CreateEntity: unknown prefab_id -- was RegisterPrefabs() called?");
    entt::entity prefab = prefab_it->second;

    entt::entity entity = m_runtime_registry->create();

    // EventHandlerComponent must exist before any cloned component's
    // on_construct<T> handler fires below (cloning still goes through
    // registry.emplace_or_replace<T>(), which fires on_construct for a
    // brand-new component) -- see Registry::CreateEntity()'s own ordering.
    m_runtime_registry->emplace<EventHandlerComponent>(entity);
    m_runtime_registry->emplace<PrefabIdComponent>(entity, prefab_id);

    // Clone every component the prefab has via entt::meta -- lets Registry
    // (Core) copy components it has no compile-time knowledge of. Each
    // component's own Register(ComponentSchemaRegistrar&) is what makes this
    // possible (see ComponentMeta.h's CloneComponent<T>). We iterate every
    // registered meta type rather than the prefab's storage() so the clone
    // works even when a component's meta id differs from entt's
    // storage/type-hash id (components author a friendly .type("name"_hs) id
    // for the data-driven loader, which would otherwise break a storage-id ->
    // meta-type bridge); CloneComponent<T> self-guards on presence, so
    // invoking it for a type the prefab lacks is a harmless no-op. instance
    // is a context-only placeholder, not the component value.
    for (auto&& [id, type] : entt::resolve(m_meta_ctx))
    {
        if (!type.func("clone"_hs))
            continue;

        entt::meta_any instance{entt::meta_ctx_arg, m_meta_ctx};
        type.invoke("clone"_hs, instance, entt::forward_as_meta(m_meta_ctx, *m_prefab_registry), prefab,
                    entt::forward_as_meta(m_meta_ctx, *m_runtime_registry), entity);
    }

    return entity;
}

void Registry::DestroyEntity(entt::entity entity) { m_runtime_registry->destroy(entity); }

bool Registry::IsValid(entt::entity entity) const { return m_runtime_registry->valid(entity); }

void Registry::RegisterPrefabs(IEntityLoader& loader)
{
    m_prefab_registry->clear();
    m_prefabs.clear();
    loader.Populate(*m_prefab_registry, m_prefabs);
}

entt::meta_ctx& Registry::GetMetaContext() { return m_meta_ctx; }

Registry& Registry::FromEntt(entt::registry& runtime_registry) { return *runtime_registry.ctx().get<Registry*>(); }

void Registry::SetAffixLibrary(const AffixLibrary& affixes)
{
    m_runtime_registry->ctx().insert_or_assign<const AffixLibrary*>(&affixes);
}

const AffixLibrary& Registry::GetAffixLibrary()
{
    auto* affixes = m_runtime_registry->ctx().find<const AffixLibrary*>();
    assert(affixes && "Registry::GetAffixLibrary: SetAffixLibrary() must be called first");
    return **affixes;
}

void Registry::SetStatusEffectLibrary(const StatusEffectLibrary& status_effects)
{
    m_runtime_registry->ctx().insert_or_assign<const StatusEffectLibrary*>(&status_effects);
}

const StatusEffectLibrary& Registry::GetStatusEffectLibrary()
{
    auto* status_effects = m_runtime_registry->ctx().find<const StatusEffectLibrary*>();
    assert(status_effects && "Registry::GetStatusEffectLibrary: SetStatusEffectLibrary() must be called first");
    return **status_effects;
}

void Registry::SetGrid(Grid& grid) { m_runtime_registry->ctx().insert_or_assign<Grid*>(&grid); }

Grid& Registry::GetGrid()
{
    auto* grid = m_runtime_registry->ctx().find<Grid*>();
    assert(grid && "Registry::GetGrid: SetGrid() must be called first");
    return **grid;
}

std::vector<ComponentValue> Registry::DescribeEntity(entt::entity entity, const EntitySchemaModel& schema) const
{
    std::vector<ComponentValue> result;
    for (const ComponentSchema& component_schema : schema.components)
    {
        entt::meta_type type = entt::resolve(m_meta_ctx, entt::hashed_string::value(component_schema.id.c_str()));
        if (!type)
            continue;

        entt::meta_any instance{entt::meta_ctx_arg, m_meta_ctx};
        entt::meta_any pointer_any =
            type.invoke("describe_fields"_hs, instance, entt::forward_as_meta(m_meta_ctx, *m_runtime_registry), entity);
        if (!pointer_any)
            continue;

        const void* raw = pointer_any.cast<const void*>();
        if (!raw)
            continue;

        entt::meta_any value = type.from_void(raw);
        if (!value)
            continue;

        result.push_back(DescribeComponentValue(component_schema, type, value));
    }
    return result;
}

} // namespace psr
