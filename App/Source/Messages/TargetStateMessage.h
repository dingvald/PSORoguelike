#pragma once

#include <string>

namespace psr {

// The player's current tab-lock target (TabTargetComponent), resolved for
// HudLayer's bottom-left target panel. Plain values only -- no Entity/
// Registry -- published by GameplayLayer::PublishTargetState every frame
// (mirrors FloatingTextStateMessage's every-frame-republish shape, since a
// target's HP can change from any source, not just an event this layer
// already hooks) and once from OnHudReady, same reasoning as
// PlayerStatusMessage's own construction-time publish.
struct TargetStateMessage
{
    bool has_target = false;
    std::string name;
    std::string race_label;

    // Unused (and the panel hidden) if has_target is false.
    int current_hp = 0;
    int max_hp = 0;
};

} // namespace psr
