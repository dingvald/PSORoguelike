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
};

class ITargetRequestSink
{
public:
    virtual ~ITargetRequestSink() = default;

    virtual void RequestTargeting(TargetRequest request) = 0;
};

} // namespace psr
