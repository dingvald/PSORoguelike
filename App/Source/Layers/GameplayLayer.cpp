#include "Layers/GameplayLayer.h"

#include "Actions/DropAction.h"
#include "Actions/PhotonArtAction.h"
#include "Actions/TechniqueAction.h"
#include "Actions/UseItemAction.h"
#include "ApplicationFilepaths.h"
#include "Combat/PhotonArt.h"
#include "Combat/PhotonArtLibraryFile.h"
#include "Combat/StatusEffectLibraryFile.h"
#include "Combat/Technique.h"
#include "Combat/TechniqueLibraryFile.h"
#include "Components/ConsumableComponent.h"
#include "Components/CurrencyComponent.h"
#include "Components/EnergyComponent.h"
#include "Components/EquipmentComponent.h"
#include "Components/HotbarComponent.h"
#include "Components/InnateWeaponComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Components/RegisterComponents.h"
#include "Components/RenderableComponent.h"
#include "Components/SectionIdComponent.h"
#include "Components/TPComponent.h"
#include "Components/WeaponComponent.h"
#include "Content/KeyBindings.h"
#include "Engine/Dungeon/DungeonInstantiator.h"
#include "Engine/Dungeon/DungeonLibrary.h"
#include "Engine/Dungeon/DungeonLibraryFile.h"
#include "Engine/Dungeon/DungeonStitcher.h"
#include "Engine/Dungeon/PieceLibraryFile.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/JsonEntityLoader.h"
#include "Engine/ECS/NameIdRegistry.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Persistence/JsonDirectoryLoader.h"
#include "Engine/Render/TileVertexMath.h"
#include "Items/CharacterScreenSnapshot.h"
#include "Items/Equip.h"
#include "Items/Hotbar.h"
#include "Layers/HudLayer.h"
#include "Messages/CharacterScreenMessage.h"
#include "Messages/EquipmentSlotActivatedMessage.h"
#include "Messages/FloatingTextStateMessage.h"
#include "Messages/GameRestartedMessage.h"
#include "Messages/HotbarSlotActivatedMessage.h"
#include "Messages/HotbarSlotAssignedMessage.h"
#include "Messages/HotbarStateMessage.h"
#include "Messages/HudReadyMessage.h"
#include "Messages/InventoryItemActivatedMessage.h"
#include "Messages/MesetaChangedMessage.h"
#include "Messages/RestartRequestedMessage.h"
#include "Messages/TechniquesScreenSlotAssignedMessage.h"
#include "States/GameState.h"

#include <entt/core/hashed_string.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_keycode.h>

#include <optional>
#include <stdexcept>
#include <string>

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

    // The two starter Item hotbar slots' bound consumable prefabs -- ids only,
    // not authored data (see the "Default hotbar loadout" comment below).
    // Content authoring (consumables/monomate.json, consumables/monofluid.json,
    // through the Prefab Editor's new Consumable card) is the user's own work,
    // per CLAUDE.md's division of labor -- until authored, these ids simply
    // never match anything in the player's inventory, so the slots stay
    // inert rather than erroring.
    constexpr const char* kMonomatePrefabId = "monomate";
    constexpr const char* kMonofluidPrefabId = "monofluid";

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
    LoadNewGame();

    Subscribe<HotbarSlotActivatedMessage>(&GameplayLayer::OnHotbarSlotActivated, this);
    Subscribe<HudReadyMessage>(&GameplayLayer::OnHudReady, this);
    Subscribe<RestartRequestedMessage>(&GameplayLayer::OnRestartRequested, this);
    Subscribe<InventoryItemActivatedMessage>(&GameplayLayer::OnInventoryItemActivated, this);
    Subscribe<EquipmentSlotActivatedMessage>(&GameplayLayer::OnEquipmentSlotActivated, this);
    Subscribe<HotbarSlotAssignedMessage>(&GameplayLayer::OnHotbarSlotAssigned, this);
    Subscribe<TechniquesScreenSlotAssignedMessage>(&GameplayLayer::OnTechniquesScreenSlotAssigned, this);

    PushOverlay<HudLayer>();

    GameplayContext context{m_registry, *m_grid, *m_turn_coordinator, m_player, GetMessageBus()};
    m_state_machine.Push(m_exploring_state, context);
}

