#include "Items/TechniqueLearning.h"

#include "Components/KnownTechniquesComponent.h"

#include <algorithm>

namespace psr {

bool LearnTechnique(Entity actor, std::uint32_t technique_id, int tier)
{
    KnownTechniquesComponent& known = actor.GetOrEmplace<KnownTechniquesComponent>();

    auto it = std::find_if(known.known.begin(), known.known.end(),
                           [technique_id](const KnownTechniqueEntry& entry)
                           { return entry.technique_id == technique_id; });
    if (it != known.known.end())
        it->tier = std::max(it->tier, tier);
    else
        known.known.push_back(KnownTechniqueEntry{technique_id, tier});

    return true;
}

} // namespace psr
