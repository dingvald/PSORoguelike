#pragma once

#include "Engine/ECS/Entity.h"

#include <random>

namespace psr {

class Registry;
class Grid;
class MessageBus;
class AffixLibrary;
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
//
// Also runs WeaponBuilder on every dropped item (a no-op for anything
// without a WeaponComponent), so a dropped weapon has a chance at a rolled
// prefix/grind/race bonus before the drop notification below is built --
// see WeaponBuilder.h.
class LootDropSystem
{
public:
    LootDropSystem(Registry& registry, Grid& grid, MessageBus& message_bus, const AffixLibrary& affixes,
                   std::mt19937& rng);

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
    const AffixLibrary* m_affixes;
    std::mt19937* m_rng;
};

} // namespace psr
