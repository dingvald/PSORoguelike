#pragma once

#include "Engine/ECS/Registry.h"

#include <entt/entt.hpp>

#include <string>

namespace psr {

// A label for entity to show in UI/combat-log text: "Player" for player,
// else its prefab's own authored id string (via PrefabIdComponent +
// NameIdRegistry::Find), else the literal fallback "something" if neither
// resolves. Shared by CombatLogBridge's combat-log lines and HudLayer's
// target panel -- there is no per-entity "display name" component anywhere
// in this codebase; a prefab's own id is the best available label.
std::string DisplayName(Registry& registry, entt::entity entity, entt::entity player);

} // namespace psr
