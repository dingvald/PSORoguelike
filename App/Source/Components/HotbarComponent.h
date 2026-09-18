#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"

#include <array>
#include <cstdint>

namespace psr {

enum class HotbarSlotType
{
    Empty,
    Technique,
    PhotonArt,
    Item,

    // The equipped weapon's own basic attack (WeaponAttackAction) -- id is
    // unused/0, since "Normal Attack" always means "whatever is currently
    // equipped," not a fixed id. See GameplayLayer::TryActivateSlot and
    // Items/Hotbar.h's AssignAbilityToHotbarSlot.
    NormalAttack,

    // The equipped weapon's own elemental prefix special (WeaponAttackAction
    // constructed with is_special_attack=true) -- id is unused/0, same
    // "always means whatever is currently equipped" convention as
    // NormalAttack. Only assignable/activatable while the equipped weapon has
    // an elemental flavor (WeaponComponent::element != Element::None); see
    // AssignAbilityToHotbarSlot and GameplayLayer::TryActivateSlot.
    SpecialAttack
};

// One quick-use slot: type plus a NameId into TechniqueLibrary/PhotonArtLibrary
// (for Item, the bound consumable's PrefabIdComponent NameId instead -- see
// GameplayLayer::TryActivateSlot's Item case and Items/Hotbar.h).
struct HotbarSlot
{
    HotbarSlotType type = HotbarSlotType::Empty;
    std::uint32_t id = 0;
};

// The player's 10 quick-use slots (keys 1-9, 0). Runtime-only player state,
// populated programmatically in GameplayLayer::OnAttach -- not authorable,
// same treatment as PlayerControlledComponent/PrefabIdComponent, since there
// is no loadout-authoring flow to hand this to content yet.
struct HotbarComponent
{
    static constexpr std::size_t kSlotCount = 10;
    std::array<HotbarSlot, kSlotCount> slots{};

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<HotbarComponent>("hotbar", /*authorable=*/false);
    }
};

} // namespace psr
