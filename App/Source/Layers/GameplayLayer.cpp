#include "Layers/GameplayLayer.h"

#include "ApplicationFilepaths.h"
#include "Components/EnergyComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Components/RegisterComponents.h"
#include "Content/KeyBindings.h"
#include "Engine/Dungeon/DungeonInstantiator.h"
#include "Engine/Dungeon/DungeonLibrary.h"
#include "Engine/Dungeon/DungeonLibraryFile.h"
#include "Engine/Dungeon/DungeonStitcher.h"
#include "Engine/Dungeon/PieceLibraryFile.h"
#include "Engine/ECS/JsonEntityLoader.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/SocketComponent.h"
#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Persistence/JsonDirectoryLoader.h"

#include <entt/core/hashed_string.hpp>

#include <SDL3/SDL.h>

#include <stdexcept>
#include <string>
#include <unordered_map>

namespace psr {

namespace {

    constexpr int kEntitySchemaVersion = 1;
    constexpr int kTileWidth = 16;
    constexpr int kTileHeight = 24;

    // The dungeon this layer loads on attach. Hardcoded for now (no mission
    // select exists yet) -- revisit once a hub/mission-select flow needs to
    // choose this at runtime instead.
    constexpr const char* kDungeonId = "test_dungeon";

    // The player's prefab -- appearance (and, later, base stats) lives in
    // App/Assets/Data/Entities/player.json like every other authored entity,
    // not hand-built here. PlayerControlledComponent/Position/EnergyComponent
    // are never authored on it (all three are engine-managed, authorable=false)
    // and are emplaced onto the spawned instance separately below.
    constexpr const char* kPlayerPrefabId = "player";

