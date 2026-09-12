#include "Layers/GameplayLayer.h"

#include "Actions/DropAction.h"
#include "Actions/PhotonArtAction.h"
#include "Actions/TechniqueAction.h"
#include "Actions/UseItemAction.h"
#include "Actions/WeaponAttackAction.h"
#include "ApplicationFilepaths.h"
#include "Areas/AreaLibraryFile.h"
#include "Combat/DisplayName.h"
#include "Combat/PhotonArt.h"
#include "Combat/PhotonArtLibraryFile.h"
#include "Combat/StatusEffectLibraryFile.h"
#include "Combat/Technique.h"
#include "Combat/TechniqueLibraryFile.h"
#include "Components/ActorComponent.h"
#include "Components/ConsumableComponent.h"
#include "Components/CurrencyComponent.h"
#include "Components/EquipmentComponent.h"
#include "Components/HotbarComponent.h"
#include "Components/InnateWeaponComponent.h"
#include "Components/InteractableComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/KnownTechniquesComponent.h"
#include "Components/LevelComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Components/ProjectileComponent.h"
#include "Components/RaceComponent.h"
#include "Components/RangedTechComponent.h"
#include "Components/RegisterComponents.h"
#include "Components/RenderableComponent.h"
#include "Components/SectionIdComponent.h"
#include "Components/StatsComponent.h"
#include "Components/StorageComponent.h"
#include "Components/TPComponent.h"
#include "Components/TabTargetComponent.h"
#include "Components/TeleporterComponent.h"
#include "Components/WeaponComponent.h"
#include "Content/KeyBindings.h"
#include "Engine/Dungeon/DungeonInstantiator.h"
#include "Engine/Dungeon/DungeonLibraryFile.h"
#include "Engine/Dungeon/DungeonStitcher.h"
#include "Engine/Dungeon/PieceLibraryFile.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/JsonEntityLoader.h"
#include "Engine/ECS/LifetimeComponent.h"
#include "Engine/ECS/NameIdRegistry.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/PrefabIdComponent.h"
#include "Engine/Events/Event.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Persistence/JsonDirectoryLoader.h"
#include "Engine/Render/TileVertexMath.h"
#include "Hub/HubDefinitionFile.h"
#include "Hub/HubInteraction.h"
#include "Items/CharacterScreenSnapshot.h"
#include "Items/Equip.h"
#include "Items/EquipPreview.h"
#include "Items/Hotbar.h"
#include "Items/Shop.h"
#include "Shop/ShopSnapshot.h"
#include "Items/Storage.h"
#include "Items/StorageSnapshot.h"
#include "Layers/HudLayer.h"
#include "Messages/CharacterScreenMessage.h"
#include "Messages/EquipmentSlotActivatedMessage.h"
#include "Messages/FloatingTextStateMessage.h"
#include "Messages/GameRestartedMessage.h"
#include "Messages/HotbarSlotActivatedMessage.h"
#include "Messages/HotbarSlotAssignedMessage.h"
#include "Messages/HotbarStateMessage.h"
#include "Messages/HubInteractionPromptMessage.h"
#include "Messages/HudReadyMessage.h"
#include "Messages/CharacterScreenStatPreviewMessage.h"
#include "Messages/InventoryItemActivatedMessage.h"
#include "Messages/InventoryItemHoverChangedMessage.h"
#include "Messages/MesetaChangedMessage.h"
#include "Messages/MissionCompletedMessage.h"
#include "Messages/MissionSelectedMessage.h"
#include "Messages/RestartRequestedMessage.h"
#include "Messages/ShopBuyRequestedMessage.h"
#include "Messages/ShopMessage.h"
#include "Messages/ShopSellRequestedMessage.h"
#include "Messages/StorageItemActivatedMessage.h"
#include "Messages/StorageMessage.h"
#include "Messages/StorageWithdrawRequestedMessage.h"
#include "Messages/TargetStateMessage.h"
#include "Messages/ActionPaletteSlotAssignedMessage.h"
#include "Messages/TeleporterPromptMessage.h"
#include "Missions/AreaProgression.h"
#include "Missions/TeleporterInteraction.h"
#include "Progression/GrowthCurveFile.h"
#include "Shop/ShopStockFile.h"
#include "States/GameState.h"

#include <entt/core/hashed_string.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_keycode.h>

#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace psr {

namespace {

    constexpr int kEntitySchemaVersion = 1;
    constexpr int kTileWidth = 16;
    constexpr int kTileHeight = 24;
    constexpr float kCameraZoomStep = 0.5f;

    // The player's prefab -- appearance (and, later, base stats) lives in
    // App/Assets/Data/Entities/player.json like every other authored entity,
    // not hand-built here. PlayerControlledComponent/Position are never
    // authored on it (both are engine-managed, authorable=false) and are
    // emplaced onto the spawned instance separately below. ActorComponent is
    // technically authorable now (see its own doc comment) but is still
    // emplaced separately below rather than authored, so TurnQueue membership
    // timing stays under this constructor's explicit control.
    constexpr const char* kPlayerPrefabId = "player";

