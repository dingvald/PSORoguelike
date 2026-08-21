#pragma once

#include "Engine/Math/Vec2f.h"
#include "Engine/Render/RenderableTile.h"

#include <entt/entt.hpp>

#include <optional>

namespace psr {

// Pure interface: resolves what to draw for a grid-tile entity, if
// anything. Keeps TileRenderer (Core, generic mechanism) decoupled from
// RenderableComponent (App, game content).
class IRenderableLookup
{
public:
    IRenderableLookup() = default;
    virtual ~IRenderableLookup() = default;

    IRenderableLookup(const IRenderableLookup&) = delete;
    IRenderableLookup& operator=(const IRenderableLookup&) = delete;
    IRenderableLookup(IRenderableLookup&&) = delete;
    IRenderableLookup& operator=(IRenderableLookup&&) = delete;

    virtual std::optional<RenderableTile> GetRenderableTile(entt::entity entity) const = 0;

    // Sub-tile render offset (tile-fraction units) to add to entity's draw
    // position -- e.g. driven by a future tween component. Not pure:
    // defaults to no offset so implementations with nothing to animate
    // don't need to override it.
    virtual Vec2f GetRenderOffset(entt::entity /*entity*/) const { return {}; }
};

} // namespace psr
