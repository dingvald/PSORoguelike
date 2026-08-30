#include "Combat/StatusEffectApplication.h"

#include "Combat/StatusEffect.h"
#include "Combat/StatusEffectEvent.h"
#include "Combat/StatusEffectLibrary.h"
#include "Combat/StatusEffectType.h"
#include "Components/StatusEffectComponent.h"
#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/HealthComponent.h"

#include <algorithm>
#include <vector>

namespace psr {

namespace {
    bool IsDamageOverTime(StatusEffectType type)
    {
        return type == StatusEffectType::Poison || type == StatusEffectType::Burn;
    }
} // namespace

void ApplyStatusEffect(Entity target, const StatusEffectLibrary& library, std::uint32_t status_effect_id)
{
    const StatusEffect* effect = library.Find(status_effect_id);
    if (!effect)
        return;

    StatusEffectComponent& status = target.GetOrEmplace<StatusEffectComponent>();
    auto it =
        std::find_if(status.active.begin(), status.active.end(), [status_effect_id](const StatusEffectStack& stack)
                     { return stack.status_effect_id == status_effect_id; });
    if (it == status.active.end())
        status.active.push_back(StatusEffectStack{status_effect_id, 1, effect->duration});
    else
    {
        ++it->stacks;
        it->remaining_duration = effect->duration; // refresh, per the user's explicit stacking answer
    }

    AfterStatusEffectsChangedEvent changed;
    target.Dispatch(changed);
}

void TickStatusEffects(Entity actor, const StatusEffectLibrary& library)
{
    StatusEffectComponent* status = actor.TryGet<StatusEffectComponent>();
    if (!status || status->active.empty())
        return;

    // Snapshot before dealing any damage -- a lethal DoT tick destroys actor
    // mid-loop (via IncomingDamageEvent -> HealthSystem -> DeathEvent ->
    // DeathSystem), at which point re-touching actor/status would be
    // invalid; the snapshot lets every remaining stack still get evaluated
    // fairly rather than depending on iteration order over a container that
    // might be mutated out from under it.
    const std::vector<StatusEffectStack> stacks = status->active;
    for (const StatusEffectStack& stack : stacks)
    {
        const StatusEffect* effect = library.Find(stack.status_effect_id);
        if (!effect || !IsDamageOverTime(effect->type))
            continue;

        const int damage = effect->magnitude * stack.stacks;
        if (damage <= 0)
            continue;

        BeforeDamageEvent before{actor, damage};
        actor.Dispatch(before);
        const int applied = before.incoming_damage;

        if (!actor.Has<HealthComponent>())
            continue;

        IncomingDamageEvent incoming{actor, applied};
        actor.Dispatch(incoming);

        if (!actor.IsValid())
            return; // actor (and its StatusEffectComponent) no longer exists
    }

    // Decrement/expire against the live component -- re-fetch rather than
    // trusting the snapshot, in case a handler reacting to the damage
    // dispatch above (e.g. a future on-hit cure) already touched it.
    StatusEffectComponent* live = actor.TryGet<StatusEffectComponent>();
    if (!live)
        return;

    for (StatusEffectStack& entry : live->active)
        --entry.remaining_duration;
    std::erase_if(live->active, [](const StatusEffectStack& entry) { return entry.remaining_duration <= 0; });

    AfterStatusEffectsChangedEvent event;
    actor.Dispatch(event);
}

} // namespace psr
