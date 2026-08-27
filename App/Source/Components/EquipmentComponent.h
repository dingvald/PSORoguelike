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
// Phase A) starts setting it; consumed by M7.1's AttackAction to read the
// wielder's WeaponComponent/StatsComponent.
struct EquipmentComponent
{
    entt::entity weapon = entt::null;
    entt::entity head = entt::null;
    entt::entity torso = entt::null;
    entt::entity hands = entt::null;
    entt::entity legs = entt::null;
};

} // namespace psr