void GameplayLayer::LoadNewGame()
{
    // Discards the previous run's whole ECS world in one move -- every other
    // member that holds a Registry&/Registry* into m_registry
    // (m_turn_coordinator, m_renderable_lookup, ...) stays valid across this,
    // since m_registry's own address never changes, only the entt::registry
    // it wraps. A no-op the first time this runs (OnAttach's default-
    // constructed m_registry is already empty), so LoadNewGame() doesn't need
    // to know whether it's an initial load or a restart.
    m_registry = Registry();
    m_pending_slot_action.reset();

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

    m_pieces = LoadPieceLibrary(ApplicationFilepaths::PiecesPath);
    m_photon_arts = LoadPhotonArtLibrary(ApplicationFilepaths::PhotonArtsPath);
    m_techniques = LoadTechniqueLibrary(ApplicationFilepaths::TechniquesPath);

    // Created before dungeon generation below so CombatLogBridge (constructed
    // right after) can Subscribe() every enemy on_enemy_spawned stamps,
    // including the ones InstantiateDungeon spawns immediately --
    // Position/PlayerControlledComponent/HealthComponent/EnergyComponent are
    // still emplaced later, once instantiation.entrance_tile is known;
    // nothing this entity carries yet (innate_weapon/blocks_movement/
    // renderable, from player.json) needs the grid or dungeon to exist first.
    m_player = m_registry.CreateEntity(entt::hashed_string::value(kPlayerPrefabId));

    // Bridges per-entity combat events onto the Layer MessageBus for HudLayer
    // to consume -- see CombatLogBridge.h. Subscribed to the player above and
    // to every enemy via on_enemy_spawned below, so HudLayer's HP bar updates
    // whether the player is the attacker or the target.
    m_combat_log_bridge.emplace(m_registry, GetMessageBus(), m_techniques, m_photon_arts, m_status_effects, m_player);
    m_combat_log_bridge->Subscribe(Entity(m_registry, m_player));
    m_damage_text_system.Subscribe(Entity(m_registry, m_player));

    const DungeonLibrary dungeons = LoadDungeonLibrary(ApplicationFilepaths::DungeonsPath);
    const Dungeon* dungeon = dungeons.Find(entt::hashed_string::value(kDungeonId));
    if (!dungeon)
        throw std::runtime_error(std::string("GameplayLayer: no '") + kDungeonId + "' dungeon definition found");

    const DungeonLayout layout = GenerateDungeon(*dungeon, m_pieces, m_rng());

    const Rect bounds = ComputeDungeonBounds(layout, m_pieces);
    if (bounds.Empty())
        throw std::runtime_error("GameplayLayer: generated dungeon has no cells");
    m_grid.emplace(bounds.size.x, bounds.size.y);

    // Must happen before any HealthComponent-carrying entity that could die
    // is created (InstantiateDungeon below, then the player) -- DeathSystem
    // resolves Registry::GetGrid() when it removes a dying entity from tile
    // occupancy.
    m_registry.SetGrid(*m_grid);

    // Writes an eased alpha into RenderableComponent::color_1/color_2 --
    // VisualEffectSystem (Core) can't name that App-only type itself, so this
    // callback is how it reaches through without knowing what it's writing to
    // (see VisualEffectSystem.h's own doc comment).
    m_visual_effects.emplace(m_registry, *m_grid,
                             [this](entt::entity entity, std::uint8_t alpha)
                             {
                                 if (RenderableComponent* renderable =
                                         m_registry.TryGetComponent<RenderableComponent>(entity))
                                 {
                                     renderable->color_1.a = alpha;
                                     renderable->color_2.a = alpha;
                                 }
                             });
    m_miss_flash_effect_system.emplace(*m_visual_effects, m_player);
    m_miss_flash_effect_system->Subscribe(Entity(m_registry, m_player));

    // Must be constructed before any entity's EnergyComponent is emplaced --
    // TurnQueue membership is driven by TurnCoordinator's own
    // OnConstruct<EnergyComponent> listener, wired in its constructor. That
    // includes enemies' EnergyComponent below (via on_enemy_spawned, run from
    // InstantiateDungeon), not just the player's -- constructing this any
    // later left the dungeon's first enemy wave emplaced before the listener
    // existed, so they never joined the turn queue and never acted.
    m_turn_coordinator.emplace(m_registry);
    m_turn_coordinator->KeyBindings() = CreateDefaultKeyBindings(*m_grid, m_affixes, m_rng, GetMessageBus());

    // Piece-authored PieceSpawn entries are creatures, not static dungeon
    // furniture -- DungeonInstantiator/SpawnWaveSystem only stamp them with
    // Position/grid membership/SpawnWaveComponent (all Core-level), so this
    // hook does the remaining App-level setup Core can't: joining the turn
    // queue, equipping an authored innate weapon, and wiring the enemy into
    // CombatLogBridge the same way the player is above (so an enemy hitting
    // the player still gets a PlayerStatusMessage published -- AfterDamageEvent
    // is dispatched at the *source*, see DamageEvent.h, so the player being
    // hit only reaches CombatLogBridge via the attacking enemy's own
    // subscription).
    const auto on_enemy_spawned = [this](entt::entity entity)
    {
        m_registry.Emplace<EnergyComponent>(entity);
        if (const auto* innate = m_registry.TryGetComponent<InnateWeaponComponent>(entity))
        {
            const entt::entity weapon = m_registry.CreateEntity(innate->weapon_prefab_id);
            m_registry.Emplace<EquipmentComponent>(entity, EquipmentComponent{weapon});
        }
        m_combat_log_bridge->Subscribe(Entity(m_registry, entity));
        m_damage_text_system.Subscribe(Entity(m_registry, entity));
        m_miss_flash_effect_system->Subscribe(Entity(m_registry, entity));
    };

    const DungeonInstantiation instantiation =
        InstantiateDungeon(layout, m_pieces, -bounds.origin, m_registry, *m_grid, on_enemy_spawned);

    m_room_map.emplace(instantiation.room_map);
    m_room_visibility.emplace(layout.pieces.size(), instantiation.room_adjacency);
    m_room_visibility->Update(m_room_map->GetRoom(instantiation.entrance_tile));

    m_spawn_wave_system.emplace(m_registry, *m_grid, instantiation.initial_wave_counts,
                                instantiation.pending_spawn_waves, on_enemy_spawned);

    m_enemy_ai_system.emplace(*m_grid, m_registry, m_affixes, m_rng);
    m_turn_coordinator->SetNpcDecision([this](Entity actor) { return m_enemy_ai_system->Decide(actor); });

    m_registry.Emplace<Position>(m_player, Position{instantiation.entrance_tile});
    m_registry.Emplace<PlayerControlledComponent>(m_player);
    m_registry.Emplace<HealthComponent>(m_player, HealthComponent{40, 40});
    // Same "hardcoded until M10.3 character creation exists" deferral as
    // HealthComponent above -- there's no Section ID picker yet, and no drop
    // has happened yet to credit any Meseta.
    m_registry.Emplace<SectionIdComponent>(m_player);
    m_registry.Emplace<CurrencyComponent>(m_player);
    m_registry.Emplace<InventoryComponent>(m_player);
    m_grid->AddEntity(instantiation.entrance_tile, m_player);
    m_camera.SetTarget(instantiation.entrance_tile);

    m_registry.Emplace<EnergyComponent>(m_player); // enqueues the player into the turn queue

    m_loot_drop_system.emplace(m_registry, *m_grid, GetMessageBus(), m_rng);
    m_loot_drop_system->Subscribe(Entity(m_registry, m_player));

    // Same auto-equip-on-spawn mechanism enemies use (see on_enemy_spawned
    // above) -- there's no interactive equip/inventory system yet (M8.1's UI
    // bullet is deliberately deferred), so the player's starting weapon is
    // authored the same way an enemy's innate weapon is: a weapon_prefab_id
    // on InnateWeaponComponent, resolved into a live weapon entity here.
    if (const auto* innate = m_registry.TryGetComponent<InnateWeaponComponent>(m_player))
    {
        const entt::entity weapon = m_registry.CreateEntity(innate->weapon_prefab_id);
        m_registry.Emplace<EquipmentComponent>(m_player, EquipmentComponent{weapon});
    }

    // Default hotbar loadout: first 4 weapon-granted Photon Arts into slots
    // 4-7 (mirrors the old placeholder cast trigger's fixed key ranges, now
    // captured as data instead of re-derived by key range on every press).
    // Technique slots (0-3) deliberately start Empty -- nothing is known at
    // spawn (see KnownTechniquesComponent.h); the player assigns them
    // manually via the Techniques/Photon Arts screen ('T') once something is
    // learned, same manual-assign flow Item slots already use. Slots 8-9 are
    // Item slots bound to the two starter consumable prefab ids (see
    // kMonomatePrefabId/kMonofluidPrefabId above) -- same "bind by prefab
    // NameId, resolve to an inventory index at activation time" style
    // PhotonArt slots already use, see TryActivateSlot's Item case.
    HotbarComponent hotbar;
    if (const EquipmentComponent* equipment = m_registry.TryGetComponent<EquipmentComponent>(m_player);
        equipment && equipment->weapon != entt::null)
    {
        if (const WeaponComponent* weapon = m_registry.TryGetComponent<WeaponComponent>(equipment->weapon))
        {
            std::size_t slot = 4;
            for (std::uint32_t photon_art_id : weapon->photon_art_ids)
            {
                if (slot >= 8)
                    break;
                hotbar.slots[slot++] = HotbarSlot{HotbarSlotType::PhotonArt, photon_art_id};
            }
        }
    }
    hotbar.slots[8] = HotbarSlot{HotbarSlotType::Item, entt::hashed_string::value(kMonomatePrefabId)};
    hotbar.slots[9] = HotbarSlot{HotbarSlotType::Item, entt::hashed_string::value(kMonofluidPrefabId)};
    m_registry.Emplace<HotbarComponent>(m_player, hotbar);

    m_status_effect_markers.emplace(m_registry, *m_grid, m_status_effects);
    m_status_effect_markers->Subscribe(Entity(m_registry, m_player));
}

