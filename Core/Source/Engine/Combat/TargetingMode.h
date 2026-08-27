#pragma once

#include "Engine/ECS/TypeReflection.h"

#include <array>
#include <string_view>
#include <utility>

namespace psr {

// How a Photon Art/Technique's target is chosen (see TargetSelectionState,
// App-side): Directional aims one of the 4 cardinal-adjacent tiles;
// TargetSquare freely picks any tile within the ability's range; SelfTarget
// always resolves to the caster's own tile. All three drive the same cursor
// state machine -- this only changes which tiles are reachable and how
// direction keys move the cursor.
enum class TargetingMode
{
    Directional,
    TargetSquare,
    SelfTarget
};

template <> struct EnumNames<TargetingMode>
{
    static constexpr std::array<std::pair<std::string_view, TargetingMode>, 3> kValues{{
        {"directional", TargetingMode::Directional},
        {"target_square", TargetingMode::TargetSquare},
        {"self_target", TargetingMode::SelfTarget},
    }};
};

} // namespace psr
