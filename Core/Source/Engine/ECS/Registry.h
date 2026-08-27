#pragma once

#include "Engine/ECS/ComponentSchema.h"
#include "Engine/ECS/EntityDescriber.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/IEntityLoader.h"

#include <entt/entt.hpp>

#include <cstdint>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace psr {

class AffixLibrary;

// Owning wrapper around a pair of entt::registry instances, so call sites use
// this class's vocabulary (CreateEntity/Emplace/GetComponent/...) rather than
// entt's registry API directly -- keeps entt as an implementation detail of
// the engine's ECS module. Non-copyable (a registry is a heavyweight
// resource, C.21); movable (owners compose one by value).
//
// Two registries, not one: runtime_registry holds gameplay entities,
// prefab_registry holds only templates authored via RegisterPrefabs().
// Neither ever contains the other's kind of entity -- structurally enforced,
// since they're different entt::registry objects.
class Registry
{
public:
    Registry();
    ~Registry();

    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;
    Registry(Registry&&) noexcept;
    Registry& operator=(Registry&&) noexcept;

    entt::entity CreateEntity();

    // Clones prefab_id's components onto a fresh runtime_registry entity.
    // prefab_id must have been registered via RegisterPrefabs() first.
    entt::entity CreateEntity(std::uint32_t prefab_id);

    // True iff prefab_id was registered via RegisterPrefabs() -- lets a
    // caller validate an id (e.g. one typed into a debug console) before
    // calling CreateEntity(prefab_id), whose own unknown-id check is only an
    // assert (a no-op in release builds).
    bool HasPrefab(std::uint32_t prefab_id) const;

    void DestroyEntity(entt::entity entity);
    bool IsValid(entt::entity entity) const;

    // decltype(auto), not TComponent&: entt's own emplace() returns void for
    // empty/tag components (its empty-type storage optimization), so a
    // fixed TComponent& return type would fail to compile for them -- see
    // ComponentMeta.h's CloneComponent<T>, which branches on the same
    // std::is_empty_v<TComponent> for the same reason.
    template <typename TComponent, typename... TArgs> decltype(auto) Emplace(entt::entity entity, TArgs&&... args)
    {
        if constexpr (std::is_empty_v<TComponent>)
            m_runtime_registry->emplace<TComponent>(entity);
        else
            return m_runtime_registry->emplace<TComponent>(entity, std::forward<TArgs>(args)...);
    }

    template <typename TComponent> TComponent& GetComponent(entt::entity entity)
    {
        return m_runtime_registry->get<TComponent>(entity);
    }

    // Returns entity's TComponent, default-constructing (emplacing) it first if
    // absent. Use over GetComponent when a component's on_construct handler must
    // reach another component that may not exist yet.
    template <typename TComponent, typename... TArgs> decltype(auto) GetOrEmplace(entt::entity entity, TArgs&&... args)
    {
        if constexpr (std::is_empty_v<TComponent>)
            m_runtime_registry->get_or_emplace<TComponent>(entity);
        else
            return m_runtime_registry->get_or_emplace<TComponent>(entity, std::forward<TArgs>(args)...);
    }

    template <typename TComponent> const TComponent& GetComponent(entt::entity entity) const
    {
        return m_runtime_registry->get<TComponent>(entity);
    }

    template <typename TComponent> TComponent* TryGetComponent(entt::entity entity)
    {
        return m_runtime_registry->try_get<TComponent>(entity);
    }

    template <typename TComponent> const TComponent* TryGetComponent(entt::entity entity) const
    {
        return m_runtime_registry->try_get<TComponent>(entity);
    }

    template <typename TComponent> bool HasComponent(entt::entity entity) const
    {
        return m_runtime_registry->all_of<TComponent>(entity);
    }

    // Invokes func(entt::entity, TComponent&) for every live entity that has
    // TComponent -- the controlled way for a system to query by component
    // without call sites touching entt's view API directly. Do not
    // create/destroy entities from inside func; collect and act afterwards.
    template <typename TComponent, typename TFunc> void Each(TFunc&& func)
    {
        m_runtime_registry->view<TComponent>().each(std::forward<TFunc>(func));
    }

    // Two-type overload: invokes func(entt::entity, TComponent&) for every live
    // entity that has both TComponent and TFilter -- TFilter contributes no
    // value to func (entt's view omits empty types from the each() tuple), so
    // this is for narrowing an Each<TComponent> sweep by a rare tag. Entt's
    // view drives iteration from whichever of the two storages is smallest, so
    // this stays cheap even when TComponent is common and TFilter is rare --
    // unlike a plain Each<TComponent> with a HasComponent<TFilter> check
    // inside, which would visit every TComponent entity regardless of
    // TFilter's rarity. TFilter must not itself be empty in a way func's
    // signature expects a value for (see the single-type Each<T> doc comment)
    // -- pass it purely as a filter.
    template <typename TComponent, typename TFilter, typename TFunc> void Each(TFunc&& func)
    {
        m_runtime_registry->view<TComponent, TFilter>().each(std::forward<TFunc>(func));
    }

    // True iff at least one live entity currently has TComponent.
    template <typename TComponent> bool Any() const { return !m_runtime_registry->view<TComponent>().empty(); }

    // TComponent must not be EventHandlerComponent -- it's managed entirely
    // by Registry (always the first component added, see CreateEntity) and
    // only ever goes away with the whole entity via DestroyEntity.
    template <typename TComponent> void Remove(entt::entity entity)
    {
        static_assert(!std::is_same_v<TComponent, EventHandlerComponent>,
                      "EventHandlerComponent is managed by Registry and must not be removed directly");
        m_runtime_registry->remove<TComponent>(entity);
    }