    // The two starter Item hotbar slots' bound consumable prefabs -- ids only,
    // not authored data (see the "Default hotbar loadout" comment below).
    // Both prefabs (App/Assets/Data/Entities/monomate.json, monofluid.json)
    // are authored; an unauthored id here would simply never match anything
    // in the player's inventory, leaving that slot inert rather than erroring.
    constexpr const char* kMonomatePrefabId = "monomate";
    constexpr const char* kMonofluidPrefabId = "monofluid";

    // Played over every freshly spawned enemy's tile (see on_enemy_spawned
    // below), one in-game turn long (LifetimeComponent{1}) -- signals to the
    // player that this enemy just appeared. App/Assets/Data/Entities/vfx/
    // enemy_spawn.json is a placeholder (non-animated) prefab; the real
    // animated artwork is content for the user to author via
    // PrefabEditorLayer, same authoring path as the other vfx/* prefabs.
    constexpr const char* kEnemySpawnEffectPrefabId = "vfx.enemy_spawn";

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
    SpawnNewCharacter();

    Subscribe<HotbarSlotActivatedMessage>(&GameplayLayer::OnHotbarSlotActivated, this);
    Subscribe<HudReadyMessage>(&GameplayLayer::OnHudReady, this);
    Subscribe<RestartRequestedMessage>(&GameplayLayer::OnRestartRequested, this);
    Subscribe<InventoryItemActivatedMessage>(&GameplayLayer::OnInventoryItemActivated, this);
    Subscribe<InventoryItemHoverChangedMessage>(&GameplayLayer::OnInventoryItemHoverChanged, this);
    Subscribe<EquipmentSlotActivatedMessage>(&GameplayLayer::OnEquipmentSlotActivated, this);
    Subscribe<HotbarSlotAssignedMessage>(&GameplayLayer::OnHotbarSlotAssigned, this);
    Subscribe<ActionPaletteSlotAssignedMessage>(&GameplayLayer::OnActionPaletteSlotAssigned, this);
    Subscribe<MissionSelectedMessage>(&GameplayLayer::OnMissionSelected, this);
    Subscribe<ShopBuyRequestedMessage>(&GameplayLayer::OnShopBuyRequested, this);
    Subscribe<ShopSellRequestedMessage>(&GameplayLayer::OnShopSellRequested, this);
    Subscribe<StorageItemActivatedMessage>(&GameplayLayer::OnStorageItemActivated, this);
    Subscribe<StorageWithdrawRequestedMessage>(&GameplayLayer::OnStorageWithdrawRequested, this);

    PushOverlay<HudLayer>();

    GameplayContext context{m_registry, *m_grid, *m_turn_coordinator, m_player, GetMessageBus()};
    m_state_machine.Push(m_exploring_state, context);
}

