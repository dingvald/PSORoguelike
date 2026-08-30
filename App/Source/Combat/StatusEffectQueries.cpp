#include "Combat/StatusEffectQueries.h"

#include "Combat/StatusEffect.h"
#include "Combat/StatusEffectLibrary.h"
#include "Components/StatusEffectComponent.h"

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
