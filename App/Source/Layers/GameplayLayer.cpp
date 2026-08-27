#include "Layers/GameplayLayer.h"

#include "ApplicationFilepaths.h"
#include "Actions/PhotonArtAction.h"
#include "Actions/TechniqueAction.h"
#include "Components/EnergyComponent.h"
#include "Components/EquipmentComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Components/RegisterComponents.h"
#include "Content/KeyBindings.h"
#include "Engine/Combat/PhotonArt.h"
#include "Engine/Combat/PhotonArtLibraryFile.h"
#include "Engine/Combat/Technique.h"
#include "Engine/Combat/TechniqueLibraryFile.h"
#include "Engine/Dungeon/DungeonInstantiator.h"
#include "Engine/Dungeon/DungeonLibrary.h"
#include "Engine/Dungeon/DungeonLibraryFile.h"
#include "Engine/Dungeon/DungeonStitcher.h"
#include "Engine/Dungeon/PieceLibraryFile.h"
#include "Engine/ECS/JsonEntityLoader.h"
#include "Engine/ECS/MesetaComponent.h"
#include "Engine/ECS/PPComponent.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/SectionIdComponent.h"
#include "Engine/ECS/SocketComponent.h"
#include "Engine/ECS/TPComponent.h"
#include "Engine/ECS/WeaponComponent.h"
#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Items/DropTableLibraryFile.h"
#include "Engine/Persistence/JsonDirectoryLoader.h"
#include "States/GameState.h"

#include <entt/core/hashed_string.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_keycode.h>

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
    m_photon_arts = LoadPhotonArtLibrary(ApplicationFilepaths::PhotonArtsPath);
    m_techniques = LoadTechniqueLibrary(ApplicationFilepaths::TechniquesPath);
    m_drop_tables = LoadDropTableLibrary(ApplicationFilepaths::DropTablesPath);

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

    // Needs a live Grid&, so constructed after m_grid.emplace() above.
    m_loot_drop_system.emplace(m_registry, *m_grid, m_drop_tables, m_rng);

    m_player = m_registry.CreateEntity(entt::hashed_string::value(kPlayerPrefabId));
    m_registry.Emplace<Position>(m_player, Position{instantiation.entrance_tile});
    m_registry.Emplace<PlayerControlledComponent>(m_player);
    m_registry.Emplace<MesetaComponent>(m_player);
    // Not overwriting an authored SectionIdComponent if player.json already
    // has one (see SectionIdComponent's own doc comment on why one might be
    // authored as a template default ahead of M10.3's real character
    // creation); only emplaces the default (None) when the prefab has none.
    m_registry.GetOrEmplace<SectionIdComponent>(m_player);
    m_grid->AddEntity(instantiation.entrance_tile, m_player);
    m_camera.SetTarget(instantiation.entrance_tile);

    m_registry.Emplace<EnergyComponent>(m_player); // enqueues the player into the turn queue

    GameplayContext context{m_registry, *m_grid, *m_turn_coordinator, m_player};
    m_state_machine.Push(m_exploring_state, context);
}

void GameplayLayer::OnUpdate(float delta_time)
{
    if (!m_turn_coordinator || !m_grid)
        return;

    GameplayContext context{m_registry, *m_grid, *m_turn_coordinator, m_player};
    m_state_machine.Update(context, delta_time);

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

bool GameplayLayer::TryBeginCast(int key_code)
{
    int slot = -1;
    bool is_technique = false;
    if (key_code >= SDLK_1 && key_code <= SDLK_4)
        slot = key_code - SDLK_1;
    else if (key_code >= SDLK_5 && key_code <= SDLK_8)
    {
        slot = key_code - SDLK_5;
        is_technique = true;
    }
    else
        return false;

    if (!m_registry.IsValid(m_player))
        return false;
    const EquipmentComponent* equipment = m_registry.TryGetComponent<EquipmentComponent>(m_player);
    if (!equipment || equipment->weapon == entt::null)
        return false;
    const WeaponComponent* weapon = m_registry.TryGetComponent<WeaponComponent>(equipment->weapon);
    if (!weapon)
        return false;

    if (is_technique)
    {
        if (slot >= static_cast<int>(weapon->technique_ids.size()))
            return false;
        const std::uint32_t technique_id = weapon->technique_ids[static_cast<std::size_t>(slot)];
        const Technique* technique = m_techniques.Find(technique_id);
        if (!technique)
            return false;
        const TPComponent* tp = m_registry.TryGetComponent<TPComponent>(m_player);
        if (!tp || tp->current_tp < technique->tp_cost)
            return false;

        m_pending_cast_action =
            std::make_unique<TechniqueAction>(*m_grid, m_techniques, m_affixes, technique_id, m_rng);
        m_turn_coordinator->RequestTargeting(
            TargetRequest{m_pending_cast_action.get(), technique->targeting_mode, technique->range_shape, technique->range});
        return true;
    }

    if (slot >= static_cast<int>(weapon->photon_art_ids.size()))
        return false;
    const std::uint32_t photon_art_id = weapon->photon_art_ids[static_cast<std::size_t>(slot)];
    const PhotonArt* art = m_photon_arts.Find(photon_art_id);
    if (!art)
        return false;
    const PPComponent* pp = m_registry.TryGetComponent<PPComponent>(m_player);
    if (!pp || pp->current_pp < art->pp_cost)
        return false;

    m_pending_cast_action = std::make_unique<PhotonArtAction>(*m_grid, m_photon_arts, m_affixes, photon_art_id, m_rng);
    m_turn_coordinator->RequestTargeting(
        TargetRequest{m_pending_cast_action.get(), art->targeting_mode, art->range_shape, art->range});
    return true;
}

void GameplayLayer::OnEvent(Event& event)
{
    if (!m_turn_coordinator || !m_grid)
        return;

    // Placeholder cast trigger only intercepts keys while the player is free
    // to act (Exploring on top, not already mid-target-select) -- see class
    // doc comment.
    if (m_state_machine.Top() == &m_exploring_state)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& key_event)
                                             { return TryBeginCast(key_event.GetKeyCode()); });
    }

    if (event.handled)
        return;

    GameplayContext context{m_registry, *m_grid, *m_turn_coordinator, m_player};
    m_state_machine.HandleEvent(event, context);
}

} // namespace psr
