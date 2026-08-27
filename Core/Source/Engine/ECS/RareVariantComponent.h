#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

namespace psr {

// Marks a prefab as a rare-variant spawn (docs/GDD.md: "a low-probability
// alt-colored variant of a normal spawn, with boosted stats and a
// guaranteed better/exclusive drop"). is_rare gates whether
// stat_multiplier applies (see App/Source/Combat/EffectiveStats.cpp) --
// authored false by default so adding this component to a prefab without
// touching is_rare is inert.
//
// The "alt palette" and "guaranteed... drop" halves of that GDD sentence
// need no engine plumbing of their own here: a rare variant is authored as
// its own separate prefab (e.g. rag_rappy_rare.json, mirroring how a normal
// enemy is authored), so "alt palette" is just that prefab's own
// RenderableComponent authored differently -- not a runtime palette swap of
// the normal spawn -- and "guaranteed... drop" is that same prefab's own
// DropTableComponent (M8.2) pointing at a DropTable with
// boss_guaranteed_rare set. Both are content-authoring choices layered on
// existing mechanisms, not new engine features this component needs to add.
//
// stat_multiplier scales the *computed* combat stats (ATP/ATA/MST/DFP/EVP/
// LCK, via ComputeEffectiveStats) rather than HealthComponent's max_hp:
// unlike those stats, max_hp is never live-computed anywhere in the combat
// pipeline (it's cloned verbatim from the prefab at spawn) -- a rare
// variant's own prefab file authors a higher current_hp/max_hp directly
// instead, the same way its RenderableComponent authors a different palette
// directly, rather than this component needing a second scaling hook.
struct RareVariantComponent
{
    bool is_rare = false;
    float stat_multiplier = 1.0f;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<RareVariantComponent>("rare_variant")
            .Data<&RareVariantComponent::is_rare>("is_rare")
            .Data<&RareVariantComponent::stat_multiplier>("stat_multiplier");
    }
};

} // namespace psr
