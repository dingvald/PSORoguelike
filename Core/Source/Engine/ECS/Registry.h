#pragma once

#include <entt/entt.hpp>

#include <type_traits>
#include <utility>

namespace psr {

// Thin wrapper around entt::registry so call sites never touch entt's API
// directly. Deliberately minimal for now -- no prefabs, no reflection, no
// save/load. Grows toward those as the systems that need them land.
class Registry
{
public:
    Registry() = default;
    ~Registry() = default;

    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;
    Registry(Registry&&) noexcept = default;
    Registry& operator=(Registry&&) noexcept = default;

    entt::entity CreateEntity() { return m_registry.create(); }
    void DestroyEntity(entt::entity entity) { m_registry.destroy(entity); }
    bool IsValid(entt::entity entity) const { return m_registry.valid(entity); }

    template <typename TComponent, typename... TArgs> decltype(auto) Emplace(entt::entity entity, TArgs&&... args)
    {
        return m_registry.emplace<TComponent>(entity, std::forward<TArgs>(args)...);
    }

    template <typename TComponent> TComponent& GetComponent(entt::entity entity)
    {
        return m_registry.get<TComponent>(entity);
    }

    template <typename TComponent> const TComponent& GetComponent(entt::entity entity) const
    {
        return m_registry.get<TComponent>(entity);
    }

    template <typename TComponent> TComponent* TryGetComponent(entt::entity entity)
    {
        return m_registry.try_get<TComponent>(entity);
    }

    template <typename TComponent> bool HasComponent(entt::entity entity) const
    {
        return m_registry.all_of<TComponent>(entity);
    }

    template <typename TComponent> void Remove(entt::entity entity) { m_registry.remove<TComponent>(entity); }

    template <typename TComponent, typename TFunc> void Each(TFunc&& func)
    {
        m_registry.view<TComponent>().each(std::forward<TFunc>(func));
    }

private:
    entt::registry m_registry;
};

} // namespace psr
