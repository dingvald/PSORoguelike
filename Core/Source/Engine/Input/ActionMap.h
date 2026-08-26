#pragma once

#include "Engine/Actions/IAction.h"

#include <memory>
#include <unordered_map>

namespace psr {

// Generic key -> IAction lookup. TKey is the caller's own type (e.g. an SDL
// keycode int) -- ActionMap knows nothing about SDL or any particular game's
// input scheme. Bound actions are owned here and reused across every input
// that resolves to them, so an IAction bound this way must be stateless
// w.r.t. the actor it's later Perform()'d against (see IAction.h);
// Resolve() hands out a non-owning observer, not a fresh instance.
template <typename TKey> class ActionMap
{
public:
    void Bind(TKey key, std::unique_ptr<IAction> action) { m_bindings[key] = std::move(action); }

    IAction* Resolve(TKey key) const
    {
        auto it = m_bindings.find(key);
        if (it == m_bindings.end())
            return nullptr;
        return it->second.get();
    }

private:
    std::unordered_map<TKey, std::unique_ptr<IAction>> m_bindings;
};

} // namespace psr
