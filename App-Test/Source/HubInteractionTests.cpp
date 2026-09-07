#include "Hub/HubInteraction.h"

#include "Components/InteractableComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/World/Grid.h"

#include <catch2/catch_test_macros.hpp>

using namespace psr;

TEST_CASE("FindInteractableAt returns nullopt for an empty tile", "[HubInteraction]")
{
    Registry registry;
    Grid grid{4, 4};

    REQUIRE_FALSE(FindInteractableAt(registry, grid, Vec2{1, 1}).has_value());
}

TEST_CASE("FindInteractableAt returns nullopt for a tile with only non-interactable entities", "[HubInteraction]")
{
    Registry registry;
    Grid grid{4, 4};

    entt::entity floor = registry.CreateEntity();
    grid.AddEntity(Vec2{1, 1}, floor);

    REQUIRE_FALSE(FindInteractableAt(registry, grid, Vec2{1, 1}).has_value());
}

TEST_CASE("FindInteractableAt returns the InteractionType of a stamped entity", "[HubInteraction]")
{
    Registry registry;
    Grid grid{4, 4};

    entt::entity floor = registry.CreateEntity();
    grid.AddEntity(Vec2{1, 1}, floor);

    entt::entity shopkeeper = registry.CreateEntity();
    registry.Emplace<InteractableComponent>(shopkeeper, InteractableComponent{InteractionType::Shop});
    grid.AddEntity(Vec2{1, 1}, shopkeeper);

    const std::optional<InteractionType> found = FindInteractableAt(registry, grid, Vec2{1, 1});
    REQUIRE(found.has_value());
    REQUIRE(*found == InteractionType::Shop);
}

TEST_CASE("FindInteractableAt resolves each InteractionType", "[HubInteraction]")
{
    Registry registry;
    Grid grid{4, 4};

    entt::entity storage = registry.CreateEntity();
    registry.Emplace<InteractableComponent>(storage, InteractableComponent{InteractionType::Storage});
    grid.AddEntity(Vec2{2, 2}, storage);

    entt::entity teleprompter = registry.CreateEntity();
    registry.Emplace<InteractableComponent>(teleprompter, InteractableComponent{InteractionType::MissionSelect});
    grid.AddEntity(Vec2{3, 3}, teleprompter);

    REQUIRE(*FindInteractableAt(registry, grid, Vec2{2, 2}) == InteractionType::Storage);
    REQUIRE(*FindInteractableAt(registry, grid, Vec2{3, 3}) == InteractionType::MissionSelect);
}

TEST_CASE("FindInteractableAt returns the first interactable when several are stacked", "[HubInteraction]")
{
    Registry registry;
    Grid grid{4, 4};

    entt::entity first = registry.CreateEntity();
    registry.Emplace<InteractableComponent>(first, InteractableComponent{InteractionType::Shop});
    grid.AddEntity(Vec2{1, 1}, first);

    entt::entity second = registry.CreateEntity();
    registry.Emplace<InteractableComponent>(second, InteractableComponent{InteractionType::Storage});
    grid.AddEntity(Vec2{1, 1}, second);

    REQUIRE(*FindInteractableAt(registry, grid, Vec2{1, 1}) == InteractionType::Shop);
}
