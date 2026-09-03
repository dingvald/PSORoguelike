#pragma once

#include "Engine/ECS/Entity.h"

#include <random>

namespace psr {

class Registry;
class Grid;
class MessageBus;
struct AfterDamageEvent;

// Rolls loot when the player lands a killing blow: subscribed only on the
// player entity (AfterDamageEvent is dispatched at the attacker, see
// DamageEvent.h, so subscribing anyone else would also catch an enemy
// defeating the player or another enemy). Reads the defeated entity's own
// DropTableComponent (the table is authored directly on the prefab, no
// separate library lookup), rolls via DropTableRoller, and spawns the
// result as a ground entity at the defeated entity's own tile -- an item
// prefab for Kind::Item, or the fixed "meseta" prefab (its
// CurrencyPickupComponent::amount overwritten with the roll) for
// Kind::Meseta. Either way it's just an ItemComponent-tagged ground entity
// that PickupAction resolves like any other pickup -- Meseta is credited to
// CurrencyComponent at pickup time, not here. No-ops silently if the
// defeated entity carries no DropTableComponent (most enemies drop
// nothing).
class LootDropSystem
{
public:
    LootDropSystem(Registry& registry, Grid& grid, MessageBus& message_bus, std::mt19937& rng);

    // Subscribed handler captures this instance's address -- neither copying
    // nor moving would keep it valid, same rationale as CombatLogBridge's
    // identical restriction.
    LootDropSystem(const LootDropSystem&) = delete;
    LootDropSystem& operator=(const LootDropSystem&) = delete;
    LootDropSystem(LootDropSystem&&) = delete;
    LootDropSystem& operator=(LootDropSystem&&) = delete;

    // Wires the player's EventHandlerComponent to this instance. Call once,
    // for the player only -- see the class doc comment for why.
    void Subscribe(Entity player);

private:
    void OnDamage(Entity player, AfterDamageEvent& event);

    Registry* m_registry;
    Grid* m_grid;
    MessageBus* m_message_bus;
    std::mt19937* m_rng;
};

} // namespace psr
