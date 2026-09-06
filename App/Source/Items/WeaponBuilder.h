#pragma once

#include <entt/entt.hpp>

#include <random>

namespace psr {

class Registry;
class AffixLibrary;

// Rolls a freshly-instantiated weapon's procedural bonus attributes -- a
// chance at a prefix (from affixes, picked among its AffixKind::Prefix
// entries), a chance at a grind level, a chance at one race damage bonus.
// Same shape as DropTableRoller's Roll(...): a plain function taking the
// caller's own std::mt19937&, no separate RNG source. Called once, right
// after LootDropSystem instantiates a dropped item -- see its own call site
// for why that's the natural hook point. No-ops silently if item has no
// WeaponComponent, so callers don't need to check first.
//
// Per the current design, every weapon has the same flat chance at each
// bonus regardless of weapon type/rarity -- there's no per-weapon rarity or
// weighting system yet, so these are plain file-local constants (see
// WeaponBuilder.cpp) rather than authored/configurable values.
void RunWeaponBuilder(Registry& registry, entt::entity item, const AffixLibrary& affixes, std::mt19937& rng);

} // namespace psr
