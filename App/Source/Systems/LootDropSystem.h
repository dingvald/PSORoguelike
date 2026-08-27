#pragma once

#include "Engine/ECS/Registry.h"
#include "Engine/Items/DropTableLibrary.h"
#include "Engine/World/Grid.h"

#include <entt/entt.hpp>

#include <random>

namespace psr {

// Rolls and applies loot whenever a HealthComponent-bearing entity is
// destroyed. Fully decoupled from AttackAction/PhotonArtAction/
// TechniqueAction: none of them need to know loot exists, since destroying
// an entity (however it dies) is what triggers this.
//
// Hooks entt's *entity-level* destroy signal (registry.on_destroy<entt::
// entity>(), via Registry::OnDestroy<entt::entity, ...>) rather than a
// per-component one (Registry::OnDestroy<HealthComponent, ...>) -- entt
// fires this before any of the dying entity's component pools are cleared,
// so every sibling component (Position, DropTableComponent, ...) is still
// safely readable here. A per-component on_destroy<T> listener only
// guarantees T's own pool is intact at that point; it says nothing about
// whether some *other* component on the same entity has already been torn
// down earlier in the same destroy() call, which is exactly why
// AttackAction/PhotonArtAction/TechniqueAction already capture whatever
// they need (the tile, for Grid removal) *before* calling DestroyEntity
// rather than reading it back afterward or from within a destroy signal.
//
// Meseta is credited directly onto whichever entity has both MesetaComponent
// and PlayerControlledComponent (single-player, per docs/GDD.md's scope
// decision -- no need to track "who landed the killing blow"). A rolled item
// entry (see Engine/Items/DropResolution.h) is spawned as a live
// Grid-resident entity at the dead entity's own tile, tagged
// GroundItemComponent -- visibly on the ground, matching PSO's "walk over a
// dropped item," but not yet pickup-able: no inventory/pickup system exists
// yet to consume it (M8.1's own deferral), same "engine lands, interaction
// UI deferred" precedent M7.1's combat math already set for HP bars/combat
// log.
class LootDropSystem
{
public:
    LootDropSystem(Registry& registry, Grid& grid, const DropTableLibrary& drop_tables, std::mt19937& rng);
    ~LootDropSystem();

    // Binds a registry entity-lifecycle listener to this instance's own
    // address (same reasoning as TurnCoordinator) -- must be constructed in
    // place, never copied or moved.
    LootDropSystem(const LootDropSystem&) = delete;
    LootDropSystem& operator=(const LootDropSystem&) = delete;
    LootDropSystem(LootDropSystem&&) = delete;
    LootDropSystem& operator=(LootDropSystem&&) = delete;

private:
    void OnEntityDestroyed(entt::registry& registry, entt::entity entity);

    Registry* m_registry;
    Grid* m_grid;
    const DropTableLibrary* m_drop_tables;
    std::mt19937* m_rng;
};

} // namespace psr
