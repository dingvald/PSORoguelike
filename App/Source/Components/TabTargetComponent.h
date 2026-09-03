#pragma once

#include <entt/entt.hpp>

namespace psr {

// The player's current tab-locked target, if any -- see TabTargetSystem.h.
// entt::null means no target, same convention as EquipmentComponent::weapon.
// Deliberately NOT meta-registered -- purely runtime state, same precedent
// as SelectedTargetComponent/EquipmentComponent/TweenComponent.
struct TabTargetComponent
{
    entt::entity target = entt::null;
};

} // namespace psr
