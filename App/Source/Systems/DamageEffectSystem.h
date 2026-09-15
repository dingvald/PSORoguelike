#pragma once

#include "Engine/ECS/Entity.h"

namespace psr {

class Registry;
struct AfterDamageEvent;

// Bridges AfterDamageEvent onto the struck entity's own RenderableComponent:
// flashes it white three times over half a second by temporarily overwriting
// RenderableComponent::color_1/color_2 each frame (via DamageFlashComponent,
// see that struct's own doc comment), restoring the entity's pre-hit colors
// once the flash completes. Deliberately does NOT go through
// VisualEffectSystem like MissFlashEffectSystem/OnHitEffectSystem do -- that
// spawns a separate tile-snapshotted entity that doesn't track a moving/
// tweening target (see VisualEffectSystem's own doc comment on that), whereas
// writing straight to the struck entity's own RenderableComponent always
// renders at wherever that entity itself currently is, tween offset
// included. Needs no constructor dependency (unlike those two systems),
// since it only ever touches the struck entity's own components -- so, like
// HealthSystem/DeathSystem, every method here is static.
class DamageEffectSystem
{
public:
    // Wires one entity's EventHandlerComponent to this bridge. Call once per
    // actor as it's created -- see DamageTextSystem::Subscribe's own doc
    // comment for the full rationale, identical here.
    static void Subscribe(Entity actor);

    // Advances every entity's in-flight DamageFlashComponent by delta_time,
    // writing the flash's current on/off phase straight to
    // RenderableComponent, and restores the entity's baseline color (removing
    // the component) once the flash completes. Call once per frame, same
    // unconditional cadence as VisualEffectSystem::Update/UpdateTweens.
    static void Update(Registry& registry, float delta_time);

private:
    static void OnDamage(Entity actor, AfterDamageEvent& event);
};

} // namespace psr
