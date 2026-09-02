#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

namespace psr {

// The player's progression state -- level and lifetime total EXP earned
// (current_exp never resets on level-up; "EXP to next level" is always
// computed on demand via LevelingMath::ExpRequiredForLevel, never cached
// here). Not authorable -- assigned programmatically when the player is
// spawned (GameplayLayer::LoadNewGame), same reasoning as
// PlayerControlledComponent: no character-creation flow exists yet to
// author a starting level from content.
struct LevelComponent
{
    int level = 1;
    int current_exp = 0;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<LevelComponent>("level", /*authorable=*/false)
            .Data<&LevelComponent::level>("level")
            .Data<&LevelComponent::current_exp>("current_exp");
    }
};

} // namespace psr
