#pragma once

#include "Engine/ECS/Registry.h"
#include "Engine/Dungeon/PieceLibrary.h"
#include "Engine/Items/AffixLibrary.h"
#include "Engine/Layer.h"
#include "Engine/Render/Camera.h"
#include "Engine/Render/TextureAtlas.h"
#include "Engine/Render/TileGpuPipeline.h"
#include "Engine/Render/TileRenderer.h"
#include "Engine/World/Grid.h"
#include "Render/RegistryRenderableLookup.h"
#include "Systems/TurnCoordinator.h"

#include <entt/entt.hpp>

#include <optional>
#include <random>

namespace psr {

// The live gameplay scene: generates a dungeon into a Grid, spawns the
// player into it, and drives the turn loop -- TurnCoordinator's buffered
// input wired to the SDL event loop, TileRenderer/Camera drawing the result.
// This is the permanent home for gameplay systems as they land (combat,
// AI, mission flow, HUD), not a throwaway harness -- start small (player
// movement in a fixed test dungeon) and grow it in place.
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

    Registry m_registry;
    PieceLibrary m_pieces;
    AffixLibrary m_affixes; // empty: no enemies spawn yet, so MoveAction's attack fallback never triggers
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

    std::optional<TextureAtlas> m_atlas;
    std::optional<TileGpuPipeline> m_gpu_pipeline;
    std::optional<RegistryRenderableLookup> m_renderable_lookup;
    std::optional<TileRenderer> m_tile_renderer;
};

} // namespace psr