void GameplayLayer::SpawnNewCharacter()
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

    // Content libraries loaded once for the whole process lifetime -- none
    // of this changes at runtime, so there's no reason to reload any of it
    // on a later scene swap (unlike the OLD single-dungeon LoadNewGame,
    // which reloaded everything on every restart because it also reset the
    // whole Registry each time).
    m_areas = LoadAreaLibrary(ApplicationFilepaths::AreasPath);
    m_pieces = LoadPieceLibrary(ApplicationFilepaths::PiecesPath);
    m_dungeons = LoadDungeonLibrary(ApplicationFilepaths::DungeonsPath);
    m_hub = LoadHubDefinition(ApplicationFilepaths::HubPath);
    m_shop_stock = LoadShopStock(ApplicationFilepaths::ShopStockPath);
    m_photon_arts = LoadPhotonArtLibrary(ApplicationFilepaths::PhotonArtsPath);
    m_techniques = LoadTechniqueLibrary(ApplicationFilepaths::TechniquesPath);
    m_growth_curve = LoadGrowthCurve(ApplicationFilepaths::GrowthCurvePath);

    m_player = m_registry.CreateEntity(entt::hashed_string::value(kPlayerPrefabId));
    // Unlike piece/enemy entities, the player is never routed through
    // DungeonInstantiator/SpawnWaveSystem's placement step (the only other
    // code that emplaces Position), so TransitionToWorld's tile assignment
    // below needs the component to already exist.
    m_registry.Emplace<Position>(m_player);
    m_registry.Emplace<HealthComponent>(m_player, HealthComponent{40, 40});
    // Same "hardcoded until M10.3 character creation exists" deferral as
    // HealthComponent above -- growth_curve.json's level-2 max_tp (24) is the
    // first authored value, so this level-1 baseline is chosen below it the
    // same way HealthComponent's 40 sits below level-2's max_hp of 48.
    m_registry.Emplace<TPComponent>(m_player, TPComponent{20, 20});
    m_registry.Emplace<TabTargetComponent>(m_player);
    m_registry.Emplace<LevelComponent>(m_player);
    // Same "hardcoded until M10.3 character creation exists" deferral as
    // HealthComponent above -- there's no Section ID picker yet, and no drop
    // has happened yet to credit any Meseta.
    m_registry.Emplace<SectionIdComponent>(m_player);
    m_registry.Emplace<CurrencyComponent>(m_player);
    m_registry.Emplace<InventoryComponent>(m_player);
    m_registry.Emplace<StorageComponent>(m_player);

    // Hands off to TransitionToWorld for everything scene-shaped (Grid,
    // TurnCoordinator, per-world systems) -- see its own doc comment. The
    // player already exists (created just above) so it can be placed at the
    // hub's entrance tile and subscribed to the per-world systems
    // TransitionToWorld builds lazily on this first call.
    TransitionToWorld(SceneKind::Hub, std::nullopt);

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
    //
    // Same auto-equip-on-spawn mechanism enemies use (see on_enemy_spawned
    // in TransitionToWorld) -- there's no interactive equip/inventory system
    // yet beyond the Character screen, so the player's starting weapon is
    // authored the same way an enemy's innate weapon is: a weapon_prefab_id
    // on InnateWeaponComponent, resolved into a live weapon entity here.
    if (const auto* innate = m_registry.TryGetComponent<InnateWeaponComponent>(m_player))
    {
        const entt::entity weapon = m_registry.CreateEntity(innate->weapon_prefab_id);
        m_registry.Emplace<EquipmentComponent>(m_player, EquipmentComponent{weapon});
    }

    HotbarComponent hotbar;
    hotbar.slots[0] = HotbarSlot{HotbarSlotType::NormalAttack, 0};
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

    // Both emplaced only now, after TransitionToWorld has lazily constructed
    // m_turn_coordinator above: TurnCoordinator's constructor is where it
    // subscribes OnConstruct<PlayerControlledComponent>/OnConstruct<ActorComponent>
    // to track m_live_player_count/TurnQueue membership, so emplacing either
    // one before that construction would silently miss the signal -- leaving
    // m_live_player_count stuck at 0 and Step() reporting PlayerDefeated
    // immediately, forever (m_turn_coordinator is never rebuilt after this
    // first call).
    m_registry.Emplace<PlayerControlledComponent>(m_player);
    m_registry.Emplace<ActorComponent>(m_player); // enqueues the player into the turn queue
}

void GameplayLayer::DestroyWorldEntities()
{
    std::unordered_set<entt::entity> keep{m_player};

    // The turn-clock sentinel is a plain EventHandlerComponent-bearing
    // entity like any other (see Registry::CreateEntity), so without this it
    // would be swept up below on the very first scene swap after
    // TurnCoordinator's lazy-once construction, silently stopping every
    // turn-counted LifetimeComponent from ever expiring again.
    if (m_turn_coordinator)
        keep.insert(m_turn_coordinator->TimeSentinel());

    if (const InventoryComponent* inventory = m_registry.TryGetComponent<InventoryComponent>(m_player))
        for (entt::entity item : inventory->items)
            keep.insert(item);

    if (const StorageComponent* storage = m_registry.TryGetComponent<StorageComponent>(m_player))
        for (entt::entity item : storage->items)
            keep.insert(item);

    if (const EquipmentComponent* equipment = m_registry.TryGetComponent<EquipmentComponent>(m_player))
        for (entt::entity slot : {equipment->weapon, equipment->head, equipment->torso, equipment->hands,
                                 equipment->legs})
            if (slot != entt::null)
                keep.insert(slot);

    std::vector<entt::entity> to_destroy;
    m_registry.Each<EventHandlerComponent>(
        [&keep, &to_destroy](entt::entity entity, EventHandlerComponent&)
        {
            if (!keep.contains(entity))
                to_destroy.push_back(entity);
        });

    for (entt::entity entity : to_destroy)
        m_registry.DestroyEntity(entity);
}

