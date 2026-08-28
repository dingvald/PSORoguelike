#include "Engine/Combat/StatusEffectQueries.h"

#include "Engine/Combat/StatusEffect.h"
#include "Engine/Combat/StatusEffectLibrary.h"
#include "Engine/ECS/StatusEffectComponent.h"

namespace psr {

bool HasActiveStatusType(Entity actor, const StatusEffectLibrary& library, StatusEffectType type)
{
    const StatusEffectComponent* status = actor.TryGet<StatusEffectComponent>();
    if (!status)
        return false;

    for (const StatusEffectStack& stack : status->active)
    {
        const StatusEffect* effect = library.Find(stack.status_effect_id);
        if (effect && effect->type == type)
            return true;
    }
    return false;
}

} // namespace psr
