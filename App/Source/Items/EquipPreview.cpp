#include "Items/EquipPreview.h"

#include "Combat/EffectiveStats.h"
#include "Components/EquipmentComponent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/Registry.h"
#include "Items/Equip.h"

namespace psr {

namespace {
    StatsComponent Subtract(const StatsComponent& after, const StatsComponent& before)
    {
        StatsComponent delta;
        delta.atp = after.atp - before.atp;
        delta.ata = after.ata - before.ata;
        delta.mst = after.mst - before.mst;
        delta.dfp = after.dfp - before.dfp;
        delta.evp = after.evp - before.evp;
        delta.lck = after.lck - before.lck;
        return delta;
    }
} // namespace

std::optional<StatsComponent> ComputeEquipStatDelta(Registry& registry, entt::entity actor, entt::entity item,
                                                    const AffixLibrary& affixes)
{
    const std::optional<EquipmentSlot> slot = ResolveEquipSlot(registry, item);
    if (!slot)
        return std::nullopt;

    EquipmentComponent* equipment = registry.TryGetComponent<EquipmentComponent>(actor);
    if (!equipment)
        return std::nullopt;

    entt::entity& slot_ref = SlotRef(*equipment, *slot);

    Entity self(registry, actor);
    const StatsComponent before = ComputeEffectiveStats(self, affixes);

    const entt::entity original = slot_ref;
    slot_ref = item;
    const StatsComponent after = ComputeEffectiveStats(self, affixes);
    slot_ref = original;

    return Subtract(after, before);
}

std::optional<StatsComponent> ComputeUnequipStatDelta(Registry& registry, entt::entity actor, EquipmentSlot slot,
                                                       const AffixLibrary& affixes)
{
    EquipmentComponent* equipment = registry.TryGetComponent<EquipmentComponent>(actor);
    if (!equipment)
        return std::nullopt;

    entt::entity& slot_ref = SlotRef(*equipment, slot);

    Entity self(registry, actor);
    const StatsComponent before = ComputeEffectiveStats(self, affixes);

    const entt::entity original = slot_ref;
    slot_ref = entt::null;
    const StatsComponent after = ComputeEffectiveStats(self, affixes);
    slot_ref = original;

    return Subtract(after, before);
}

} // namespace psr