void GameplayLayer::OnRestartRequested(const RestartRequestedMessage& /*message*/)
{
    LoadNewGame();

    // GameOverState never replaced ExploringState -- it was pushed on top
    // (see ExploringState::Update's PlayerDefeated case), so popping it here
    // uncovers the same ExploringState instance, now driving the fresh
    // TurnCoordinator/registry LoadNewGame() just built.
    GameplayContext context{m_registry, *m_grid, *m_turn_coordinator, m_player, GetMessageBus()};
    m_state_machine.Pop(context);

    // HudLayer cached the previous run's HP/TP/hotbar; every entity/component
    // that produced them was just discarded by LoadNewGame(), so republish
    // fresh values the same way OnHudReady() does after HudLayer's own
    // (re)attach.
    PublishHotbarState();
    m_combat_log_bridge->PublishPlayerStatus();
    m_combat_log_bridge->PublishStatusEffects();
    Publish(GameRestartedMessage{});
}

void GameplayLayer::OnUpdate(float delta_time)
{
    HandleQueuedMessages();

    if (!m_turn_coordinator || !m_grid)
        return;

    GameplayContext context{m_registry, *m_grid, *m_turn_coordinator, m_player, GetMessageBus()};
    m_state_machine.Update(context, delta_time);

    if (m_registry.IsValid(m_player))
    {
        const Vec2 player_tile = m_registry.GetComponent<Position>(m_player).tile;
        m_camera.SetTarget(player_tile);
        m_room_visibility->Update(m_room_map->GetRoom(player_tile));
    }
    m_camera.Update(delta_time);

    m_floating_text.Update(delta_time);
    m_visual_effects->Update(delta_time);
    m_animation_clock.Update(delta_time);
    PublishFloatingTextState();
}

