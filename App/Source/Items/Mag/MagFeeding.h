#pragma once

#include <cstdint>

#include <entt/entt.hpp>

namespace psr {

class Registry;
struct MagComponent;

// How much progress (xp) fills one stat level. Not authored; a fixed
// engine constant, so every mag species' MagFeedResponse deltas (expressed
// in these same xp units) mean the same thing.
constexpr int kMagPointsPerLevel = 100;

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

// Call once per successful feed (after ApplyMagFood), independent of it
// since a caller may need to gate/consume inventory in between -- see
// GameplayLayer::OnMagFeedRequested. Starts feed_cooldown_turns ticking on
// the first feed of a charge cycle (feed_charges_used == 0), then increments
// MagComponent::feed_charges_used -- further feeds stay allowed up to
// feed_charges while the cooldown counts down.
void RegisterMagFeed(MagComponent& mag);

// Decrements every live MagComponent::feed_cooldown_remaining above zero by
// one, refilling feed_charges_used back to zero the instant a mag's cooldown
// reaches zero -- called once per elapsed round from GameplayLayer's
// TurnCoordinator::SetOnTurnPassed callback (the same turn-sentinel cadence
// LifetimeSystem::Tick already uses).
void TickMagFeedCooldowns(Registry& registry);

} // namespace psr
