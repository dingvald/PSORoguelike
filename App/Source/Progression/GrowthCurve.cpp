#include "Progression/GrowthCurve.h"

#include <algorithm>

namespace psr {

const GrowthCurveLevel* GrowthCurve::Find(int level) const
{
    auto it = std::find_if(levels.begin(), levels.end(),
                           [level](const GrowthCurveLevel& entry) { return entry.level == level; });
    return it == levels.end() ? nullptr : &*it;
}

} // namespace psr
