#pragma once

#include <entt/entt.hpp>

namespace psr {

// Which live item entities a character/enemy currently has equipped -- one
// weapon slot plus the four ArmorSlot body locations. Deliberately NOT
// meta-registered (entt::entity has no FieldKind mapping, so it can't go
// through ComponentSchemaRegistrar at all): this is runtime-only state, set
// by game code as items are equipped, never hand-authored in a prefab JSON
// or listed in the editor's Add Component picker. Same precedent as
// TweenComponent. Unpopulated until the debug mission launcher (later in
// Phase A) starts setting it.
struct EquipmentComponent
{
    entt::entity weapon = entt::null;
    entt::entity head = entt::null;
    entt::entity torso = entt::null;
    entt::entity hands = entt::null;
    entt::entity legs = entt::null;

    // Contributes this entity's equipped-weapon data and effective stats to
    // BeforeAttackEvent/BeforeTechniqueCastEvent/BeforePhotonArtCastEvent,
    // dispatched to this same entity by AttackAction/TechniqueAction/
    // PhotonArtAction -- see EquipmentComponent.cpp. Wired via
    // Registry::BindComponentEvents<EquipmentComponent>() in
    // RegisterComponents.cpp, so every entity gets this the moment it gains
    // an EquipmentComponent, with no per-entity opt-in required by whoever
    // equips it.
    static void AttachHandlers(entt::registry& registry, entt::entity entity);
    static void DetachHandlers(entt::registry& registry, entt::entity entity);
};

} // namespace psr