    // Scans every authored entity once, instantiating (then immediately
    // destroying) a throwaway runtime instance per prefab to read its
    // SocketComponent, if any -- mirrors DungeonEditorLayer::BuildPrefabCaches'
    // socket-lookup construction, but against the layer's own live registry
    // (already populated via RegisterPrefabs) rather than a separate one, since
    // this layer has no other use for a second registry.
    std::unordered_map<std::uint32_t, SocketInfo> BuildSocketLookup(Registry& registry)
    {
        std::unordered_map<std::uint32_t, SocketInfo> sockets;
        for (const JsonDirectoryEntry& entry : LoadJsonDirectory(ApplicationFilepaths::EntitiesPath, kEntitySchemaVersion))
        {
            const std::uint32_t prefab_id = entt::hashed_string::value(entry.id.c_str());
            const entt::entity instance = registry.CreateEntity(prefab_id);
            if (instance == entt::null)
                continue;

            if (const SocketComponent* socket = registry.TryGetComponent<SocketComponent>(instance))
                sockets.emplace(prefab_id, SocketInfo{socket->tags, socket->fallback_prefab_id});

            registry.DestroyEntity(instance);
        }
        return sockets;
    }

} // namespace

GameplayLayer::GameplayLayer() : Layer("GameplayLayer") {}
GameplayLayer::~GameplayLayer() = default;

void GameplayLayer::OnAttach()
{
    // Content-load/generation failures below are build-input bugs (a missing
    // or malformed file, a dungeon definition with no valid layout), not a
    // runtime condition a player can hit -- they're allowed to propagate as
    // exceptions rather than being caught and swallowed into a black screen.
    // main.cpp's top-level catch turns an uncaught one into a logged, clean
    // exit instead of an OS crash dialog.
    const EntitySchemaModel schema = RegisterComponents(m_registry);
    JsonEntityLoader loader{m_registry.GetMetaContext(), &schema};
    loader.Load(ApplicationFilepaths::EntitiesPath);
    m_registry.RegisterPrefabs(loader);

    const std::unordered_map<std::uint32_t, SocketInfo> sockets = BuildSocketLookup(m_registry);
    const SocketLookup socket_lookup = [&sockets](std::uint32_t id) -> std::optional<SocketInfo>
    {
        auto it = sockets.find(id);
        return it == sockets.end() ? std::nullopt : std::make_optional(it->second);
    };

    m_pieces = LoadPieceLibrary(ApplicationFilepaths::PiecesPath);

    const DungeonLibrary dungeons = LoadDungeonLibrary(ApplicationFilepaths::DungeonsPath);
    const Dungeon* dungeon = dungeons.Find(entt::hashed_string::value(kDungeonId));
    if (!dungeon)
        throw std::runtime_error(std::string("GameplayLayer: no '") + kDungeonId + "' dungeon definition found");

    const DungeonLayout layout = GenerateDungeon(*dungeon, m_pieces, socket_lookup, m_rng());

    const Rect bounds = ComputeDungeonBounds(layout, m_pieces);
    if (bounds.Empty())
        throw std::runtime_error("GameplayLayer: generated dungeon has no cells");
    m_grid.emplace(bounds.size.x, bounds.size.y);
    const DungeonInstantiation instantiation =
        InstantiateDungeon(layout, m_pieces, -bounds.origin, m_registry, *m_grid);

    // Must be constructed before the player's EnergyComponent is emplaced --
    // TurnQueue membership is driven by TurnCoordinator's own
    // OnConstruct<EnergyComponent> listener, wired in its constructor.
    m_turn_coordinator.emplace(m_registry);
    m_turn_coordinator->KeyBindings() = CreateDefaultKeyBindings(*m_grid, m_affixes, m_rng);

    m_player = m_registry.CreateEntity(entt::hashed_string::value(kPlayerPrefabId));
    m_registry.Emplace<Position>(m_player, Position{instantiation.entrance_tile});
    m_registry.Emplace<PlayerControlledComponent>(m_player);
    m_grid->AddEntity(instantiation.entrance_tile, m_player);
    m_camera.SetTarget(instantiation.entrance_tile);

    m_registry.Emplace<EnergyComponent>(m_player); // enqueues the player into the turn queue
}

void GameplayLayer::OnUpdate(float delta_time)
{
    if (!m_turn_coordinator || !m_grid)
        return;

    m_turn_coordinator->Step(delta_time);

    if (m_registry.IsValid(m_player))
        m_camera.SetTarget(m_registry.GetComponent<Position>(m_player).tile);
}

void GameplayLayer::EnsureRenderResources(SDL_Renderer& renderer)
{
    if (m_tile_renderer)
        return;

    m_atlas.emplace(renderer, ApplicationFilepaths::TexturesPath);
    m_gpu_pipeline.emplace(renderer, ApplicationFilepaths::ShadersPath / "TileSprite.vert.spv",
                           ApplicationFilepaths::ShadersPath / "TileSprite.frag.spv");
    m_renderable_lookup.emplace(m_registry);
    m_tile_renderer.emplace(*m_grid, *m_atlas, *m_gpu_pipeline, *m_renderable_lookup, kTileWidth, kTileHeight);
}

void GameplayLayer::OnRender(SDL_Renderer* renderer)
{
    if (!renderer || !m_grid)
        return;

    EnsureRenderResources(*renderer);
    if (!m_tile_renderer)
        return;

    int width = 0;
    int height = 0;
    SDL_GetCurrentRenderOutputSize(renderer, &width, &height);
    m_tile_renderer->Draw(*renderer, m_camera.GetPosition(), width, height);
}

void GameplayLayer::OnEvent(Event& event)
{
    if (!m_turn_coordinator)
        return;

    EventDispatcher dispatcher(event);
    dispatcher.Dispatch<KeyPressedEvent>(
        [this](KeyPressedEvent& key_event)
        {
            m_turn_coordinator->PressKey(key_event.GetKeyCode());
            return true;
        });
    dispatcher.Dispatch<KeyReleasedEvent>(
        [this](KeyReleasedEvent& key_event)
        {
            m_turn_coordinator->ReleaseKey(key_event.GetKeyCode());
            return true;
        });
}

} // namespace psr
