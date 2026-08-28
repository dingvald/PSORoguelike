#pragma once

#include "Components/EquipmentComponent.h"
#include "Engine/Combat/DeathSystem.h"
#include "Engine/Combat/HealthSystem.h"
#include "Engine/Combat/StatusEffectLibrary.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/ECS/StatusEffectComponent.h"
#include "Engine/ECS/TPComponent.h"
#include "Engine/Items/AffixLibrary.h"
#include "Engine/World/Grid.h"

namespace psr {

// Wires the same component-event plumbing RegisterComponents.cpp /
// GameplayLayer::OnAttach set up for real gameplay -- BindComponentEvents for
// every component with its own AttachHandlers, BindSystemEvents for
// HealthSystem/DeathSystem, plus SetAffixLibrary/SetStatusEffectLibrary/
// SetGrid -- so Action tests exercise the same BeforeAttackEvent/
// BeforeTechniqueCastEvent/BeforePhotonArtCastEvent contribution path and
// IncomingDamageEvent/DeathEvent damage-application path production code
// uses, not a hand-rolled substitute. Call before any entity that could
// carry EquipmentComponent/TPComponent/StatusEffectComponent/HealthComponent
// is created. affixes/status_effects/grid must outlive registry's use of
// them (same contract as Registry::SetAffixLibrary). GetStatusEffectLibrary()
// is called unconditionally on every landed hit (see AttackAction/
// PhotonArtAction/TechniqueAction's MaybeApplyElementalStatus call) and by
// Shock's cancellation check, so every combat-action test needs this even
// when it exercises no status effect directly -- a missing
// SetStatusEffectLibrary() call asserts, per
// Registry::GetStatusEffectLibrary's own doc comment.
inline void SetUpCombatRegistry(Registry& registry, Grid& grid, const AffixLibrary& affixes,
                                const StatusEffectLibrary& status_effects)
{
    registry.BindComponentEvents<EquipmentComponent>();
    registry.BindComponentEvents<TPComponent>();
    registry.BindComponentEvents<StatusEffectComponent>();
    registry.BindSystemEvents<HealthComponent, HealthSystem>();
    registry.BindSystemEvents<HealthComponent, DeathSystem>();
    registry.SetAffixLibrary(affixes);
    registry.SetStatusEffectLibrary(status_effects);
    registry.SetGrid(grid);
}

} // namespace psr
