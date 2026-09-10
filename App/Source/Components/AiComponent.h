#pragma once

#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/TypeReflection.h"

#include <array>
#include <string_view>
#include <utility>

namespace psr {

// FleeWhenHit/StationarySpawner/PackFollower/RangedTechAtDistance each read
// one additional, behavior-specific authorable component (SpawnerAiComponent/
// PackFollowerComponent/RangedTechComponent) when present -- see
// EnemyAiSystem.h's own doc comment for what each behavior actually does.
enum class AiBehavior
{
    ChaseAndAttack,
    FleeWhenHit,
    StationarySpawner,
    PackFollower,
    RangedTechAtDistance
};

template <> struct EnumNames<AiBehavior>
{
    static constexpr std::array<std::pair<std::string_view, AiBehavior>, 5> kValues{{
        {"chase_and_attack", AiBehavior::ChaseAndAttack},
        {"flee_when_hit", AiBehavior::FleeWhenHit},
        {"stationary_spawner", AiBehavior::StationarySpawner},
        {"pack_follower", AiBehavior::PackFollower},
        {"ranged_tech_at_distance", AiBehavior::RangedTechAtDistance},
    }};
};

// Marks a non-player entity as AI-driven and selects which behavior
// EnemyAiSystem::Decide should run for it -- entities without this component
// keep TurnCoordinator's own default (Wait every turn). ChaseAndAttack always
// steps toward the nearest PlayerControlledComponent entity within
// detection_range tiles (Manhattan distance); MoveAction's own bump-into-
// hostile fallback is what turns an adjacent step into an attack, so no
// separate attack-range field is needed here.
struct AiComponent
{
    AiBehavior behavior = AiBehavior::ChaseAndAttack;
    int detection_range = 8;

    static void Register(ComponentSchemaRegistrar& reg)
    {
        reg.Component<AiComponent>("ai")
            .Data<&AiComponent::behavior>("behavior")
            .Data<&AiComponent::detection_range>("detection_range");
    }
};

} // namespace psr
