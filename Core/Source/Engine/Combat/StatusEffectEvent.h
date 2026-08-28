#pragma once

namespace psr {

// Dispatched by ApplyStatusEffect/TickStatusEffects (StatusEffectApplication.h)
// to the affected entity's own EventHandlerComponent whenever its active
// stack list actually changes (a stack added/incremented, a tick applied, an
// effect expired) -- mirrors AfterWaitEvent's empty-struct shape. A UI bridge
// (see CombatLogBridge) subscribes to react with a fresh HUD/world-marker
// snapshot; nothing else needs to know why the list changed, only that it did.
struct AfterStatusEffectsChangedEvent
{
};

} // namespace psr
