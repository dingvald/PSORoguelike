#include "Layers/GameplayLayer.h"

#include "Actions/PhotonArtAction.h"
#include "Actions/TechniqueAction.h"
#include "ApplicationFilepaths.h"
#include "Components/EnergyComponent.h"
#include "Components/EquipmentComponent.h"
#include "Components/HotbarComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Components/RegisterComponents.h"
#include "Content/KeyBindings.h"
#include "Engine/Combat/PhotonArt.h"
#include "Engine/Combat/PhotonArtLibraryFile.h"
#include "Engine/Combat/StatusEffectLibraryFile.h"
#include "Engine/Combat/Technique.h"
#include "Engine/Combat/TechniqueLibraryFile.h"
#include "Engine/Dungeon/DungeonInstantiator.h"
#include "Engine/Dungeon/DungeonLibrary.h"
#include "Engine/Dungeon/DungeonLibraryFile.h"
#include "Engine/Dungeon/DungeonStitcher.h"
#include "Engine/Dungeon/PieceLibraryFile.h"
#include "Engine/ECS/JsonEntityLoader.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/SocketComponent.h"
#include "Engine/ECS/TPComponent.h"
#include "Engine/ECS/WeaponComponent.h"
#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Persistence/JsonDirectoryLoader.h"
#include "Layers/HudLayer.h"
#include "Messages/HotbarSlotActivatedMessage.h"
#include "Messages/HotbarStateMessage.h"
#include "Messages/HudReadyMessage.h"
#include "States/GameState.h"

#include <entt/core/hashed_string.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_keycode.h>

#include <optional>
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
        for (const JsonDirectoryEntry& entry :
             LoadJsonDirectory(ApplicationFilepaths::EntitiesPath, kEntitySchemaVersion))
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

    // Number-row key to hotbar slot index: 1-9 -> 0-8, 0 -> 9.
    std::optional<int> KeyCodeToHotbarSlot(int key_code)
    {
        if (key_code >= SDLK_1 && key_code <= SDLK_9)
            return key_code - SDLK_1;
        if (key_code == SDLK_0)
            return 9;
        return std::nullopt;
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

    // Loaded before SetStatusEffectLibrary below needs it -- unlike
    // m_affixes (still empty pending M8.2's drop-table work), status
    // effects are real, immediately-consumed content: StatusEffectComponent's
    // handlers and TurnCoordinator's Freeze check both resolve ids through
    // this library on every turn.
    m_status_effects = LoadStatusEffectLibrary(ApplicationFilepaths::StatusEffectsPath);

    // Lets EquipmentComponent's AttachHandlers-registered handler (which
    // can't capture state) reach affix data when it contributes a
    // Before<Action>Event's effective stats -- must happen before any entity
    // that could carry EquipmentComponent/StatsComponent is created.
    m_registry.SetAffixLibrary(m_affixes);

    // Same purpose, for status effects -- must happen before any turn/entity
    // work begins (StatusEffectComponent handlers, TurnCoordinator's Freeze
    // check).
    m_registry.SetStatusEffectLibrary(m_status_effects);

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

    // Default hotbar loadout: first 4 weapon-granted Techniques into slots
    // 0-3, first 4 Photon Arts into slots 4-7 (mirrors the old placeholder
    // cast trigger's fixed key ranges, now captured as data instead of
    // re-derived by key range on every press). Typically all-empty today --
    // nothing sets EquipmentComponent::weapon on the player yet (see its own
    // doc comment). Slots 8-9 are always Item stubs so the HUD always has one
    // of each slot type to render -- item activation is a deliberate no-op,
    // there is no inventory system yet.
    HotbarComponent hotbar;
    if (const EquipmentComponent* equipment = m_registry.TryGetComponent<EquipmentComponent>(m_player);
        equipment && equipment->weapon != entt::null)
    {
        if (const WeaponComponent* weapon = m_registry.TryGetComponent<WeaponComponent>(equipment->weapon))
        {
            std::size_t slot = 0;
            for (std::uint32_t technique_id : weapon->technique_ids)
            {
                if (slot >= 4)
                    break;
                hotbar.slots[slot++] = HotbarSlot{HotbarSlotType::Technique, technique_id};
            }
            slot = 4;
            for (std::uint32_t photon_art_id : weapon->photon_art_ids)
            {
                if (slot >= 8)
                    break;
                hotbar.slots[slot++] = HotbarSlot{HotbarSlotType::PhotonArt, photon_art_id};
            }
        }
    }
    hotbar.slots[8].type = HotbarSlotType::Item;
    hotbar.slots[9].type = HotbarSlotType::Item;
    m_registry.Emplace<HotbarComponent>(m_player, hotbar);

    m_combat_log_bridge.emplace(m_registry, GetMessageBus(), m_techniques, m_photon_arts, m_status_effects, m_player);
    m_combat_log_bridge->Subscribe(Entity(m_registry, m_player));

    m_status_effect_markers.emplace(m_registry, *m_grid, m_status_effects);
    m_status_effect_markers->Subscribe(Entity(m_registry, m_player));

    Subscribe<HotbarSlotActivatedMessage>(&GameplayLayer::OnHotbarSlotActivated, this);
    Subscribe<HudReadyMessage>(&GameplayLayer::OnHudReady, this);

    PushOverlay<HudLayer>();

    GameplayContext context{m_registry, *m_grid, *m_turn_coordinator, m_player};
    m_state_machine.Push(m_exploring_state, context);
}