    // Removes TComponent from every entity that currently has it, in one
    // call -- a thin wrapper over entt's own registry::clear<T>(). Entities
    // themselves are untouched (not destroyed); exists so a caller doing a
    // full per-turn tag reset can avoid iterating and calling Remove<T> one
    // entity at a time.
    template <typename TComponent> void Clear()
    {
        static_assert(!std::is_same_v<TComponent, EventHandlerComponent>,
                      "EventHandlerComponent is managed by Registry and must not be cleared directly");
        m_runtime_registry->clear<TComponent>();
    }

    // Wires TComponent::AttachHandlers/DetachHandlers (signature must match
    // entt's on_construct/on_destroy listener signature, void(entt::registry&,
    // entt::entity)) to fire whenever a TComponent is added to/removed from
    // any live entity -- this is how a component subscribes its own event
    // handlers onto that entity's EventHandlerComponent without Registry
    // needing any compile-time knowledge of what TComponent does. Called
    // explicitly per component type from an App-side RegisterComponents
    // aggregator, only for components that actually define those two statics.
    template <typename TComponent> void BindComponentEvents()
    {
        m_runtime_registry->on_construct<TComponent>().template connect<&TComponent::AttachHandlers>();
        m_runtime_registry->on_destroy<TComponent>().template connect<&TComponent::DetachHandlers>();
    }

    // Connects a member function -- signature void(entt::registry&,
    // entt::entity), entt's fixed listener shape -- to fire whenever TComponent
    // is added to / removed from an entity. Unlike BindComponentEvents (which
    // wires a component's own statics), these bind an external observer that
    // needs state the component can't reach. Always pair a bound instance with
    // DisconnectComponentLifecycle before either the instance or this Registry
    // is destroyed.
    template <typename TComponent, auto Candidate, typename TInstance> void OnConstruct(TInstance& instance)
    {
        m_runtime_registry->on_construct<TComponent>().template connect<Candidate>(instance);
    }

    template <typename TComponent, auto Candidate, typename TInstance> void OnDestroy(TInstance& instance)
    {
        m_runtime_registry->on_destroy<TComponent>().template connect<Candidate>(instance);
    }

    // Drops every on_construct/on_destroy<TComponent> listener bound to
    // instance (see OnConstruct/OnDestroy). Idempotent.
    template <typename TComponent, typename TInstance> void DisconnectComponentLifecycle(TInstance& instance)
    {
        // Pass the address: entt's disconnect(const void*) drops every listener
        // bound to that instance (disconnect(Type&) would need the specific
        // member-function candidate spelled out again).
        m_runtime_registry->on_construct<TComponent>().disconnect(&instance);
        m_runtime_registry->on_destroy<TComponent>().disconnect(&instance);
    }

    // Clears any previously registered prefabs, then fills prefab_registry
    // via loader.Populate() and remembers the resulting prefab_id -> entity
    // mapping for CreateEntity(prefab_id). Safe to call repeatedly (e.g. on a
    // ContentWatcher-detected change) -- each call fully replaces the
    // previous prefab set rather than appending to or leaking it.
    void RegisterPrefabs(IEntityLoader& loader);

    // Components register themselves here (see e.g. a component's own
    // static Register(ComponentSchemaRegistrar&)) before any prefab work
    // happens -- an App-side RegisterComponents() aggregator drives this at
    // startup.
    entt::meta_ctx& GetMetaContext();

    // Stashes a reference to affixes in this registry's ctx(), the same
    // storage FromEntt's self-pointer uses -- lets a component's static
    // AttachHandlers-registered handler (which can't capture state) reach
    // shared combat-content data it needs to contribute to an event (e.g.
    // EquipmentComponent computing effective stats for a Before<Action>Event).
    // Call once, early, before any entity that needs it is created. affixes
    // must outlive this Registry.
    void SetAffixLibrary(const AffixLibrary& affixes);

    // Returns the AffixLibrary stashed via SetAffixLibrary(). Asserts if none
    // was ever set -- a call site that needs this only exists because a
    // component handler needs it, so a missing SetAffixLibrary() call is a
    // setup bug, not a runtime condition to handle gracefully.
    const AffixLibrary& GetAffixLibrary();

    // Recovers the owning Registry& from a raw entt::registry& -- needed
    // because entt's on_construct/on_destroy listener signature is fixed as
    // void(entt::registry&, entt::entity), but component handler bodies want
    // to work in Entity/Registry terms rather than raw entt. Backed by a
    // self-pointer this Registry stashes in its own ctx() (see the
    // constructor and the move special members below, which must keep that
    // pointer in sync with *this*).
    static Registry& FromEntt(entt::registry& runtime_registry);

    // Reads every schema-registered component entity currently has into a
    // formatted, generic value tree -- the live-entity read-direction
    // counterpart to JsonEntityLoader's JSON-to-component write direction.
    // Iterates schema.components in registration order (not entity's actual
    // storage), resolving each by its schema id and invoking its
    // "describe_fields"_hs meta func (bound by every component, see
    // ComponentMeta.h's ReadComponentFields) to test presence and read the
    // current value in one step; components entity doesn't have are silently
    // skipped. This is what lets a caller (e.g. a future item-detail UI) list
    // an arbitrary entity's stats without knowing at compile time which
    // components exist.
    std::vector<ComponentValue> DescribeEntity(entt::entity entity, const EntitySchemaModel& schema) const;

private:
    entt::meta_ctx m_meta_ctx;
    std::unique_ptr<entt::registry> m_runtime_registry;
    std::unique_ptr<entt::registry> m_prefab_registry;
    std::unordered_map<std::uint32_t, entt::entity> m_prefabs;
};

} // namespace psr
