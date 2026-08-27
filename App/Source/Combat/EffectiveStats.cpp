#include "Combat/EffectiveStats.h"

#include "Components/EquipmentComponent.h"
#include "Engine/ECS/RareVariantComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/ECS/WeaponComponent.h"

#include <cmath>

namespace psr {

namespace {
    void AddStats(StatsComponent& total, const StatsComponent& bonus)
    {
        total.atp += bonus.atp;
        total.ata += bonus.ata;
        total.mst += bonus.mst;
        total.dfp += bonus.dfp;
        total.evp += bonus.evp;
        total.lck += bonus.lck;
    }

    void ApplyRareVariantMultiplier(StatsComponent& total, float multiplier)
    {
        total.atp = static_cast<int>(std::lround(total.atp * multiplier));
        total.ata = static_cast<int>(std::lround(total.ata * multiplier));
        total.mst = static_cast<int>(std::lround(total.mst * multiplier));
        total.dfp = static_cast<int>(std::lround(total.dfp * multiplier));
        total.evp = static_cast<int>(std::lround(total.evp * multiplier));
        total.lck = static_cast<int>(std::lround(total.lck * multiplier));
    }

    void AddEquippedStats(StatsComponent& total, Registry& registry, entt::entity item)
    {
        if (item == entt::null)
            return;
        if (const StatsComponent* stats = registry.TryGetComponent<StatsComponent>(item))
            AddStats(total, *stats);
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

    const EquipmentComponent* equipment = actor.TryGet<EquipmentComponent>();
    if (!equipment)
        return total;

    Registry& registry = actor.GetRegistry();
    AddEquippedStats(total, registry, equipment->weapon);
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

    if (const RareVariantComponent* rare = actor.TryGet<RareVariantComponent>(); rare && rare->is_rare)
        ApplyRareVariantMultiplier(total, rare->stat_multiplier);

    return total;
}

} // namespace psr
