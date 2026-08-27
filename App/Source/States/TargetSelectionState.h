#pragma once

#include "Actions/ITargetRequestSink.h"
#include "Engine/Math/Color.h"
#include "Engine/Math/Vec2.h"
#include "States/GameState.h"

#include <entt/entt.hpp>

namespace psr {

// Modal target-select cursor, pushed by ExploringState on
// TurnStep::TargetingRequested and popped once the player confirms or
// cancels. Ported from UnnamedRoguelike's own TargetSelectionState: the
// cursor is a real, data-driven ECS entity
// (App/Assets/Data/Entities/ui/target_select_cursor.json) spawned/moved/
// despawned in the live Grid and drawn by the ordinary tile render pass --
// not a bespoke SDL overlay -- with out-of-range signaled by recoloring the
// cursor sprite grey in place, not by highlighting a whole tile set.
//
// A single mechanism serves all three TargetingMode values by varying only
// what counts as "reachable" and how arrow keys move the cursor:
//   - SelfTarget: reachable = {origin} only; cursor fixed there.
//   - Directional: reachable = the 4 cardinal-adjacent tiles; an arrow key
//     press jumps the cursor straight to that neighbour.
//   - TargetSquare: reachable = every tile within Chebyshev range of origin
//     (grid-clamped); arrow keys move the cursor incrementally by one tile.
// Confirm (Space) is a no-op off the reachable set; Escape cancels without
// consuming the actor's turn. Following the sibling's own split: HandleEvent
// only updates cursor state and sets a confirmed/cancelled flag; Update()
// (called every frame regardless of input) is what actually applies the
// confirm/cancel side effect and returns the Pop transition, since
// HandleEvent has no way to return a StateTransition itself.
class TargetSelectionState : public GameState
{
public:
    // Configures the pending request just before the caller (ExploringState)
    // pushes this state -- request.action must stay valid until this state
    // either confirms (SetPendingAction hands it to TurnCoordinator) or
    // cancels (the caller is responsible for its lifetime either way; this
    // state never owns it).
    void Begin(TargetRequest request, entt::entity actor);

    GameStateId GetId() const override { return GameStateId::TargetSelection; }

    void OnEnter(GameplayContext& context) override;
    void OnExit(GameplayContext& context) override;
    StateTransition Update(GameplayContext& context, float delta_time) override;
    bool HandleEvent(Event& event, GameplayContext& context) override;

private:
    bool IsReachable(Vec2 tile) const;
    void MoveCursor(GameplayContext& context, Vec2 direction);
    void UpdateCursorVisual(GameplayContext& context);

    TargetRequest m_request;
    entt::entity m_actor = entt::null;
    Vec2 m_origin;
    Vec2 m_cursor;
    entt::entity m_cursor_entity = entt::null;
    Color m_base_color; // the cursor prefab's own authored color, cached at spawn so greying can be reversed
    bool m_confirmed = false;
    bool m_cancelled = false;
};

} // namespace psr
