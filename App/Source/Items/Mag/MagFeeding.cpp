#include "Items/Mag/MagFeeding.h"

#include "Components/MagComponent.h"
#include "Components/RenderableComponent.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/ECS/Registry.h"
#include "Items/Mag/MagEvolutionExpression.h"

#include <optional>

namespace psr {

namespace {

    void ApplyStatDelta(int& level, int& progress, int delta)
    {
        int total = level * kMagPointsPerLevel + progress + delta;
        if (total < 0)
            total = 0;
        level = total / kMagPointsPerLevel;
        progress = total % kMagPointsPerLevel;
    }

    // Re-syncs mag_entity's config (feed_cooldown_turns/feed_charges/
    // feed_response/evolution_tree) and visual/species identity
    // (RenderableComponent/PrefabIdComponent) against target_prefab_id's own
    // template -- a field-by-field copy, not CopyFromPrefab<MagComponent>'s
    // blind whole-component clone, so stat levels/progress/iq/sync/
    // feed_cooldown_remaining/feed_charges_used/bob_elapsed survive the
    // evolution untouched.
    void EvolveMag(Registry& registry, entt::entity mag_entity, std::uint32_t target_prefab_id)
    {
        MagComponent& mag = registry.GetComponent<MagComponent>(mag_entity);
        const MagComponent& target_template = registry.GetPrefabComponent<MagComponent>(target_prefab_id);

        mag.feed_cooldown_turns = target_template.feed_cooldown_turns;
        mag.feed_charges = target_template.feed_charges;
        mag.feed_response = target_template.feed_response;
        mag.evolution_tree = target_template.evolution_tree;

        registry.CopyFromPrefab<RenderableComponent>(mag_entity, target_prefab_id);
        registry.CopyFromPrefab<PrefabIdComponent>(mag_entity, target_prefab_id);
    }

    void TryEvolveMag(Registry& registry, entt::entity mag_entity)
    {
        const MagComponent& mag = registry.GetComponent<MagComponent>(mag_entity);

        // Read the whole evolution_tree to a decision (which prefab, if any)
        // before mutating anything -- EvolveMag reassigns
        // MagComponent::evolution_tree itself, which would otherwise
        // invalidate this loop's own range mid-iteration.
        std::optional<std::uint32_t> target_prefab_id;
        for (const MagEvolutionRule& rule : mag.evolution_tree)
        {
            if (EvaluateMagEvolutionCondition(rule.condition, mag))
            {
                target_prefab_id = rule.target_prefab_id;
                break;
            }
        }

        if (target_prefab_id)
            EvolveMag(registry, mag_entity, *target_prefab_id);
    }

} // namespace

bool ApplyMagFood(Registry& registry, entt::entity mag_entity, std::uint32_t item_prefab_id)
{
    MagComponent* mag = registry.TryGetComponent<MagComponent>(mag_entity);
    if (!mag)
        return false;

    const MagFeedResponse* response = nullptr;
    for (const MagFeedResponse& entry : mag->feed_response)
    {
        if (entry.item_prefab_id == item_prefab_id)
        {
            response = &entry;
            break;
        }
    }
    if (!response)
        return false;

    ApplyStatDelta(mag->pow_level, mag->pow_progress, response->pow);
    ApplyStatDelta(mag->def_level, mag->def_progress, response->def);
    ApplyStatDelta(mag->dex_level, mag->dex_progress, response->dex);
    ApplyStatDelta(mag->mind_level, mag->mind_progress, response->mind);
    ++mag->iq;

    TryEvolveMag(registry, mag_entity);
    return true;
}

void RegisterMagFeed(MagComponent& mag)
{
    if (mag.feed_charges_used == 0)
        mag.feed_cooldown_remaining = mag.feed_cooldown_turns;
    ++mag.feed_charges_used;
}

void TickMagFeedCooldowns(Registry& registry)
{
    registry.Each<MagComponent>(
        [](entt::entity, MagComponent& mag)
        {
            if (mag.feed_cooldown_remaining <= 0)
                return;

            --mag.feed_cooldown_remaining;
            if (mag.feed_cooldown_remaining == 0)
                mag.feed_charges_used = 0;
        });
}

} // namespace psr
