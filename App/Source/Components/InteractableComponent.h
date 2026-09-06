#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/TypeReflection.h"

#include <array>
#include <string_view>
#include <utility>

namespace psr {

// What walking onto this entity's tile and pressing Space opens, in the hub
// (see GameplayLayer's Space-key interception and Hub/HubInteraction.h). A
// hub-specific, structural gameplay concept (not open-ended theme content),
// so a fixed enum rather than a NameId, same reasoning ArmorSlot's own doc
// comment gives.
enum class InteractionType
{
    Shop,
    Storage,
    MissionSelect
};

template <> struct EnumNames<InteractionType>
{
    static constexpr std::array<std::pair<std::string_view, InteractionType>, 3> kValues{{
        {"shop", InteractionType::Shop},
        {"storage", InteractionType::Storage},
        {"mission_select", InteractionType::MissionSelect},
    }};
};

// Marks a placed hub entity (shopkeeper, storage terminal, teleprompter) as
// something the player can open by walking onto its tile and pressing Space
// -- see Hub/HubInteraction.h's FindInteractableAt. Deliberately doesn't
// carry BlocksMovementComponent itself (that's an independent, per-prefab
// choice); this project's hub entities are authored without it so the
// player walks onto them rather than bumping into them.
struct InteractableComponent
{
    InteractionType interaction_type = InteractionType::Shop;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<InteractableComponent>("interactable").Data<&InteractableComponent::interaction_type>(
            "interaction_type");
    }
};

} // namespace psr