void GameplayLayer::TransitionToWorld(SceneKind target, std::optional<std::string> dungeon_id_string)
{
    DestroyWorldEntities();

    DungeonLayout layout;
    Rect bounds;
    if (target == SceneKind::Hub)
    {
        const std::uint32_t hub_piece_id = entt::hashed_string::value(m_hub.piece_id_string.c_str());
        layout = DungeonLayout{{PlacedPiece{hub_piece_id, Vec2{0, 0}, PieceTransform{}}}, {}, {}, {}};
        bounds = ComputeDungeonBounds(layout, m_pieces);
        if (bounds.Empty())
            throw std::runtime_error("GameplayLayer: hub piece '" + m_hub.piece_id_string + "' has no cells");
        m_current_dungeon_id.clear();
    }
    else
    {
        const Dungeon* dungeon = m_dungeons.Find(entt::hashed_string::value(dungeon_id_string.value().c_str()));
        if (!dungeon)
            throw std::runtime_error("GameplayLayer: no '" + dungeon_id_string.value() + "' dungeon definition found");

        layout = GenerateDungeon(*dungeon, m_pieces, m_rng());
        bounds = ComputeDungeonBounds(layout, m_pieces);
        if (bounds.Empty())
            throw std::runtime_error("GameplayLayer: generated dungeon has no cells");
        m_current_dungeon_id = dungeon_id_string.value();
    }

    m_grid.emplace(bounds.size.x, bounds.size.y);

    // Must happen before any HealthComponent-carrying entity that could die
    // is created (InstantiateDungeon below) -- DeathSystem resolves
    // Registry::GetGrid() when it removes a dying entity from tile
    // occupancy. Safe (and necessary) to call again on every transition:
    // *m_grid's address is stable (see the class doc comment), so this just
    // keeps the stashed reference pointed at whichever Grid is current.
    m_registry.SetGrid(*m_grid);

    // Writes an eased alpha into RenderableComponent::color_1/color_2 --
    // VisualEffectSystem (Core) can't name that App-only type itself, so this
    // callback is how it reaches through without knowing what it's writing to
    // (see VisualEffectSystem.h's own doc comment). Lazy-once: see the class
    // doc comment for why this and the other player-Subscribe()ing systems
    // below are never rebuilt after their first construction.
    if (!m_visual_effects)
    {
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
        m_on_hit_effect_system.emplace(*m_visual_effects);
        m_on_hit_effect_system->Subscribe(Entity(m_registry, m_player));
        m_heal_effect_system.emplace(*m_visual_effects);
        m_heal_effect_system->Subscribe(Entity(m_registry, m_player));
    }

    // Must be constructed before any entity's ActorComponent is emplaced --
    // TurnQueue membership is driven by TurnCoordinator's own
    // OnConstruct<ActorComponent> listener, wired in its constructor. That
    // includes enemies' ActorComponent below (via on_enemy_spawned, run from
    // InstantiateDungeon), not just the player's. Lazy-once (see the class
    // doc comment): a scene swap never touches the player's ActorComponent/
    // TurnQueue membership, so there's no reason to reconstruct this
    // alongside the world it schedules turns for.
    if (!m_turn_coordinator)
    {
        m_turn_coordinator.emplace(m_registry);
        m_turn_coordinator->KeyBindings() = CreateDefaultKeyBindings(*m_grid, m_affixes, m_rng, GetMessageBus());
    }
    if (!m_lifetime_system)
    {
        m_lifetime_system.emplace(m_registry);
        m_turn_coordinator->SetOnTurnPassed([this] { m_lifetime_system->Tick(); });
    }

    if (!m_combat_log_bridge)
    {
        m_combat_log_bridge.emplace(m_registry, GetMessageBus(), m_techniques, m_photon_arts, m_status_effects,
                                    m_player);
        m_combat_log_bridge->Subscribe(Entity(m_registry, m_player));
        m_damage_text_system.Subscribe(Entity(m_registry, m_player));
        m_heal_text_system.Subscribe(Entity(m_registry, m_player));
        m_turn_coordinator->Subscribe(Entity(m_registry, m_player));
    }
    if (!m_loot_drop_system)
    {
        m_loot_drop_system.emplace(m_registry, *m_grid, GetMessageBus(), m_rng);
        m_loot_drop_system->Subscribe(Entity(m_registry, m_player));
    }
    if (!m_experience_system)
    {
        m_experience_system.emplace(GetMessageBus(), m_growth_curve, m_floating_text);
        m_experience_system->Subscribe(Entity(m_registry, m_player));
    }
    if (!m_status_effect_markers)
    {
        m_status_effect_markers.emplace(m_registry, *m_grid, m_status_effects);
        m_status_effect_markers->Subscribe(Entity(m_registry, m_player));
    }
    if (!m_switch_trigger_system)
    {
        m_switch_trigger_system.emplace(m_registry, *m_grid);
        m_switch_trigger_system->Subscribe(Entity(m_registry, m_player));
    }

    // Piece-authored PieceSpawn entries are creatures, not static dungeon
    // furniture -- DungeonInstantiator/SpawnWaveSystem only stamp them with
    // Position/grid membership/SpawnWaveComponent (all Core-level), so this
    // hook does the remaining App-level setup Core can't: joining the turn
    // queue, equipping an authored innate weapon, and wiring the enemy into
    // the player-subscribed systems above the same way the player already
    // is. Always subscribes a *fresh* entity (every enemy spawn is a brand
    // new entt::entity, never the persisting player), so unlike the
    // lazy-once blocks above, this runs every time regardless.
    const auto on_enemy_spawned = [this](entt::entity entity)
    {
        m_registry.GetOrEmplace<ActorComponent>(entity);
        if (const auto* innate = m_registry.TryGetComponent<InnateWeaponComponent>(entity))
        {
            const entt::entity weapon = m_registry.CreateEntity(innate->weapon_prefab_id);
            m_registry.Emplace<EquipmentComponent>(entity, EquipmentComponent{weapon});
        }
        // Bridges AiBehavior::RangedTechAtDistance's authored RangedTechComponent
        // (template data, same "bake it onto the concrete spawned instance"
        // role InnateWeaponComponent plays above) into the real
        // KnownTechniquesComponent TechniqueAction actually gates casting on --
        // that component is deliberately player-oriented/non-authorable (see
        // its own doc comment), so this is the only way an enemy prefab can
        // "know" a technique.
        if (const auto* ranged_tech = m_registry.TryGetComponent<RangedTechComponent>(entity))
            m_registry.Emplace<KnownTechniquesComponent>(
                entity, KnownTechniquesComponent{{KnownTechniqueEntry{ranged_tech->technique_id, 1}}});
        m_combat_log_bridge->Subscribe(Entity(m_registry, entity));
        m_damage_text_system.Subscribe(Entity(m_registry, entity));
        m_heal_text_system.Subscribe(Entity(m_registry, entity));
        m_miss_flash_effect_system->Subscribe(Entity(m_registry, entity));
        m_on_hit_effect_system->Subscribe(Entity(m_registry, entity));
        m_heal_effect_system->Subscribe(Entity(m_registry, entity));
        m_turn_coordinator->Subscribe(Entity(m_registry, entity));

        if (const Position* position = m_registry.TryGetComponent<Position>(entity))
        {
            const auto effect_prefab_id = entt::hashed_string::value(kEnemySpawnEffectPrefabId);
            if (m_registry.HasPrefab(effect_prefab_id))
            {
                const entt::entity effect = m_registry.CreateEntity(effect_prefab_id);
                m_registry.Emplace<Position>(effect, Position{position->tile});
                m_grid->AddEntity(position->tile, effect);
                m_registry.Emplace<LifetimeComponent>(effect, LifetimeComponent{1});
            }
        }
    };

    const DungeonInstantiation instantiation = InstantiateDungeon(layout, m_pieces, -bounds.origin, m_registry, *m_grid);

    m_room_map.emplace(instantiation.room_map);
    m_room_visibility.emplace(layout.pieces.size(), instantiation.room_adjacency);

    m_spawn_wave_system.emplace(m_registry, *m_grid, instantiation.pending_spawn_waves, on_enemy_spawned);
    m_room_clear_door_system.emplace(m_registry, *m_grid, instantiation.pending_spawn_waves,
                                     instantiation.room_cleared_doors, instantiation.room_entry_doors,
                                     layout.locked_door_prefab_id, layout.unlocked_door_prefab_id);

    EnterRoom(instantiation.entrance_tile, m_room_map->GetRoom(instantiation.entrance_tile));

    m_enemy_ai_system.emplace(*m_grid, m_registry, m_affixes, m_techniques, m_rng, on_enemy_spawned);
    m_projectile_advance_action.emplace(*m_grid, m_affixes, m_rng);
    m_tab_target_system.emplace(m_registry, *m_grid, *m_room_map, *m_room_visibility);
    m_turn_coordinator->SetNpcDecision(
        [this](Entity actor) -> IAction*
        {
            // Projectiles aren't AI-driven actors -- route them to their own
            // advance action first, same "check the tag, else fall through
            // to the real decision" shape as TurnCoordinator's own Frozen
            // check (see TurnCoordinator.cpp::Step).
            if (actor.Has<ProjectileComponent>())
                return &*m_projectile_advance_action;
            return m_enemy_ai_system->Decide(actor);
        });

    m_registry.GetComponent<Position>(m_player).tile = instantiation.entrance_tile;
    m_grid->AddEntity(instantiation.entrance_tile, m_player);
    m_camera.SetTarget(instantiation.entrance_tile);

    // Leaving a mission (win, die, or bail) or arriving at the hub always
    // returns the player topped up -- confirmed with the user: no lingering-
    // injury mechanic exists at this milestone.
    HealthComponent& health = m_registry.GetComponent<HealthComponent>(m_player);
    health.current_hp = health.max_hp;
    TPComponent& tp = m_registry.GetComponent<TPComponent>(m_player);
    tp.current_tp = tp.max_tp;

    m_scene = target;
    RepublishHudStateAfterTransition();
}

