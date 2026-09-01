#include "States/AnimationState.h"

#include "Components/TweenComponent.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Messages/MessageBus.h"
#include "Engine/World/Grid.h"
#include "Systems/TurnCoordinator.h"

#include <catch2/catch_test_macros.hpp>

namespace {

struct Fixture
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    psr::TurnCoordinator turn_coordinator{registry};
    psr::MessageBus message_bus;

    psr::GameplayContext Context()
    {
        return psr::GameplayContext{registry, grid, turn_coordinator, entt::null, message_bus};
    }
};

} // namespace

TEST_CASE("AnimationState stays on top while any TweenComponent is in flight", "[AnimationState]")
{
    Fixture fixture;
    entt::entity entity = fixture.registry.CreateEntity();
    fixture.registry.GetOrEmplace<psr::TweenComponent>(entity).queue.push_back(
        psr::Tween{psr::Vec2f{1.0f, 0.0f}, psr::Vec2f{}, /*duration=*/0.2f, /*elapsed=*/0.0f, nullptr});

    psr::AnimationState state;
    psr::GameplayContext context = fixture.Context();
    psr::StateTransition transition = state.Update(context, 0.1f);

    REQUIRE(transition.kind == psr::StateTransitionKind::None);
    REQUIRE(fixture.registry.HasComponent<psr::TweenComponent>(entity));
}

TEST_CASE("AnimationState pops once the registry has no Tweens left", "[AnimationState]")
{
    Fixture fixture;
    entt::entity entity = fixture.registry.CreateEntity();
    fixture.registry.GetOrEmplace<psr::TweenComponent>(entity).queue.push_back(
        psr::Tween{psr::Vec2f{1.0f, 0.0f}, psr::Vec2f{}, /*duration=*/0.2f, /*elapsed=*/0.0f, nullptr});

    psr::AnimationState state;
    psr::GameplayContext context = fixture.Context();
    psr::StateTransition transition = state.Update(context, 0.2f);

    REQUIRE(transition.kind == psr::StateTransitionKind::Pop);
    REQUIRE_FALSE(fixture.registry.HasComponent<psr::TweenComponent>(entity));
}

TEST_CASE("AnimationState pops immediately when there is nothing to animate", "[AnimationState]")
{
    Fixture fixture;
    psr::AnimationState state;
    psr::GameplayContext context = fixture.Context();

    psr::StateTransition transition = state.Update(context, 0.1f);

    REQUIRE(transition.kind == psr::StateTransitionKind::Pop);
}

TEST_CASE("AnimationState's UpdateTweens call fires a Tween's on_completion before it pops", "[AnimationState]")
{
    Fixture fixture;
    entt::entity entity = fixture.registry.CreateEntity();
    bool fired = false;
    fixture.registry.GetOrEmplace<psr::TweenComponent>(entity).queue.push_back(
        psr::Tween{psr::Vec2f{1.0f, 0.0f}, psr::Vec2f{}, /*duration=*/0.1f, /*elapsed=*/0.0f,
                  [&fired]() { fired = true; }});

    psr::AnimationState state;
    psr::GameplayContext context = fixture.Context();
    psr::StateTransition transition = state.Update(context, 0.1f);

    REQUIRE(fired);
    REQUIRE(transition.kind == psr::StateTransitionKind::Pop);
}

TEST_CASE("AnimationState keeps blocking when a completion queues a follow-up Tween on the same entity",
          "[AnimationState]")
{
    Fixture fixture;
    entt::entity entity = fixture.registry.CreateEntity();
    psr::Registry* registry_ptr = &fixture.registry;
    fixture.registry.GetOrEmplace<psr::TweenComponent>(entity).queue.push_back(psr::Tween{
        psr::Vec2f{1.0f, 0.0f}, psr::Vec2f{}, /*duration=*/0.1f, /*elapsed=*/0.0f,
        [registry_ptr, entity]()
        {
            registry_ptr->GetComponent<psr::TweenComponent>(entity).queue.push_back(
                psr::Tween{psr::Vec2f{}, psr::Vec2f{1.0f, 0.0f}, 0.1f, 0.0f, nullptr});
        }});

    psr::AnimationState state;
    psr::GameplayContext context = fixture.Context();
    psr::StateTransition transition = state.Update(context, 0.1f);

    REQUIRE(transition.kind == psr::StateTransitionKind::None);
    REQUIRE(fixture.registry.HasComponent<psr::TweenComponent>(entity));
}
