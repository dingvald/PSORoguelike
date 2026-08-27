#pragma once

#include "Engine/Combat/PhotonArtLibrary.h"
#include "Engine/Combat/TechniqueLibrary.h"
#include "Engine/Dungeon/PieceLibrary.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Items/AffixLibrary.h"
#include "Engine/Layer.h"
#include "Engine/Render/Camera.h"
#include "Engine/Render/TextureAtlas.h"
#include "Engine/Render/TileGpuPipeline.h"
#include "Engine/Render/TileRenderer.h"
#include "Engine/World/Grid.h"
#include "Render/RegistryRenderableLookup.h"
#include "States/ExploringState.h"
#include "States/GameStateMachine.h"
#include "States/TargetSelectionState.h"
#include "Systems/CombatLogBridge.h"
#include "Systems/TurnCoordinator.h"

#include <entt/entt.hpp>

#include <memory>
#include <optional>
#include <random>

namespace psr {

class IAction;
struct HotbarSlotActivatedMessage;
struct HudReadyMessage;

// The live gameplay scene: generates a dungeon into a Grid, spawns the
// player into it, and drives the turn loop -- TurnCoordinator's buffered
// input wired to the SDL event loop, TileRenderer/Camera drawing the result.
// This is the permanent home for gameplay systems as they land (combat,
// AI, mission flow, HUD), not a throwaway harness -- start small (player
// movement in a fixed test dungeon) and grow it in place.
//
// Input/turn flow is now hosted on a GameStateMachine (ExploringState/
// TargetSelectionState -- see States/), ported from UnnamedRoguelike so
// Photon Art/Technique targeting can suspend normal play the same way that
// sibling suspends its own ExploringState for a modal cursor. Photon Art/
// Technique activation is driven by the player's HotbarComponent (number
// keys 1-9/0, or a HudLayer click forwarded as HotbarSlotActivatedMessage)
// -- see TryActivateSlot.
class GameplayLayer : public Layer
{
public:
    GameplayLayer();
    ~GameplayLayer() override;

    GameplayLayer(const GameplayLayer&) = delete;
    GameplayLayer& operator=(const GameplayLayer&) = delete;
    GameplayLayer(GameplayLayer&&) = delete;
    GameplayLayer& operator=(GameplayLayer&&) = delete;

    void OnAttach() override;
    void OnUpdate(float delta_time) override;
    void OnRender(SDL_Renderer* renderer) override;
    void OnEvent(Event& event) override;

private:
    // Builds the atlas/GPU pipeline/tile renderer on first call -- they need
    // a live SDL_Renderer&, which Layer only ever hands to OnRender, never
    // OnAttach (see Application; there is no other renderer accessor).
    void EnsureRenderResources(SDL_Renderer& renderer);

    // If slot_index names a Technique/PhotonArt hotbar slot (see
    // HotbarComponent) the player can currently afford, constructs the
    // corresponding Action and requests targeting for it. Returns whether the
    // slot was consumed (Item/Empty slots, or an unaffordable ability,
    // return false).
    bool TryActivateSlot(int slot_index);

    void OnHotbarSlotActivated(const HotbarSlotActivatedMessage& message);

    // Responds to HudLayer's HudReadyMessage by re-publishing current hotbar/
    // player-status state -- see that message's own doc comment for why a
    // one-time publish from this layer's own OnAttach can't reach HudLayer
    // directly.
    void OnHudReady(const HudReadyMessage& message);

    // Resolves the player's current HotbarComponent into display names
    // (via m_techniques/m_photon_arts) and publishes a HotbarStateMessage.
    void PublishHotbarState();

    Registry m_registry;
    PieceLibrary m_pieces;
    AffixLibrary m_affixes; // empty: no enemies spawn yet, so MoveAction's attack fallback never triggers
    PhotonArtLibrary m_photon_arts;
    TechniqueLibrary m_techniques;
    std::mt19937 m_rng{std::random_device{}()};

    std::optional<Grid> m_grid;
    entt::entity m_player = entt::null;
    Camera m_camera;

    // Non-movable (binds registry component-lifecycle listeners to its own
    // address) -- must be constructed in place, after m_registry exists but
    // before the player's EnergyComponent is spawned (TurnQueue membership
    // is driven by that construction order, see TurnCoordinator.cpp).
    // Declared after m_registry so it's destroyed first.
    std::optional<TurnCoordinator> m_turn_coordinator;

    // Bridges the player's per-entity combat events onto the Layer
    // MessageBus for HudLayer to consume -- see CombatLogBridge.h. Holds
    // only pointers into m_registry/m_techniques/m_photon_arts, so its
    // declaration order relative to them doesn't matter for construction/
    // destruction safety.
    std::optional<CombatLogBridge> m_combat_log_bridge;

    // Kept alive across the interactive target-select flow -- RequestTargeting
    // only takes a non-owning IAction*, so whoever constructs the action
    // (this layer, for the reasons in the class doc comment) must own its
    // lifetime until it's either resolved (TurnCoordinator::SetPendingAction
    // then Step()'s normal resolution) or the player cancels.
    std::unique_ptr<IAction> m_pending_cast_action;

    // GameStateMachine and its two states this round -- declaration order
    // matters: m_target_selection_state must outlive m_exploring_state (which
    // holds a reference to it) and both must outlive m_state_machine's use of
    // either.
    TargetSelectionState m_target_selection_state;
    ExploringState m_exploring_state{m_target_selection_state};
    GameStateMachine m_state_machine;

    std::optional<TextureAtlas> m_atlas;
    std::optional<TileGpuPipeline> m_gpu_pipeline;
    std::optional<RegistryRenderableLookup> m_renderable_lookup;
    std::optional<TileRenderer> m_tile_renderer;
};

} // namespace psr