void GameplayLayer::EnterRoom(Vec2 player_tile, std::optional<std::uint32_t> room)
{
    if (room && room != m_room_visibility->CurrentRoom())
    {
        if (m_room_clear_door_system->IsEntryThresholdTile(*room, player_tile))
            return;
        m_spawn_wave_system->TriggerRoomEntered(*room);
        m_room_clear_door_system->LockRoomOnEntry(*room);
    }
    m_room_visibility->Update(room);
}

void GameplayLayer::RepublishHudStateAfterTransition()
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
    PublishTargetState();
    if (m_scene == SceneKind::Hub)
    {
        PublishHubInteractionPrompt();
        Publish(TeleporterPromptMessage{});
    }
    else
    {
        Publish(HubInteractionPromptMessage{});
        PublishTeleporterPrompt();
    }
}

void GameplayLayer::OnTeleporterActivated(TeleporterDestination destination)
{
    if (destination == TeleporterDestination::ReturnToHub)
    {
        TransitionToWorld(SceneKind::Hub, std::nullopt);
        return;
    }

    const std::uint32_t current_hash = entt::hashed_string::value(m_current_dungeon_id.c_str());
    m_run_progress.completed_dungeon_ids.insert(current_hash);
    Publish(MissionCompletedMessage{m_current_dungeon_id});

    const Dungeon* current = m_dungeons.Find(current_hash);
    const Dungeon* next = current ? NextDungeonInArea(*current, m_areas, m_dungeons) : nullptr;
    if (next)
        TransitionToWorld(SceneKind::Dungeon, next->id_string);
    else
        TransitionToWorld(SceneKind::Hub, std::nullopt);
}

