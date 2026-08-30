#pragma once

#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/Registry.h"

#include <entt/entt.hpp>

#include <utility>

namespace psr {

// A lightweight, non-owning {Registry*, entt::entity} pair -- this engine's
// equivalent of entt::handle. Actions and event-handler code use this as
// their vocabulary for "an entity plus the means to touch its components",
// so callers only need to be constructed with their semantic target rather
// than threading a Registry& and entt::entity separately.
class Entity
{
public:
    Entity() = default;
    Entity(Registry& registry, entt::entity handle) : m_registry(&registry), m_handle(handle) {}

    template <typename TComponent, typename... TArgs> decltype(auto) Emplace(TArgs&&... args) const
    {
        return m_registry->Emplace<TComponent>(m_handle, std::forward<TArgs>(args)...);
    }

    template <typename TComponent, typename... TArgs> decltype(auto) GetOrEmplace(TArgs&&... args) const
    {
        return m_registry->GetOrEmplace<TComponent>(m_handle, std::forward<TArgs>(args)...);
    }

    template <typename TComponent> TComponent& Get() const { return m_registry->GetComponent<TComponent>(m_handle); }

    template <typename TComponent> TComponent* TryGet() { return m_registry->TryGetComponent<TComponent>(m_handle); }

    template <typename TComponent> const TComponent* TryGet() const
    {
        return m_registry->TryGetComponent<TComponent>(m_handle);
    }

    template <typename TComponent> bool Has() const { return m_registry->HasComponent<TComponent>(m_handle); }

    template <typename TComponent> void Remove() const { m_registry->Remove<TComponent>(m_handle); }

    // Get<EventHandlerComponent>().Dispatch(*this, event) -- this entity's
    // own EventHandlerComponent, which every live entity always has.
    template <typename TEvent> void Dispatch(TEvent& event) const
    {
        GetOrEmplace<EventHandlerComponent>().Dispatch(*this, event);
    }

    Registry& GetRegistry() const { return *m_registry; }
    entt::entity Handle() const { return m_handle; }
    bool IsValid() const { return m_registry != nullptr && m_registry->IsValid(m_handle); }

    friend bool operator==(const Entity&, const Entity&) = default;

private:
    Registry* m_registry = nullptr;
    entt::entity m_handle = entt::null;
};

} // namespace psr
