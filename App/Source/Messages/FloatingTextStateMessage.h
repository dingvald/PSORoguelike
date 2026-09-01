#pragma once

#include "Engine/Math/Color.h"

#include <string>
#include <vector>

namespace psr {

// Resolved screen-space snapshot of every currently-active FloatingTextSystem
// instance, published every frame by GameplayLayer (see
// GameplayLayer::PublishFloatingTextState) so HudLayer never needs a
// Camera/Registry reference of its own -- same fully-resolved contract as
// HotbarStateMessage/CharacterScreenMessage.
struct FloatingTextStateMessage
{
    struct Entry
    {
        float screen_x = 0.0f;
        float screen_y = 0.0f;
        std::string text;
        Color color;
    };

    std::vector<Entry> entries;
};

} // namespace psr
