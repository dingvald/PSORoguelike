#pragma once

#include "Engine/ECS/Registry.h"

#include <entt/entt.hpp>

#include <string>

namespace psr {

// A label for entity to show in UI/combat-log text: player's own chosen
// NameComponent (see CharacterCreationLayer's Name step) for player, falling
// back to the literal "Player" if unset; else its prefab's own authored id
// string (via PrefabIdComponent + NameIdRegistry::Find), else the literal
// fallback "something" if neither resolves. Shared by CombatLogBridge's
// combat-log lines and HudLayer's target panel.
std::string DisplayName(Registry& registry, entt::entity entity, entt::entity player);

} // namespace psr
