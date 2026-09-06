#pragma once

#include "Components/WeaponComponent.h" // WeaponRangeShape
#include "Engine/Combat/TargetingMode.h"

namespace psr {

class IAction;

// Everything TargetSelectionState needs to run its cursor before actor is a
// specific PhotonArtAction/TechniqueAction is finally resolved -- action
// itself is deliberately generic (an IAction*, not a concrete type),
// mode/shape/range are the art/technique definition's own fields, copied out
// at request time since the state machine has no reason to know which
// concrete content type they came from. Mirrors UnnamedRoguelike's own
// ITargetRequestSink, extended with mode/shape/range: that sibling's single
// hardcoded RangedAttackComponent needed nothing beyond the action pointer
// itself (one fixed max_range check), but PSORoguelike's per-ability
// mode/shape/range table needs those values threaded through explicitly.
struct TargetRequest
{
    IAction* action = nullptr; // non-owning; caller keeps it alive until resolved or cancelled
    TargetingMode mode = TargetingMode::Directional;
    WeaponRangeShape shape = WeaponRangeShape::SingleTarget;
    int range = 1;

    // Mirrors Technique::projectile_speed > 0 / projectile_pierces at request
    // time, purely so TargetSelectionState can preview the same
    // BuildProjectilePath travel/impact tiles TechniqueAction::Perform will
    // actually resolve -- the cast itself still reads Technique directly.
    // Always false for Photon Arts (weapon-style arts never spawn a
    // projectile today).
    bool is_projectile = false;
    bool projectile_pierces = false;
};

class ITargetRequestSink
{
public:
    virtual ~ITargetRequestSink() = default;

    virtual void RequestTargeting(TargetRequest request) = 0;
};

} // namespace psr