void GameplayLayer::OnRestartRequested(const RestartRequestedMessage& /*message*/)
{
    // Dying returns the player to the hub with gear/Meseta/level intact
    // (full-heal, no reset) instead of the old full-registry wipe -- see the
    // class doc comment. Real permadeath is M11.2's job.
    TransitionToWorld(SceneKind::Hub, std::nullopt);

    // GameOverState never replaced ExploringState -- it was pushed on top
    // (see ExploringState::Update's PlayerDefeated case), so popping it here
    // uncovers the same ExploringState instance, now driving the hub
    // TransitionToWorld just built.
    GameplayContext context{m_registry, *m_grid, *m_turn_coordinator, m_player, GetMessageBus()};
    m_state_machine.Pop(context);

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
        EnterRoom(player_tile, m_room_map->GetRoom(player_tile));
    }
    m_camera.Update(delta_time);

    m_floating_text.Update(delta_time);
    m_visual_effects->Update(delta_time);
    m_animation_clock.Update(delta_time);
    PublishFloatingTextState();

    if (m_registry.IsValid(m_player))
    {
        m_tab_target_system->Update(Entity(m_registry, m_player));
        PublishTargetState();
        if (m_scene == SceneKind::Hub)
            PublishHubInteractionPrompt();
        else
            PublishTeleporterPrompt();
    }
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
    m_tile_renderer->Draw(*renderer, m_camera.GetPosition(), width, height, m_camera.GetZoom(),
                          m_camera.GetRenderOffset());
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
        // Matches TechniqueAction::Perform's own projectile-spawn gate exactly,
        // so TargetSelectionState's travel/area preview always shows what the
        // cast will actually do.
        const bool is_projectile =
            technique->projectile_speed > 0 && (technique->range_shape == WeaponRangeShape::SingleTarget ||
                                                technique->range_shape == WeaponRangeShape::Line);
        TargetRequest request{m_pending_slot_action.get(), technique->targeting_mode, technique->range_shape,
                              technique->range};
        request.is_projectile = is_projectile;
        request.projectile_pierces = technique->projectile_pierces;
        m_turn_coordinator->RequestTargeting(request);
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
    case HotbarSlotType::NormalAttack:
    {
        const EquipmentComponent* equipment = m_registry.TryGetComponent<EquipmentComponent>(m_player);
        if (!equipment || equipment->weapon == entt::null)
            return false;
        const WeaponComponent* weapon = m_registry.TryGetComponent<WeaponComponent>(equipment->weapon);
        if (!weapon)
            return false;

        m_pending_slot_action = std::make_unique<WeaponAttackAction>(*m_grid, m_affixes, m_rng);
        TargetRequest request{m_pending_slot_action.get(), weapon->targeting_mode, weapon->range_shape,
                              weapon->range};
        request.is_projectile = weapon->fires_projectile;
        request.projectile_pierces = weapon->projectile_pierces;
        m_turn_coordinator->RequestTargeting(request);
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

void GameplayLayer::OnInventoryItemHoverChanged(const InventoryItemHoverChangedMessage& message)
{
    if (m_state_machine.Top() != &m_character_screen_state || !m_registry.IsValid(m_player))
        return;

    CharacterScreenStatPreviewMessage response;

    const InventoryComponent* inventory = m_registry.TryGetComponent<InventoryComponent>(m_player);
    if (inventory && message.inventory_index >= 0 &&
        message.inventory_index < static_cast<int>(inventory->items.size()))
    {
        const entt::entity item = inventory->items[static_cast<std::size_t>(message.inventory_index)];
        if (const std::optional<StatsComponent> delta = ComputeEquipStatDelta(m_registry, m_player, item, m_affixes))
        {
            response.active = true;
            response.atp_delta = delta->atp;
            response.ata_delta = delta->ata;
            response.mst_delta = delta->mst;
            response.dfp_delta = delta->dfp;
            response.evp_delta = delta->evp;
            response.lck_delta = delta->lck;
        }
    }

    Publish(response);
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

void GameplayLayer::OnActionPaletteSlotAssigned(const ActionPaletteSlotAssignedMessage& message)
{
    if (m_state_machine.Top() != &m_action_palette_state || !m_registry.IsValid(m_player))
        return;

    if (AssignAbilityToHotbarSlot(Entity(m_registry, m_player), message.type, message.id, message.hotbar_slot))
        PublishHotbarState();
}

void GameplayLayer::OnMissionSelected(const MissionSelectedMessage& message)
{
    if (m_state_machine.Top() != &m_mission_select_state)
        return;

    const Dungeon* dungeon = m_dungeons.Find(entt::hashed_string::value(message.dungeon_id_string.c_str()));
    if (!dungeon || !IsDungeonUnlocked(m_run_progress, *dungeon, m_dungeons, m_areas))
        return;

    GameplayContext context{m_registry, *m_grid, *m_turn_coordinator, m_player, GetMessageBus()};
    m_state_machine.Pop(context);
    TransitionToWorld(SceneKind::Dungeon, message.dungeon_id_string);
}

void GameplayLayer::OnShopBuyRequested(const ShopBuyRequestedMessage& message)
{
    if (m_state_machine.Top() != &m_shop_state || !m_registry.IsValid(m_player))
        return;

    if (BuyItem(Entity(m_registry, m_player), m_shop_stock, message.stock_index))
    {
        Publish(BuildShopMessage(m_registry, m_player, m_shop_stock, m_affixes));
        if (const CurrencyComponent* currency = m_registry.TryGetComponent<CurrencyComponent>(m_player))
            Publish(MesetaChangedMessage{currency->meseta, 0});
    }
}

void GameplayLayer::OnShopSellRequested(const ShopSellRequestedMessage& message)
{
    if (m_state_machine.Top() != &m_shop_state || !m_registry.IsValid(m_player))
        return;

    if (SellItem(Entity(m_registry, m_player), message.inventory_index))
    {
        Publish(BuildShopMessage(m_registry, m_player, m_shop_stock, m_affixes));
        if (const CurrencyComponent* currency = m_registry.TryGetComponent<CurrencyComponent>(m_player))
            Publish(MesetaChangedMessage{currency->meseta, 0});
    }
}

void GameplayLayer::OnStorageItemActivated(const StorageItemActivatedMessage& message)
{
    if (m_state_machine.Top() != &m_storage_state || !m_registry.IsValid(m_player))
        return;

    if (StoreItem(Entity(m_registry, m_player), message.inventory_index))
        Publish(BuildStorageMessage(m_registry, m_player, m_affixes));
}

void GameplayLayer::OnStorageWithdrawRequested(const StorageWithdrawRequestedMessage& message)
{
    if (m_state_machine.Top() != &m_storage_state || !m_registry.IsValid(m_player))
        return;

    if (WithdrawItem(Entity(m_registry, m_player), message.storage_index))
        Publish(BuildStorageMessage(m_registry, m_player, m_affixes));
}

void GameplayLayer::PublishCharacterScreenState()
{
    if (!m_registry.IsValid(m_player))
        return;

    Publish(BuildCharacterScreenMessage(m_registry, m_player, m_affixes, m_growth_curve));
}

void GameplayLayer::PublishFloatingTextState()
{
    FloatingTextStateMessage state;
    state.entries.reserve(m_floating_text.Active().size());

    for (const FloatingTextInstance& instance : m_floating_text.Active())
    {
        const float zoomed_tile_width = static_cast<float>(kTileWidth) * m_camera.GetZoom();
        const float zoomed_tile_height = static_cast<float>(kTileHeight) * m_camera.GetZoom();
        const PixelPosition pixel =
            TileToPixel(instance.origin_tile, instance.offset, m_camera.GetPosition(), m_last_render_width,
                        m_last_render_height, zoomed_tile_width, zoomed_tile_height, m_camera.GetRenderOffset());
        // TileToPixel returns a tile's top-left corner (matches TileRenderer's
        // own sprite-quad anchor) -- shift by half a zoomed tile to land on
        // the tile's centre instead.
        state.entries.push_back(FloatingTextStateMessage::Entry{pixel.x + zoomed_tile_width / 2.0f,
                                                                pixel.y + zoomed_tile_height / 2.0f, m_camera.GetZoom(),
                                                                instance.text, instance.color});
    }

    Publish(state);
}

void GameplayLayer::PublishTargetState()
{
    TargetStateMessage state;
    if (m_registry.IsValid(m_player))
    {
        const TabTargetComponent* tab_target = m_registry.TryGetComponent<TabTargetComponent>(m_player);
        if (tab_target && tab_target->target != entt::null && m_registry.IsValid(tab_target->target))
        {
            Entity target(m_registry, tab_target->target);
            state.has_target = true;
            state.name = DisplayName(m_registry, tab_target->target, m_player);
            if (const RaceComponent* race = target.TryGet<RaceComponent>())
                state.race_label = NameIdRegistry::Find(race->race_id).value_or("Unknown");
            if (const HealthComponent* health = target.TryGet<HealthComponent>())
            {
                state.current_hp = health->current_hp;
                state.max_hp = health->max_hp;
            }
        }
    }
    Publish(state);
}

void GameplayLayer::PublishHubInteractionPrompt()
{
    HubInteractionPromptMessage state;
    if (m_registry.IsValid(m_player) && m_grid)
    {
        const Vec2 player_tile = m_registry.GetComponent<Position>(m_player).tile;
        state.interaction_type = FindInteractableAt(m_registry, *m_grid, player_tile);
    }
    Publish(state);
}

void GameplayLayer::PublishTeleporterPrompt()
{
    TeleporterPromptMessage state;
    if (m_registry.IsValid(m_player) && m_grid)
    {
        const Vec2 player_tile = m_registry.GetComponent<Position>(m_player).tile;
        state.destination = FindTeleporterAt(m_registry, *m_grid, player_tile);
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
    PublishTargetState();
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

    // Camera zoom is a view control, not a turn action -- it's handled
    // unconditionally (any GameState on top) rather than gated to
    // ExploringState like the hotbar/character-screen keys below.
    EventDispatcher zoom_dispatcher(event);
    zoom_dispatcher.Dispatch<KeyPressedEvent>(
        [this](KeyPressedEvent& key_event)
        {
            if (key_event.GetKeyCode() == SDLK_KP_PLUS)
            {
                m_camera.SetZoom(m_camera.GetZoom() + kCameraZoomStep);
                return true;
            }
            if (key_event.GetKeyCode() == SDLK_KP_MINUS)
            {
                m_camera.SetZoom(m_camera.GetZoom() - kCameraZoomStep);
                return true;
            }
            return false;
        });
    if (event.handled)
        return;

    // Hotbar key-press trigger, the Character/Techniques-screen toggles, and
    // the hub interaction/mission-abandon keys only intercept keys while the
    // player is free to act (Exploring on top, not already mid-target-select
    // or already viewing a modal screen -- closing one is that screen's own
    // HandleEvent's job, reached via m_state_machine.HandleEvent below once
    // it's on top).
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

                if (key_event.GetKeyCode() == SDLK_P)
                {
                    GameplayContext context{m_registry, *m_grid, *m_turn_coordinator, m_player, GetMessageBus()};
                    m_state_machine.Push(m_action_palette_state, context);
                    return true;
                }

                if (key_event.GetKeyCode() == SDLK_TAB)
                {
                    m_tab_target_system->CycleTarget(Entity(m_registry, m_player));
                    return true;
                }

                // Only claimed when there's a target to clear -- nothing else
                // binds Escape while ExploringState is on top, but leaving it
                // unhandled otherwise keeps room for that to change later.
                if (key_event.GetKeyCode() == SDLK_ESCAPE &&
                    m_registry.GetComponent<TabTargetComponent>(m_player).target != entt::null)
                {
                    m_tab_target_system->ClearTarget(Entity(m_registry, m_player));
                    return true;
                }

                // Walking onto a hub entity carrying InteractableComponent
                // and pressing Space opens the matching screen -- Space
                // otherwise falls through to ActionMap's existing Wait
                // binding (see KeyBindings.cpp), untouched here.
                if (m_scene == SceneKind::Hub && key_event.GetKeyCode() == SDLK_SPACE && m_registry.IsValid(m_player))
                {
                    const Vec2 player_tile = m_registry.GetComponent<Position>(m_player).tile;
                    if (const std::optional<InteractionType> interaction =
                            FindInteractableAt(m_registry, *m_grid, player_tile))
                    {
                        GameplayContext context{m_registry, *m_grid, *m_turn_coordinator, m_player, GetMessageBus()};
                        switch (*interaction)
                        {
                        case InteractionType::Shop:
                            m_state_machine.Push(m_shop_state, context);
                            return true;
                        case InteractionType::Storage:
                            m_state_machine.Push(m_storage_state, context);
                            return true;
                        case InteractionType::MissionSelect:
                            m_state_machine.Push(m_mission_select_state, context);
                            return true;
                        }
                    }
                }

                // Walking onto a dungeon's Entrance/Exit teleporter entity and
                // pressing Space activates it directly (see
                // OnTeleporterActivated) -- unlike the hub's InteractableComponent
                // screens above, this never opens a modal GameState.
                if (m_scene == SceneKind::Dungeon && key_event.GetKeyCode() == SDLK_SPACE &&
                    m_registry.IsValid(m_player))
                {
                    const Vec2 player_tile = m_registry.GetComponent<Position>(m_player).tile;
                    if (const std::optional<TeleporterDestination> destination =
                            FindTeleporterAt(m_registry, *m_grid, player_tile))
                    {
                        OnTeleporterActivated(*destination);
                        return true;
                    }
                }

                // Abandons the current mission without completion credit,
                // returning to the hub -- no in-mission entity to walk onto
                // for this one, so it stays a plain keybind, active only
                // mid-dungeon.
                if (m_scene == SceneKind::Dungeon && key_event.GetKeyCode() == SDLK_H)
                {
                    TransitionToWorld(SceneKind::Hub, std::nullopt);
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
