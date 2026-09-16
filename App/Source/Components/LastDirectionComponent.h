#pragma once

#include "Engine/Math/Vec2.h"

#include <entt/entt.hpp>

namespace psr {

// The player's most recent movement or attack direction -- used to position
// the Mag companion on the tile opposite it (see MagCompanion.cpp). Not
// meta/schema-registered (engine-managed runtime state, same precedent as
// EquipmentComponent/TweenComponent -- never hand-authored in a prefab).
// Emplaced only on the player entity (GameplayLayer's spawn code), never on
// enemies. Defaults to south so a freshly spawned player's mag has somewhere
// sensible to sit before the first move/attack.
struct LastDirectionComponent
{
    Vec2 direction{0, 1};

    // Subscribes to AfterMoveEvent/BeforeAttackEvent on this same entity's
    // EventHandlerComponent -- wired via Registry::BindComponentEvents in
    // RegisterComponents.cpp, same idiom as StatusEffectComponent.
    static void AttachHandlers(entt::registry& registry, entt::entity entity);
    static void DetachHandlers(entt::registry& registry, entt::entity entity);
};

} // namespace psr
