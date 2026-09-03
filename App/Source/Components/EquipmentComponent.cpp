#include "Components/EquipmentComponent.h"

#include "Combat/AttackEvent.h"
#include "Combat/EffectiveStats.h"
#include "Combat/PhotonArtCastEvent.h"
#include "Combat/TechniqueCastEvent.h"
#include "Components/WeaponComponent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/Registry.h"

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
        // Technique casting no longer depends on the equipped weapon (see
        // Technique.h's own doc comment) -- only the effective stats it
        // contributes to damage/heal magnitude still matter here.
        event.attacker_stats = ComputeEffectiveStats(actor, actor.GetRegistry().GetAffixLibrary());
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
    EventHandlerComponent& events = self.GetOrEmplace<EventHandlerComponent>();

    events.Subscribe<BeforeAttackEvent, EquipmentComponent>([](Entity actor, BeforeAttackEvent& event)
                                                            { ContributeAttack(actor, event); });
    events.Subscribe<BeforeTechniqueCastEvent, EquipmentComponent>([](Entity actor, BeforeTechniqueCastEvent& event)
                                                                   { ContributeTechniqueCast(actor, event); });
    events.Subscribe<BeforePhotonArtCastEvent, EquipmentComponent>([](Entity actor, BeforePhotonArtCastEvent& event)
                                                                   { ContributePhotonArtCast(actor, event); });
}

void EquipmentComponent::DetachHandlers(entt::registry& registry, entt::entity entity)
{
    Registry& psr_registry = Registry::FromEntt(registry);
    Entity self(psr_registry, entity);
    // TryGet, not GetOrEmplace: this fires from on_destroy<EquipmentComponent>
    // during whole-entity destruction, where entt::registry::destroy()'s
    // pool-removal order is registration order, not declaration order --
    // EventHandlerComponent may already be gone by the time this runs.
    // GetOrEmplace would resurrect it mid-destroy(), corrupting that pool's
    // bookkeeping for the rest of the entity's (about to be recycled) index.
    EventHandlerComponent* events = self.TryGet<EventHandlerComponent>();
    if (!events)
        return;

    events->Unsubscribe<BeforeAttackEvent, EquipmentComponent>();
    events->Unsubscribe<BeforeTechniqueCastEvent, EquipmentComponent>();
    events->Unsubscribe<BeforePhotonArtCastEvent, EquipmentComponent>();
}

} // namespace psr
