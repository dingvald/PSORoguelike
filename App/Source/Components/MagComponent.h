#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

#include <cstdint>
#include <string>
#include <vector>

namespace psr {

// One food item's per-stat progress reward for this mag species -- authored
// inline on MagComponent, not a separate library indirection, same "nested
// value type inside an authorable vector field" shape as
// DropTableComponent::LootEntry. Deltas can be negative (PSO mag food often
// raises some stats while lowering others).
struct MagFeedResponse
{
    std::uint32_t item_prefab_id = 0;
    int pow = 0;
    int def = 0;
    int dex = 0;
    int mind = 0;

    template <typename V> static void Describe(V& v)
    {
        v.template Field<&MagFeedResponse::item_prefab_id>("item_prefab_id");
        v.template Field<&MagFeedResponse::pow>("pow");
        v.template Field<&MagFeedResponse::def>("def");
        v.template Field<&MagFeedResponse::dex>("dex");
        v.template Field<&MagFeedResponse::mind>("mind");
    }
};

// One evolution rule: if `condition` (a math expression over this mag's
// current stat levels, e.g. "POW + DEX > MIND + DEF" -- see
// MagEvolutionExpression.h) evaluates true, the mag evolves into
// target_prefab_id. MagComponent::evolution_tree is evaluated in authored
// order; the first matching rule wins (see MagFeeding.cpp's TryEvolveMag).
struct MagEvolutionRule
{
    std::string condition;
    std::uint32_t target_prefab_id = 0;

    template <typename V> static void Describe(V& v)
    {
        v.template Field<&MagEvolutionRule::condition>("condition");
        v.template Field<&MagEvolutionRule::target_prefab_id>("target_prefab_id");
    }
};

// A mag companion's growth state (four PSO-style stats -- POW/DEF/DEX/MIND,
// each a level plus progress toward the next level, PSO's own "5 sub-units
// per displayed point" feeding mechanic) and, authored inline per species/
// prefab (same "no separate library" shape as DropTableComponent), its own
// feed-response table and evolution tree. Sync/IQ are informational only for
// now (see docs/GDD.md's Mag section) -- displayed in the Character screen's
// mag panel, but neither currently gates or scales anything.
//
// feed_cooldown_remaining and bob_elapsed are runtime-only (normally left at
// their zero default in authored JSON, the same way e.g. HealthComponent's
// current_hp is both an authorable starting value and a runtime-mutated one)
// -- still schema-registered/clone-eligible for uniformity with every other
// flat component in this codebase.
struct MagComponent
{
    int pow_level = 0;
    int pow_progress = 0;
    int def_level = 0;
    int def_progress = 0;
    int dex_level = 0;
    int dex_progress = 0;
    int mind_level = 0;
    int mind_progress = 0;

    int iq = 0;
    float sync = 0.0f;

    int feed_cooldown_turns = 50;
    int feed_cooldown_remaining = 0;

    float bob_elapsed = 0.0f;

    std::vector<MagFeedResponse> feed_response;
    std::vector<MagEvolutionRule> evolution_tree;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<MagComponent>("mag")
            .Data<&MagComponent::pow_level>("pow_level")
            .Data<&MagComponent::pow_progress>("pow_progress")
            .Data<&MagComponent::def_level>("def_level")
            .Data<&MagComponent::def_progress>("def_progress")
            .Data<&MagComponent::dex_level>("dex_level")
            .Data<&MagComponent::dex_progress>("dex_progress")
            .Data<&MagComponent::mind_level>("mind_level")
            .Data<&MagComponent::mind_progress>("mind_progress")
            .Data<&MagComponent::iq>("iq")
            .Data<&MagComponent::sync>("sync")
            .Data<&MagComponent::feed_cooldown_turns>("feed_cooldown_turns")
            .Data<&MagComponent::feed_cooldown_remaining>("feed_cooldown_remaining")
            .Data<&MagComponent::feed_response>("feed_response")
            .Data<&MagComponent::evolution_tree>("evolution_tree");
    }
};

// A mag's overall level, derived rather than stored: the sum of its four
// stat levels, so feeding a stat to its next level always raises this too.
inline int MagLevel(const MagComponent& mag)
{
    return mag.pow_level + mag.def_level + mag.dex_level + mag.mind_level;
}

} // namespace psr