void GameplayLayer::OnUpdate(float delta_time)
{
    HandleQueuedMessages();

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

bool GameplayLayer::TryActivateSlot(int slot_index)
{
    if (!m_registry.IsValid(m_player) || slot_index < 0 ||
        slot_index >= static_cast<int>(HotbarComponent::kSlotCount))
        return false;

    const HotbarComponent* hotbar = m_registry.TryGetComponent<HotbarComponent>(m_player);
    if (!hotbar)
        return false;
    const HotbarSlot& slot = hotbar->slots[static_cast<std::size_t>(slot_index)];

    switch (slot.type)
    {
    case HotbarSlotType::Technique:
    {
        const Technique* technique = m_techniques.Find(slot.id);
        if (!technique)
            return false;
        const TPComponent* tp = m_registry.TryGetComponent<TPComponent>(m_player);
        if (!tp || tp->current_tp < technique->tp_cost)
            return false;

        m_pending_cast_action = std::make_unique<TechniqueAction>(*m_grid, m_techniques, m_affixes, slot.id, m_rng);
        m_turn_coordinator->RequestTargeting(TargetRequest{m_pending_cast_action.get(), technique->targeting_mode,
                                                           technique->range_shape, technique->range});
        return true;
    }
    case HotbarSlotType::PhotonArt:
    {
        const PhotonArt* art = m_photon_arts.Find(slot.id);
        if (!art)
            return false;
        const TPComponent* tp = m_registry.TryGetComponent<TPComponent>(m_player);
        if (!tp || tp->current_tp < art->tp_cost)
            return false;

        m_pending_cast_action = std::make_unique<PhotonArtAction>(*m_grid, m_photon_arts, m_affixes, slot.id, m_rng);
        m_turn_coordinator->RequestTargeting(
            TargetRequest{m_pending_cast_action.get(), art->targeting_mode, art->range_shape, art->range});
        return true;
    }
    case HotbarSlotType::Item:
    case HotbarSlotType::Empty:
    default:
        return false;
    }
}

void GameplayLayer::OnHotbarSlotActivated(const HotbarSlotActivatedMessage& message)
{
    // Same guard the key-press path already implicitly has via OnEvent below --
    // don't let a stray click activate an ability while target-select is
    // already in progress.
    if (m_state_machine.Top() == &m_exploring_state)
        TryActivateSlot(message.slot_index);
}

void GameplayLayer::OnHudReady(const HudReadyMessage& /*message*/)
{
    PublishHotbarState();
    if (m_combat_log_bridge)
    {
        m_combat_log_bridge->PublishPlayerStatus();
        m_combat_log_bridge->PublishStatusEffects();
    }
}

void GameplayLayer::PublishHotbarState()
{
    if (!m_registry.IsValid(m_player))
        return;
    const HotbarComponent* hotbar = m_registry.TryGetComponent<HotbarComponent>(m_player);
    if (!hotbar)
        return;

    HotbarStateMessage state;
    for (std::size_t i = 0; i < HotbarComponent::kSlotCount; ++i)
    {
        const HotbarSlot& slot = hotbar->slots[i];
        HotbarStateMessage::SlotView view;
        view.type = slot.type;
        switch (slot.type)
        {
        case HotbarSlotType::Technique:
            if (const Technique* technique = m_techniques.Find(slot.id))
                view.name = technique->name;
            break;
        case HotbarSlotType::PhotonArt:
            if (const PhotonArt* art = m_photon_arts.Find(slot.id))
                view.name = art->name;
            break;
        case HotbarSlotType::Item:
            view.name = "(item)";
            break;
        case HotbarSlotType::Empty:
        default:
            break;
        }
        state.slots[i] = view;
    }
    Publish(state);
}

void GameplayLayer::OnEvent(Event& event)
{
    if (!m_turn_coordinator || !m_grid)
        return;

    // Hotbar key-press trigger only intercepts keys while the player is free
    // to act (Exploring on top, not already mid-target-select).
    if (m_state_machine.Top() == &m_exploring_state)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<KeyPressedEvent>(
            [this](KeyPressedEvent& key_event)
            {
                const std::optional<int> slot = KeyCodeToHotbarSlot(key_event.GetKeyCode());
                return slot.has_value() && TryActivateSlot(*slot);
            });
    }

    if (event.handled)
        return;

    GameplayContext context{m_registry, *m_grid, *m_turn_coordinator, m_player};
    m_state_machine.HandleEvent(event, context);
}

} // namespace psr
