#pragma once

#include "Components/EquipmentComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/ECS/TPComponent.h"
#include "Engine/Items/AffixLibrary.h"

namespace psr {

// Wires the same component-event plumbing RegisterComponents.cpp /
// GameplayLayer::OnAttach set up for real gameplay -- BindComponentEvents for
// every component with its own AttachHandlers, plus SetAffixLibrary -- so
// Action tests exercise the same BeforeAttackEvent/BeforeTechniqueCastEvent/
// BeforePhotonArtCastEvent contribution path production code uses, not a
// hand-rolled substitute. Call before any entity that could carry
// EquipmentComponent/TPComponent is created. affixes must outlive registry's
// use of it (same contract as Registry::SetAffixLibrary).
inline void SetUpCombatRegistry(Registry& registry, const AffixLibrary& affixes)
{
    registry.BindComponentEvents<EquipmentComponent>();
    registry.BindComponentEvents<TPComponent>();
    registry.SetAffixLibrary(affixes);
}

} // namespace psr
