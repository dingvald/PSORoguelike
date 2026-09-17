#pragma once

#include <cstdint>

#include <entt/entt.hpp>

namespace psr {

class Registry;

// How much progress fills one stat level -- mirrors PSO's own mag-feeding
// mechanic of 5 internal sub-units per displayed stat point. Not authored;
// a fixed engine constant, so every mag species' MagFeedResponse deltas
// (expressed in these same sub-units) mean the same thing.
constexpr int kMagPointsPerLevel = 5;

// Applies item_prefab_id's MagFeedResponse entry (if mag_entity's
// MagComponent::feed_response has one) to its four stat progress/level
// pairs -- rolling progress into one or more level increments for a large
// delta, and clamping at zero level/progress for a negative one (never
// going below zero). Then checks MagComponent::evolution_tree for a
// satisfied condition (see MagEvolutionExpression.h) and evolves mag_entity
// in place if one matches, preserving every other runtime field. Returns
// false (a no-op) if mag_entity has no MagComponent, or item_prefab_id
// isn't in its feed_response.
bool ApplyMagFood(Registry& registry, entt::entity mag_entity, std::uint32_t item_prefab_id);

// Decrements every live MagComponent::feed_cooldown_remaining above zero by
// one -- called once per elapsed round from GameplayLayer's
// TurnCoordinator::SetOnTurnPassed callback (the same turn-sentinel cadence
// LifetimeSystem::Tick already uses).
void TickMagFeedCooldowns(Registry& registry);

} // namespace psr
