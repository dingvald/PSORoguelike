#pragma once

#include "Engine/ECS/Entity.h"

#include <entt/entt.hpp>

#include <string>

namespace psr {

class Registry;
class MessageBus;
class TechniqueLibrary;
class PhotonArtLibrary;
struct AfterDamageEvent;
struct AfterTechniqueCastEvent;
struct AfterPhotonArtCastEvent;
struct AfterItemPickupEvent;

// Bridges per-entity combat events (dispatched via EventHandlerComponent,
// see DamageEvent.h/TechniqueCastEvent.h/PhotonArtCastEvent.h) onto the
// Layer MessageBus as fully-resolved, plain-data messages
// (PlayerStatusMessage/CombatLogEntryMessage) for HudLayer to consume.
// Actions never publish to MessageBus directly -- this is the "some other
// system" that does it on their behalf, so HudLayer never needs a
// Registry/entt::entity/content-library reference of its own.
class CombatLogBridge
{
public:
    CombatLogBridge(Registry& registry, MessageBus& message_bus, const TechniqueLibrary& techniques,
                     const PhotonArtLibrary& photon_arts, entt::entity player);

    // Subscribed handlers capture this instance's address (see Subscribe) --
    // neither copying nor moving would keep them valid, same rationale as
    // TurnCoordinator's identical restriction.
    CombatLogBridge(const CombatLogBridge&) = delete;
    CombatLogBridge& operator=(const CombatLogBridge&) = delete;
    CombatLogBridge(CombatLogBridge&&) = delete;
    CombatLogBridge& operator=(CombatLogBridge&&) = delete;

    // Wires one entity's EventHandlerComponent to this bridge. Call once per
    // actor as it's created. Only the player is wired today (no enemies
    // spawn yet); a future AI/enemy-spawn feature should call this for each
    // new actor too -- an entt on_construct<EnergyComponent> auto-hook
    // (mirroring TurnCoordinator's own OnEnergyConstructed) is the natural
    // follow-up once that's needed, deliberately not built now for a
    // mechanism with exactly one caller.
    void Subscribe(Entity actor);

    // Reads the player's current HealthComponent/TPComponent-or-PPComponent
    // and publishes a fresh PlayerStatusMessage snapshot. Called internally
    // whenever a combat event changes the player's HP/TP/PP; also called by
    // GameplayLayer in response to HudReadyMessage (see that message's own
    // doc comment for why a snapshot can't just be published once up front).
    void PublishPlayerStatus();

private:
    void OnDamage(Entity actor, AfterDamageEvent& event);
    void OnTechniqueCast(Entity actor, AfterTechniqueCastEvent& event);
    void OnPhotonArtCast(Entity actor, AfterPhotonArtCastEvent& event);
    void OnItemPickup(Entity actor, AfterItemPickupEvent& event); // no producer yet, see ItemPickupEvent.h

    std::string DisplayName(entt::entity entity) const;

    Registry* m_registry;
    MessageBus* m_message_bus;
    const TechniqueLibrary* m_techniques;
    const PhotonArtLibrary* m_photon_arts;
    entt::entity m_player;
};

} // namespace psr
