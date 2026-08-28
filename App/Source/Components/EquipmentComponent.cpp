#include "Components/EquipmentComponent.h"

#include "Combat/EffectiveStats.h"
#include "Engine/Combat/AttackEvent.h"
#include "Engine/Combat/PhotonArtCastEvent.h"
#include "Engine/Combat/TechniqueCastEvent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/ECS/WeaponComponent.h"

#include <algorithm>

namespace psr {

namespace {
    bool Grants(const std::vector<std::uint32_t>& ids, std::uint32_t id)
    {
        return std::find(ids.begin(), ids.end(), id) != ids.end();
    }

    const WeaponComponent* FindEquippedWeapon(Entity actor)
    {
        const EquipmentComponent* equipment = actor.TryGet<EquipmentComponent>();
        if (!equipment || equipment->weapon == entt::null)
            return nullptr;
        return actor.GetRegistry().TryGetComponent<WeaponComponent>(equipment->weapon);
    }

    void ContributeAttack(Entity actor, BeforeAttackEvent& event)
    {
        event.attacker_stats = ComputeEffectiveStats(actor, actor.GetRegistry().GetAffixLibrary());

        const WeaponComponent* weapon = FindEquippedWeapon(actor);
        if (!weapon)
            return;

        event.has_weapon = true;
        event.range_shape = weapon->range_shape;
        event.range = weapon->range;
        event.hits_per_turn = weapon->hits_per_turn;
        event.race_bonuses = weapon->race_bonuses;
        event.element = weapon->element;
        event.status_effect_id = weapon->status_effect_id;
        event.status_chance_percent = weapon->status_chance_percent;
    }

    void ContributeTechniqueCast(Entity actor, BeforeTechniqueCastEvent& event)
    {
        event.attacker_stats = ComputeEffectiveStats(actor, actor.GetRegistry().GetAffixLibrary());

        const WeaponComponent* weapon = FindEquippedWeapon(actor);
        if (!weapon)
            return;

        event.has_weapon = true;
        event.weapon_grants_id = Grants(weapon->technique_ids, event.technique_id);
        event.race_bonuses = weapon->race_bonuses;
    }

    void ContributePhotonArtCast(Entity actor, BeforePhotonArtCastEvent& event)
    {
        event.attacker_stats = ComputeEffectiveStats(actor, actor.GetRegistry().GetAffixLibrary());

        const WeaponComponent* weapon = FindEquippedWeapon(actor);
        if (!weapon)
            return;

        event.has_weapon = true;
        event.weapon_grants_id = Grants(weapon->photon_art_ids, event.photon_art_id);
        event.race_bonuses = weapon->race_bonuses;
        event.element = weapon->element;
        event.status_effect_id = weapon->status_effect_id;
        event.status_chance_percent = weapon->status_chance_percent;
    }
} // namespace

void EquipmentComponent::AttachHandlers(entt::registry& registry, entt::entity entity)
{
    Registry& psr_registry = Registry::FromEntt(registry);
    Entity self(psr_registry, entity);
    EventHandlerComponent& events = self.Get<EventHandlerComponent>();

    events.Subscribe<BeforeAttackEvent, EquipmentComponent>(
        [](Entity actor, BeforeAttackEvent& event) { ContributeAttack(actor, event); });
    events.Subscribe<BeforeTechniqueCastEvent, EquipmentComponent>(
        [](Entity actor, BeforeTechniqueCastEvent& event) { ContributeTechniqueCast(actor, event); });
    events.Subscribe<BeforePhotonArtCastEvent, EquipmentComponent>(
        [](Entity actor, BeforePhotonArtCastEvent& event) { ContributePhotonArtCast(actor, event); });
}

void EquipmentComponent::DetachHandlers(entt::registry& registry, entt::entity entity)
{
    Registry& psr_registry = Registry::FromEntt(registry);
    Entity self(psr_registry, entity);
    EventHandlerComponent& events = self.Get<EventHandlerComponent>();

    events.Unsubscribe<BeforeAttackEvent, EquipmentComponent>();
    events.Unsubscribe<BeforeTechniqueCastEvent, EquipmentComponent>();
    events.Unsubscribe<BeforePhotonArtCastEvent, EquipmentComponent>();
}

} // namespace psr