void GameplayLayer::EnsureRenderResources(SDL_Renderer& renderer)
{
    if (m_tile_renderer)
        return;

    m_atlas.emplace(renderer, ApplicationFilepaths::TexturesPath);
    m_gpu_pipeline.emplace(renderer, ApplicationFilepaths::ShadersPath / "TileSprite.vert.spv",
                           ApplicationFilepaths::ShadersPath / "TileSprite.frag.spv");
    m_renderable_lookup.emplace(m_registry, m_animation_clock);
    m_fog_lookup.emplace(m_registry, *m_room_map, *m_room_visibility, *m_renderable_lookup);
    m_tile_renderer.emplace(*m_grid, *m_atlas, *m_gpu_pipeline, *m_fog_lookup, kTileWidth, kTileHeight);
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
    m_last_render_width = width;
    m_last_render_height = height;
    m_tile_renderer->Draw(*renderer, m_camera.GetPosition(), width, height, /*zoom=*/1.0f, m_camera.GetRenderOffset());
}

bool GameplayLayer::TryActivateSlot(int slot_index)
{
    if (!m_registry.IsValid(m_player) || slot_index < 0 || slot_index >= static_cast<int>(HotbarComponent::kSlotCount))
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

        m_pending_slot_action = std::make_unique<TechniqueAction>(*m_grid, m_techniques, m_affixes, slot.id, m_rng);
        m_turn_coordinator->RequestTargeting(TargetRequest{m_pending_slot_action.get(), technique->targeting_mode,
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

        m_pending_slot_action = std::make_unique<PhotonArtAction>(*m_grid, m_photon_arts, m_affixes, slot.id, m_rng);
        m_turn_coordinator->RequestTargeting(
            TargetRequest{m_pending_slot_action.get(), art->targeting_mode, art->range_shape, art->range});
        return true;
    }
    case HotbarSlotType::Item:
    {
        // slot.id is the consumable prefab's NameId (same binding style as
        // Technique/PhotonArt above), not an inventory index -- an index
        // would go stale as the inventory reshuffles. Resolve it to whichever
        // inventory slot currently holds a matching, consumable-tagged item;
        // no target-select detour needed, item use is always self-targeted.
        const InventoryComponent* inventory = m_registry.TryGetComponent<InventoryComponent>(m_player);
        if (!inventory)
            return false;

        int found_index = -1;
        for (std::size_t i = 0; i < inventory->items.size(); ++i)
        {
            const entt::entity item = inventory->items[i];
            const PrefabIdComponent* prefab_id = m_registry.TryGetComponent<PrefabIdComponent>(item);
            if (prefab_id && prefab_id->value == slot.id && m_registry.HasComponent<ConsumableComponent>(item))
            {
                found_index = static_cast<int>(i);
                break;
            }
        }
        if (found_index < 0)
            return false;

        m_pending_slot_action = std::make_unique<UseItemAction>(found_index);
        m_turn_coordinator->SetPendingAction(m_pending_slot_action.get());
        return true;
    }
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

void GameplayLayer::OnInventoryItemActivated(const InventoryItemActivatedMessage& message)
{
    if (m_state_machine.Top() != &m_character_screen_state || !m_registry.IsValid(m_player))
        return;

    switch (message.action)
    {
    case InventoryItemAction::Equip:
        if (EquipItem(Entity(m_registry, m_player), message.inventory_index))
            PublishCharacterScreenState();
        return;
    case InventoryItemAction::Use:
        m_pending_slot_action = std::make_unique<UseItemAction>(message.inventory_index);
        break;
    case InventoryItemAction::Drop:
        m_pending_slot_action = std::make_unique<DropAction>(*m_grid, message.inventory_index);
        break;
    }

    m_turn_coordinator->SetPendingAction(m_pending_slot_action.get());
    m_character_screen_state.RequestClose();
}

void GameplayLayer::OnEquipmentSlotActivated(const EquipmentSlotActivatedMessage& message)
{
    if (m_state_machine.Top() != &m_character_screen_state || !m_registry.IsValid(m_player))
        return;

    if (UnequipSlot(Entity(m_registry, m_player), message.slot))
        PublishCharacterScreenState();
}

void GameplayLayer::OnHotbarSlotAssigned(const HotbarSlotAssignedMessage& message)
{
    if (m_state_machine.Top() != &m_character_screen_state || !m_registry.IsValid(m_player))
        return;

    if (AssignItemToHotbarSlot(Entity(m_registry, m_player), message.inventory_index, message.hotbar_slot))
        PublishHotbarState();
}

void GameplayLayer::OnTechniquesScreenSlotAssigned(const TechniquesScreenSlotAssignedMessage& message)
{
    if (m_state_machine.Top() != &m_techniques_screen_state || !m_registry.IsValid(m_player))
        return;

    if (AssignAbilityToHotbarSlot(Entity(m_registry, m_player), message.type, message.id, message.hotbar_slot))
        PublishHotbarState();
}

void GameplayLayer::PublishCharacterScreenState()
{
    if (!m_registry.IsValid(m_player))
        return;

    Publish(BuildCharacterScreenMessage(m_registry, m_player, m_affixes));
}

void GameplayLayer::PublishFloatingTextState()
{
    FloatingTextStateMessage state;
    state.entries.reserve(m_floating_text.Active().size());

    for (const FloatingTextInstance& instance : m_floating_text.Active())
    {
        const PixelPosition pixel = TileToPixel(
            instance.origin_tile, instance.offset, m_camera.GetPosition(), m_last_render_width, m_last_render_height,
            static_cast<float>(kTileWidth), static_cast<float>(kTileHeight), m_camera.GetRenderOffset());
        state.entries.push_back(FloatingTextStateMessage::Entry{pixel.x, pixel.y, instance.text, instance.color});
    }

    Publish(state);
}

void GameplayLayer::OnHudReady(const HudReadyMessage& /*message*/)
{
    PublishHotbarState();
    if (m_combat_log_bridge)
    {
        m_combat_log_bridge->PublishPlayerStatus();
        m_combat_log_bridge->PublishStatusEffects();
    }
    if (m_registry.IsValid(m_player))
    {
        if (const CurrencyComponent* currency = m_registry.TryGetComponent<CurrencyComponent>(m_player))
            Publish(MesetaChangedMessage{currency->meseta, 0});
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
            // slot.id is a consumable prefab's NameId (see TryActivateSlot);
            // NameIdRegistry::Find resolves it back to the id string JsonEntityLoader
            // originally hashed, same lookup ItemDisplayName.h uses for a live item
            // instance -- falls back to the placeholder stub for an unbound slot.
            view.name = "(item)";
            if (const std::optional<std::string> label = NameIdRegistry::Find(slot.id))
                view.name = *label;
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

    // Key-up must reach the input buffer no matter which GameState is on
    // top -- unlike presses, releases aren't gated to ExploringState, since a
    // Move's TweenComponent pushes AnimationState for the animation's
    // duration and a release landing in that window would otherwise never
    // clear the buffer's held-key state, leaving it auto-repeating the last
    // direction indefinitely.
    EventDispatcher release_dispatcher(event);
    release_dispatcher.Dispatch<KeyReleasedEvent>(
        [this](KeyReleasedEvent& key_event)
        {
            m_turn_coordinator->ReleaseKey(key_event.GetKeyCode());
            return true;
        });
    if (event.handled)
        return;

    // Hotbar key-press trigger and the Character-screen toggle only
    // intercept keys while the player is free to act (Exploring on top, not
    // already mid-target-select or already viewing the Character screen --
    // closing the latter is CharacterScreenState's own HandleEvent's job,
    // reached via m_state_machine.HandleEvent below once it's on top).
    if (m_state_machine.Top() == &m_exploring_state)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<KeyPressedEvent>(
            [this](KeyPressedEvent& key_event)
            {
                const std::optional<int> slot = KeyCodeToHotbarSlot(key_event.GetKeyCode());
                if (slot.has_value())
                    return TryActivateSlot(*slot);

                if (key_event.GetKeyCode() == SDLK_C)
                {
                    GameplayContext context{m_registry, *m_grid, *m_turn_coordinator, m_player, GetMessageBus()};
                    m_state_machine.Push(m_character_screen_state, context);
                    return true;
                }

                if (key_event.GetKeyCode() == SDLK_T)
                {
                    GameplayContext context{m_registry, *m_grid, *m_turn_coordinator, m_player, GetMessageBus()};
                    m_state_machine.Push(m_techniques_screen_state, context);
                    return true;
                }

                return false;
            });
    }

    if (event.handled)
        return;

    GameplayContext context{m_registry, *m_grid, *m_turn_coordinator, m_player, GetMessageBus()};
    m_state_machine.HandleEvent(event, context);
}

} // namespace psr
