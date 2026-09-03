#include "Combat/EffectiveStats.h"

#include "Combat/StatusEffectQueries.h"
#include "Combat/StatusEffectType.h"
#include "Components/EquipmentComponent.h"
#include "Components/StatusEffectComponent.h"
#include "Components/WeaponComponent.h"
#include "Engine/ECS/Registry.h"

namespace psr {

namespace {
    constexpr int kGrindAtpPerLevel = 2;
    constexpr float kShockEvpMultiplier = 0.85f;
    constexpr float kFreezeEvpMultiplier = 0.7f;
    constexpr float kShockAndFreezeEvpMultiplier = 0.55f;

    void AddStats(StatsComponent& total, const StatsComponent& bonus)
    {
        total.atp += bonus.atp;
        total.ata += bonus.ata;
        total.mst += bonus.mst;
        total.dfp += bonus.dfp;
        total.evp += bonus.evp;
        total.lck += bonus.lck;
    }

    void AddEquippedStats(StatsComponent& total, Registry& registry, entt::entity item)
    {
        if (item == entt::null)
            return;
        if (const StatsComponent* stats = registry.TryGetComponent<StatsComponent>(item))
            AddStats(total, *stats);
    }

    // Applies the equipped weapon's own StatsComponent bonus (as above) plus
    // its dead-until-now grind_level as flat ATP (PSO: +2 ATP per grind).
    void AddWeaponStats(StatsComponent& total, Registry& registry, entt::entity weapon)
    {
        AddEquippedStats(total, registry, weapon);
        if (weapon == entt::null)
            return;
        if (const WeaponComponent* weapon_component = registry.TryGetComponent<WeaponComponent>(weapon))
            total.atp += weapon_component->grind_level * kGrindAtpPerLevel;
    }

    // PSO: Shocked/Paralyzed EVP x0.85, Frozen x0.7, both x0.55 (a distinct
    // authored value, not the product of the two).
    void ApplyStatusEvpModifier(StatsComponent& total, Entity actor)
    {
        if (!actor.Has<StatusEffectComponent>())
            return;
        const StatusEffectLibrary& status_effects = actor.GetRegistry().GetStatusEffectLibrary();
        const bool shocked = HasActiveStatusType(actor, status_effects, StatusEffectType::Shock);
        const bool frozen = HasActiveStatusType(actor, status_effects, StatusEffectType::Freeze);
        if (shocked && frozen)
            total.evp = static_cast<int>(static_cast<float>(total.evp) * kShockAndFreezeEvpMultiplier);
        else if (frozen)
            total.evp = static_cast<int>(static_cast<float>(total.evp) * kFreezeEvpMultiplier);
        else if (shocked)
            total.evp = static_cast<int>(static_cast<float>(total.evp) * kShockEvpMultiplier);
    }

    void AddAffixBonus(StatsComponent& total, const AffixLibrary& affixes, std::uint32_t affix_id)
    {
        if (affix_id == 0)
            return;
        const Affix* affix = affixes.Find(affix_id);
        if (!affix)
            return;

        switch (affix->stat)
        {
        case AffixStat::Atp:
            total.atp += affix->amount;
            break;
        case AffixStat::Ata:
            total.ata += affix->amount;
            break;
        case AffixStat::Mst:
            total.mst += affix->amount;
            break;
        case AffixStat::Dfp:
            total.dfp += affix->amount;
            break;
        case AffixStat::Evp:
            total.evp += affix->amount;
            break;
        case AffixStat::Lck:
            total.lck += affix->amount;
            break;
        }
    }
} // namespace

StatsComponent ComputeEffectiveStats(Entity actor, const AffixLibrary& affixes)
{
    StatsComponent total;
    if (const StatsComponent* base = actor.TryGet<StatsComponent>())
        total = *base;

    if (const EquipmentComponent* equipment = actor.TryGet<EquipmentComponent>())
    {
        Registry& registry = actor.GetRegistry();
        AddWeaponStats(total, registry, equipment->weapon);
        AddEquippedStats(total, registry, equipment->head);
        AddEquippedStats(total, registry, equipment->torso);
        AddEquippedStats(total, registry, equipment->hands);
        AddEquippedStats(total, registry, equipment->legs);

        if (equipment->weapon != entt::null)
        {
            if (const WeaponComponent* weapon = registry.TryGetComponent<WeaponComponent>(equipment->weapon))
            {
                AddAffixBonus(total, affixes, weapon->prefix_affix_id);
                AddAffixBonus(total, affixes, weapon->suffix_affix_id);
            }
        }
    }

    ApplyStatusEvpModifier(total, actor);
    return total;
}

} // namespace psr
