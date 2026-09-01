#include "Systems/CombatLogBridge.h"

#include "Messages/CombatLogEntryMessage.h"
#include "Messages/PlayerStatusMessage.h"
#include "Messages/StatusEffectsMessage.h"

#include "Combat/PhotonArt.h"
#include "Combat/PhotonArtCastEvent.h"
#include "Combat/PhotonArtLibrary.h"
#include "Combat/StatusEffect.h"
#include "Combat/StatusEffectEvent.h"
#include "Combat/StatusEffectLibrary.h"
#include "Combat/Technique.h"
#include "Combat/TechniqueCastEvent.h"
#include "Combat/TechniqueLibrary.h"
#include "Components/StatusEffectComponent.h"
#include "Components/TPComponent.h"
#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/NameIdRegistry.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Items/ItemDropEvent.h"
#include "Engine/Items/ItemPickupEvent.h"
#include "Engine/Messages/MessageBus.h"

#include <optional>

namespace psr {

CombatLogBridge::CombatLogBridge(Registry& registry, MessageBus& message_bus, const TechniqueLibrary& techniques,
                                 const PhotonArtLibrary& photon_arts, const StatusEffectLibrary& status_effects,
                                 entt::entity player)
    : m_registry(&registry), m_message_bus(&message_bus), m_techniques(&techniques), m_photon_arts(&photon_arts),
      m_status_effects(&status_effects), m_player(player)
{
}

void CombatLogBridge::Subscribe(Entity actor)
{
    EventHandlerComponent& events = actor.GetOrEmplace<EventHandlerComponent>();
    events.Subscribe<AfterDamageEvent, CombatLogBridge>([this](Entity entity, AfterDamageEvent& event)
                                                        { OnDamage(entity, event); });
    events.Subscribe<AfterTechniqueCastEvent, CombatLogBridge>([this](Entity entity, AfterTechniqueCastEvent& event)
                                                               { OnTechniqueCast(entity, event); });
    events.Subscribe<AfterPhotonArtCastEvent, CombatLogBridge>([this](Entity entity, AfterPhotonArtCastEvent& event)
                                                               { OnPhotonArtCast(entity, event); });
    events.Subscribe<AfterItemPickupEvent, CombatLogBridge>([this](Entity entity, AfterItemPickupEvent& event)
                                                            { OnItemPickup(entity, event); });
    events.Subscribe<AfterItemDropEvent, CombatLogBridge>([this](Entity entity, AfterItemDropEvent& event)
                                                          { OnItemDrop(entity, event); });
    events.Subscribe<AfterStatusEffectsChangedEvent, CombatLogBridge>(
        [this](Entity entity, AfterStatusEffectsChangedEvent& event) { OnStatusEffectsChanged(entity, event); });
}

void CombatLogBridge::OnDamage(Entity actor, AfterDamageEvent& event)
{
    const entt::entity target_handle = event.target.Handle();
    const std::string actor_name = DisplayName(actor.Handle());
    const std::string target_name = DisplayName(target_handle);

    m_message_bus->Publish(
        CombatLogEntryMessage{actor_name + " hit " + target_name + " for " + std::to_string(event.amount) + " damage"});
    if (event.target_defeated)
        m_message_bus->Publish(CombatLogEntryMessage{actor_name + " defeated " + target_name});

    if (actor.Handle() == m_player || target_handle == m_player)
        PublishPlayerStatus();
}

void CombatLogBridge::OnTechniqueCast(Entity actor, AfterTechniqueCastEvent& event)
{
    const Technique* technique = m_techniques->Find(event.technique_id);
    const std::string ability_name = technique ? technique->name : std::string("something");
    m_message_bus->Publish(CombatLogEntryMessage{DisplayName(actor.Handle()) + " cast " + ability_name});

    if (actor.Handle() == m_player)
        PublishPlayerStatus();
}

void CombatLogBridge::OnPhotonArtCast(Entity actor, AfterPhotonArtCastEvent& event)
{
    const PhotonArt* art = m_photon_arts->Find(event.photon_art_id);
    const std::string ability_name = art ? art->name : std::string("something");
    m_message_bus->Publish(CombatLogEntryMessage{DisplayName(actor.Handle()) + " used " + ability_name});

    if (actor.Handle() == m_player)
        PublishPlayerStatus();
}

void CombatLogBridge::OnItemPickup(Entity actor, AfterItemPickupEvent& event)
{
    const std::optional<std::string> label = NameIdRegistry::Find(event.item_prefab_id);
    const std::string item_name = label ? *label : std::string("an item");
    m_message_bus->Publish(CombatLogEntryMessage{DisplayName(actor.Handle()) + " picked up " + item_name});
}

void CombatLogBridge::OnItemDrop(Entity actor, AfterItemDropEvent& event)
{
    const std::optional<std::string> label = NameIdRegistry::Find(event.item_prefab_id);
    const std::string item_name = label ? *label : std::string("an item");
    m_message_bus->Publish(CombatLogEntryMessage{DisplayName(actor.Handle()) + " dropped " + item_name});
}

void CombatLogBridge::PublishPlayerStatus()
{
    PlayerStatusMessage status;
    if (const HealthComponent* health = m_registry->TryGetComponent<HealthComponent>(m_player))
    {
        status.current_hp = health->current_hp;
        status.max_hp = health->max_hp;
    }

    if (const TPComponent* tp = m_registry->TryGetComponent<TPComponent>(m_player))
    {
        status.has_secondary = true;
        status.current_secondary = tp->current_tp;
        status.max_secondary = tp->max_tp;
    }

    m_message_bus->Publish(status);
}

void CombatLogBridge::PublishStatusEffects()
{
    StatusEffectsMessage message;
    if (const StatusEffectComponent* status = m_registry->TryGetComponent<StatusEffectComponent>(m_player))
    {
        for (const StatusEffectStack& stack : status->active)
        {
            const StatusEffect* effect = m_status_effects->Find(stack.status_effect_id);
            if (!effect)
                continue;
            message.active.push_back(
                StatusEffectsMessage::ActiveEntry{effect->type, stack.stacks, stack.remaining_duration});
        }
    }

    m_message_bus->Publish(message);
}

void CombatLogBridge::OnStatusEffectsChanged(Entity actor, AfterStatusEffectsChangedEvent& /*event*/)
{
    if (actor.Handle() == m_player)
        PublishStatusEffects();
}

std::string CombatLogBridge::DisplayName(entt::entity entity) const
{
    if (entity == m_player)
        return "Player";

    if (const PrefabIdComponent* prefab_id = m_registry->TryGetComponent<PrefabIdComponent>(entity))
    {
        if (std::optional<std::string> label = NameIdRegistry::Find(prefab_id->value))
            return *label;
    }
    return "something";
}

} // namespace psr
