#include "Combat/EffectiveStats.h"

#include "Components/EquipmentComponent.h"
#include "Components/WeaponComponent.h"
#include "Engine/ECS/Registry.h"

#include <array>
#include <cstddef>

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

    // Shared by ComputeEffectiveStats and ComputeEffectiveStatsWithSlotOverride
    // -- everything past "here's the base stats and the 5 slot occupants" is
    // identical between a real loadout and a hypothetical one.
    StatsComponent SumLoadout(Registry& registry, const AffixLibrary& affixes, const StatsComponent& base,
                              const std::array<entt::entity, 5>& slots)
    {
        StatsComponent total = base;
        for (entt::entity item : slots)
            AddEquippedStats(total, registry, item);

        const entt::entity weapon = slots[static_cast<std::size_t>(EquipmentSlot::Weapon)];
        if (weapon != entt::null)
        {
            if (const WeaponComponent* weapon_component = registry.TryGetComponent<WeaponComponent>(weapon))
            {
                AddAffixBonus(total, affixes, weapon_component->prefix_affix_id);
                AddAffixBonus(total, affixes, weapon_component->suffix_affix_id);
            }
        }

        return total;
    }

    StatsComponent BaseStats(Entity actor)
    {
        if (const StatsComponent* base = actor.TryGet<StatsComponent>())
            return *base;
        return StatsComponent{};
    }

    std::array<entt::entity, 5> LoadoutSlots(Entity actor)
    {
        const EquipmentComponent* equipment = actor.TryGet<EquipmentComponent>();
        if (!equipment)
            return {entt::null, entt::null, entt::null, entt::null, entt::null};
        return {equipment->weapon, equipment->head, equipment->torso, equipment->hands, equipment->legs};
    }
} // namespace

StatsComponent ComputeEffectiveStats(Entity actor, const AffixLibrary& affixes)
{
    return SumLoadout(actor.GetRegistry(), affixes, BaseStats(actor), LoadoutSlots(actor));
}

StatsComponent ComputeEffectiveStatsWithSlotOverride(Entity actor, const AffixLibrary& affixes, EquipmentSlot slot,
                                                     entt::entity replacement)
{
    std::array<entt::entity, 5> slots = LoadoutSlots(actor);
    slots[static_cast<std::size_t>(slot)] = replacement;
    return SumLoadout(actor.GetRegistry(), affixes, BaseStats(actor), slots);
}

} // namespace psr
