#include "States/ConfirmState.h"
#include "States/PauseState.h"

#include "Engine/ECS/Registry.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Messages/MessageBus.h"
#include "Engine/World/Grid.h"
#include "Systems/TurnCoordinator.h"

#include <catch2/catch_test_macros.hpp>

#include <SDL3/SDL_keycode.h>

namespace {

// Neither state touches Registry/Grid/TurnCoordinator/the player entity at
// all (see their own doc comments -- both are pure publish-on-enter,
// close-on-Escape modals), so this fixture only exists to satisfy
// GameplayContext's constructor, same minimal-construction precedent
// TargetSelectionStateTests.cpp's own Fixture already sets.
struct Fixture
{
    psr::Registry registry;
    psr::Grid grid{4, 4};
    psr::TurnCoordinator turn_coordinator{registry};
    psr::MessageBus message_bus;
    entt::entity actor = entt::null;

    psr::GameplayContext Context() { return psr::GameplayContext{registry, grid, turn_coordinator, actor, message_bus}; }
};

template <typename TState> bool Send(TState& state, psr::GameplayContext& context, int key_code)
{
    psr::KeyPressedEvent key_event(key_code, /*repeat=*/false);
    return state.HandleEvent(key_event, context);
}

} // namespace

TEST_CASE("PauseState closes itself on Escape", "[PauseState]")
{
    Fixture fixture;
    psr::PauseState state;
    psr::GameplayContext context = fixture.Context();

    state.OnEnter(context);
    REQUIRE(state.Update(context, 0.0f).kind == psr::StateTransitionKind::None);

    REQUIRE(Send(state, context, SDLK_ESCAPE));
    REQUIRE(state.Update(context, 0.0f).kind == psr::StateTransitionKind::Pop);
}

TEST_CASE("PauseState ignores non-Escape keys", "[PauseState]")
{
    Fixture fixture;
    psr::PauseState state;
    psr::GameplayContext context = fixture.Context();

    state.OnEnter(context);
    REQUIRE_FALSE(Send(state, context, SDLK_A));
    REQUIRE(state.Update(context, 0.0f).kind == psr::StateTransitionKind::None);
}

TEST_CASE("PauseState resets its close flag on re-entry", "[PauseState]")
{
    Fixture fixture;
    psr::PauseState state;
    psr::GameplayContext context = fixture.Context();

    state.OnEnter(context);
    REQUIRE(Send(state, context, SDLK_ESCAPE));

    // Re-entering (e.g. a fresh Push after a previous Pop) must not carry
    // the stale close request forward -- same "OnEnter resets" contract
    // MissionSelectState/CharacterScreenState's own m_close_requested reset
    // already documents.
    state.OnEnter(context);
    REQUIRE(state.Update(context, 0.0f).kind == psr::StateTransitionKind::None);
}

TEST_CASE("ConfirmState round-trips Configure through GetAction", "[ConfirmState]")
{
    psr::ConfirmState state;
    state.Configure("Quit to Title?", psr::ConfirmAction::QuitToTitle);
    REQUIRE(state.GetAction() == psr::ConfirmAction::QuitToTitle);

    state.Configure("Quit to desktop?", psr::ConfirmAction::QuitToDesktop);
    REQUIRE(state.GetAction() == psr::ConfirmAction::QuitToDesktop);
}

TEST_CASE("ConfirmState closes itself on Escape, treated as an implicit No", "[ConfirmState]")
{
    Fixture fixture;
    psr::ConfirmState state;
    psr::GameplayContext context = fixture.Context();

    state.Configure("Quit to desktop?", psr::ConfirmAction::QuitToDesktop);
    state.OnEnter(context);
    REQUIRE(state.Update(context, 0.0f).kind == psr::StateTransitionKind::None);

    REQUIRE(Send(state, context, SDLK_ESCAPE));
    REQUIRE(state.Update(context, 0.0f).kind == psr::StateTransitionKind::Pop);
}

TEST_CASE("ConfirmState resets its close flag on re-entry", "[ConfirmState]")
{
    Fixture fixture;
    psr::ConfirmState state;
    psr::GameplayContext context = fixture.Context();

    state.Configure("Quit to desktop?", psr::ConfirmAction::QuitToDesktop);
    state.OnEnter(context);
    REQUIRE(Send(state, context, SDLK_ESCAPE));

    state.OnEnter(context);
    REQUIRE(state.Update(context, 0.0f).kind == psr::StateTransitionKind::None);
}
