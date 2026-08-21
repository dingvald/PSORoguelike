#pragma once

#include <entt/entt.hpp>

#include <type_traits>

namespace psr {

// Shared clone glue every component's Register(ComponentSchemaRegistrar&) binds
// as its "clone"_hs meta function (see ComponentSchemaRegistrar::Component,
// which calls RegisterComponent<T> below):
//
//   type.invoke("clone"_hs, instance, entt::forward_as_meta(ctx, source_registry), source_entity,
//               entt::forward_as_meta(ctx, registry), entity);
//
// Copies TComponent from source_entity in source_registry onto entity in
// registry, or does nothing if source_entity has no TComponent. This is what
// lets Registry::CreateEntity(prefab_id) clone a component it has no
// compile-time knowledge of, by invoking "clone"_hs on every registered meta
// type (the self-guard makes cloning types the prefab lacks a harmless no-op).
//
// Deliberately NOT instance-bound (no parameter is TComponent) -- entt::meta's
// empty-type storage optimization (used for tag components) never materializes
// a value pointer, so an instance-bound signature has no valid TComponent
// instance to bind for those types. Reading the source value explicitly via
// source_registry/source_entity sidesteps that entirely, for both empty and
// non-empty TComponent.
template <typename TComponent>
void CloneComponent(const entt::registry& source_registry, entt::entity source_entity, entt::registry& registry,
                    entt::entity entity)
{
    if constexpr (std::is_empty_v<TComponent>)
    {
        if (source_registry.all_of<TComponent>(source_entity))
            registry.emplace_or_replace<TComponent>(entity);
    }
    else
    {
        if (const TComponent* value = source_registry.try_get<TComponent>(source_entity))
            registry.emplace_or_replace<TComponent>(entity, *value);
    }
}

// Read-direction mirror of CloneComponent -- returns a pointer to
// TComponent's current value on entity, or nullptr if entity doesn't have
// it. Bound as every component's "describe_fields"_hs meta func (see
// Registry::DescribeEntity), letting a caller holding only an
// EntitySchemaModel entry -- no compile-time TComponent -- ask "does this
// entity have this component, and if so what's in it right now."
//
// Returns a raw const void*, not entt::meta_any, and deliberately takes no
// entt::meta_ctx -- entt::meta_ctx holds a dense_map of unique_ptrs
// internally and so isn't copy-assignable, and building a meta_any of (or a
// pointer to) *any* type forces entt to instantiate that type's copy-vtable
// entry, which fails to compile for meta_ctx specifically. Returning a bare
// pointer sidesteps meta_ctx entirely; Registry::DescribeEntity turns it back
// into a properly-typed meta_any via meta_type::from_void, which only
// touches TComponent's vtable (always safe -- an ordinary component type),
// never meta_ctx's. For an empty tag component there's no storage to point
// at, so a stable per-instantiation sentinel stands in for "present" --
// DescribeComponentValue never dereferences it (a tag schema has no fields).
template <typename TComponent> const void* ReadComponentFields(const entt::registry& registry, entt::entity entity)
{
    if constexpr (std::is_empty_v<TComponent>)
    {
        static const TComponent kTagPresentSentinel{};
        return registry.all_of<TComponent>(entity) ? static_cast<const void*>(&kTagPresentSentinel) : nullptr;
    }
    else
    {
        return static_cast<const void*>(registry.try_get<TComponent>(entity));
    }
}

// Emplaces value onto entity in registry -- the write-side counterpart used by
// data-driven loaders (JsonEntityLoader) that have built a TComponent through
// entt::meta but can't call registry.emplace<TComponent> without the concrete
// type. For empty tag components value carries no state, so it's dropped and
// only the tag is emplaced (mirrors CloneComponent's empty-type branch).
template <typename TComponent>
void EmplaceComponent(entt::registry& registry, entt::entity entity, const TComponent& value)
{
    if constexpr (std::is_empty_v<TComponent>)
        registry.emplace_or_replace<TComponent>(entity);
    else
        registry.emplace_or_replace<TComponent>(entity, value);
}

template <typename TComponent> entt::meta_factory<TComponent> RegisterComponent(entt::meta_ctx& ctx)
{
    using namespace entt::literals;
    return entt::meta_factory<TComponent>(ctx)
        .func<&CloneComponent<TComponent>>("clone"_hs)
        .func<&EmplaceComponent<TComponent>>("emplace"_hs)
        .func<&ReadComponentFields<TComponent>>("describe_fields"_hs);
}

} // namespace psr
