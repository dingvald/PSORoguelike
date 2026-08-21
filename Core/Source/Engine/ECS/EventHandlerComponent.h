#pragma once

#include <algorithm>
#include <any>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace psr {

class Entity;

// Per-entity event dispatcher -- the base mode of communication within an
// entity (a component reacting to another component's event) and between
// entities (e.g. a melee-attack action dispatching an apply-damage event at
// a target). Always the first component on any live entity (see
// Registry::CreateEntity) and never removed except as part of whole-entity
// destruction (see Registry::Remove's static_assert).
//
// Handlers are keyed by (event type, owning component type) rather than an
// opaque subscription handle: a component's own AttachHandlers/
// DetachHandlers (wired via Registry::BindComponentEvents to entt's
// on_construct/on_destroy signals) always knows both types statically, so
// Unsubscribe never needs anything else to identify what to remove.
class EventHandlerComponent
{
public:
    template <typename TEvent, typename TOwner>
    void Subscribe(std::function<void(Entity, TEvent&)> handler, int priority = 0)
    {
        std::vector<HandlerEntry>& handlers = m_handlers[typeid(TEvent)];

        // Kept sorted by ascending priority on insert so Dispatch can just
        // iterate in order -- e.g. an armor-mitigation handler at priority 0
        // must run before a health component's damage-application handler at
        // priority 100, regardless of which component was added to the
        // entity first.
        auto insert_at = std::find_if(handlers.begin(), handlers.end(),
                                      [priority](const HandlerEntry& entry) { return entry.priority > priority; });
        handlers.insert(insert_at, HandlerEntry{typeid(TOwner), priority, std::move(handler)});
    }

    template <typename TEvent, typename TOwner> void Unsubscribe()
    {
        auto bucket = m_handlers.find(typeid(TEvent));
        if (bucket == m_handlers.end())
            return;

        std::erase_if(bucket->second, [](const HandlerEntry& entry) { return entry.owner == typeid(TOwner); });
    }

    template <typename TEvent> void Dispatch(Entity self, TEvent& event) const
    {
        auto bucket = m_handlers.find(typeid(TEvent));
        if (bucket == m_handlers.end())
            return;

        // Copy the handler list before invoking: a handler may itself
        // Subscribe/Unsubscribe (e.g. a one-shot buff removing itself once
        // triggered), which would otherwise invalidate this iteration.
        std::vector<HandlerEntry> handlers = bucket->second;
        for (const HandlerEntry& entry : handlers)
            std::any_cast<const std::function<void(Entity, TEvent&)>&>(entry.callback)(self, event);
    }

private:
    struct HandlerEntry
    {
        std::type_index owner;
        int priority;
        std::any callback;
    };

    std::unordered_map<std::type_index, std::vector<HandlerEntry>> m_handlers;
};

} // namespace psr
