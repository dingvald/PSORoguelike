#pragma once

#include "Engine/Math/Vec2.h"

namespace psr {

// The tile a TargetSelectionState confirm wrote onto the caster, for
// PhotonArtAction/TechniqueAction's Perform() to read at resolution time --
// mirrors UnnamedRoguelike's own SelectedTargetComponent of the same name/
// role. Deliberately NOT meta-registered (never hand-authored in a prefab,
// never round-tripped through JSON), same precedent as EquipmentComponent/
// TweenComponent: purely runtime state set by TargetSelectionState just
// before TurnCoordinator::SetPendingAction resolves the wrapped action.
struct SelectedTargetComponent
{
    Vec2 tile;
};

} // namespace psr
