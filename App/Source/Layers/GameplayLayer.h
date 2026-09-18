#pragma once

#include "Actions/ProjectileAdvanceAction.h"
#include "Areas/AreaLibrary.h"
#include "Combat/PhotonArtLibrary.h"
#include "Combat/StatusEffectLibrary.h"
#include "Combat/TechniqueLibrary.h"
#include "Components/TeleporterComponent.h"
#include "Engine/Dungeon/DungeonLibrary.h"
#include "Engine/Dungeon/PieceLibrary.h"
#include "Engine/Dungeon/RoomMap.h"
#include "Engine/Dungeon/RoomClearDoorSystem.h"
#include "Engine/Dungeon/RoomVisibilityTracker.h"
#include "Engine/Dungeon/SpawnWaveSystem.h"
#include "Engine/Dungeon/SwitchTriggerSystem.h"
#include "Engine/ECS/LifetimeSystem.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Layer.h"
#include "Engine/Render/AnimationClock.h"
#include "Engine/Render/Camera.h"
#include "Engine/Render/FloatingTextSystem.h"
#include "Engine/Render/TextureAtlas.h"
#include "Engine/Render/TileGpuPipeline.h"
#include "Engine/Render/TileRenderer.h"
#include "Engine/Render/VisualEffectSystem.h"
#include "Engine/World/Grid.h"
#include "Hub/HubDefinition.h"
#include "Items/AffixLibrary.h"
#include "Items/SectionId.h"
#include "Missions/RunProgress.h"
#include "Progression/CharacterClass.h"
#include "Progression/ClassDefinition.h"
#include "Render/FogOfWarRenderableLookup.h"
#include "Render/RegistryRenderableLookup.h"
#include "Shop/ShopStock.h"
#include "States/AnimationState.h"
#include "States/CharacterScreenState.h"
#include "States/ExploringState.h"
#include "States/GameOverState.h"
#include "States/GameStateMachine.h"
#include "States/MissionSelectState.h"
#include "States/ShopState.h"
#include "States/StorageState.h"
#include "States/TargetSelectionState.h"
#include "States/ActionPaletteState.h"
#include "Systems/CombatLogBridge.h"
#include "Systems/DamageEffectSystem.h"
#include "Systems/DamageTextSystem.h"
#include "Systems/EnemyAiSystem.h"
#include "Systems/ExperienceSystem.h"
#include "Systems/HealEffectSystem.h"
#include "Systems/HealTextSystem.h"
#include "Systems/LootDropSystem.h"
#include "Systems/MissFlashEffectSystem.h"
#include "Systems/OnHitEffectSystem.h"
#include "Systems/StatusEffectWorldMarkers.h"
#include "Systems/TabTargetSystem.h"
#include "Systems/TurnCoordinator.h"

#include <entt/entt.hpp>

#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace psr {

class IAction;
struct HotbarSlotActivatedMessage;
struct HudReadyMessage;
struct RestartRequestedMessage;
struct InventoryItemActivatedMessage;
struct InventoryItemHoverChangedMessage;
struct EquipmentSlotActivatedMessage;
struct EquipmentSlotHoverChangedMessage;
struct MagFeedRequestedMessage;
struct HotbarSlotAssignedMessage;
struct ActionPaletteSlotAssignedMessage;
struct MissionSelectedMessage;
struct ShopBuyRequestedMessage;
struct ShopSellRequestedMessage;
struct StorageItemActivatedMessage;
struct StorageWithdrawRequestedMessage;
struct WorldMouseDownMessage;
struct WorldMouseMoveMessage;
struct WorldMouseScrollMessage;

// The live gameplay scene: generates a dungeon into a Grid, spawns the
// player into it, and drives the turn loop -- TurnCoordinator's buffered
// input wired to the SDL event loop, TileRenderer/Camera drawing the result.
// This is the permanent home for gameplay systems as they land (combat,
// AI, mission flow, HUD), not a throwaway harness -- start small (player
// movement in a fixed test dungeon) and grow it in place.
//
// Now hosts two scenes sharing one Grid/Registry/render pipeline/state
// machine (see SceneKind): a non-procedural Hub (persistent, built from a
// single hand-authored DungeonPiece, never DungeonStitcher-generated) and
// Dungeon missions entered from it via Mission Select. TransitionToWorld is
// the seam between them -- it preserves the player and everything reachable
// from its Inventory/Equipment/Storage (see DestroyWorldEntities) and wipes
// everything else, so buying gear at the hub shop or looting a mission
// survives the swap back. SpawnNewCharacter is the one-time setup that
// creates the player and its permanent components, then hands off to
// TransitionToWorld(Hub) for everything scene-shaped.
//
// Input/turn flow is hosted on a GameStateMachine (ExploringState/
// TargetSelectionState -- see States/), ported from UnnamedRoguelike so
// Photon Art/Technique targeting can suspend normal play the same way that
// sibling suspends its own ExploringState for a modal cursor. Photon Art/
// Technique/Item activation is driven by the player's HotbarComponent
// (number keys 1-9/0, or a HudLayer click forwarded as
// HotbarSlotActivatedMessage) -- see TryActivateSlot. Item activation skips
// the target-select detour (self-only) and submits its UseItemAction via
// TurnCoordinator::SetPendingAction directly. Standing on a hub entity
// carrying InteractableComponent (shopkeeper/storage terminal/teleporter) and
// pressing Space opens the matching screen -- see Hub/HubInteraction.h's
// FindInteractableAt and OnEvent's Space handling. A dungeon's Entrance/Exit
// pieces carry their own TeleporterComponent-stamped entities instead
// (M4.6): standing on one and pressing Space calls OnTeleporterActivated
// directly rather than opening a modal screen -- see
// Missions/TeleporterInteraction.h's FindTeleporterAt.
class GameplayLayer : public Layer
{
public:
    // chosen_class/chosen_section_id are the player's character-creation
    // picks (see CharacterCreationLayer), fixed for the whole run -- read by
    // SpawnNewCharacter to load the matching ClassDefinition and set
    // SectionIdComponent.
    GameplayLayer(ClassId chosen_class, SectionId chosen_section_id);
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
    // Which scene TransitionToWorld last built -- gates the hub-only
    // interaction prompt/Space handling and the dungeon-only mission-exit
    // check/'H' abandon key.
    enum class SceneKind
    {
        Hub,
        Dungeon
    };

    // Builds the atlas/GPU pipeline/tile renderer on first call -- they need
    // a live SDL_Renderer&, which Layer only ever hands to OnRender, never
    // OnAttach (see Application; there is no other renderer accessor).
    void EnsureRenderResources(SDL_Renderer& renderer);

    // One-time setup, called once from OnAttach(): a fresh Registry/schema/
    // content libraries, the player entity and its permanent components
    // (Health/TP/TabTarget/Level/Class/SectionId/Currency/Inventory/Storage --
    // Class/SectionId/starting HP-TP/starting weapon/starting known
    // Techniques all come from m_chosen_class's ClassDefinition, see
    // CharacterCreationLayer). Hands off to
    // TransitionToWorld(Hub) for everything scene-shaped (Grid/dungeon/
    // per-world systems), then -- now that the player is placed and
    // TurnCoordinator exists -- does the innate-weapon auto-equip and
    // default-hotbar-loadout blocks, finishing with ActorComponent to
    // enqueue the player into the turn queue.
    void SpawnNewCharacter();

    // The player's own DeathEvent handler, subscribed in SpawnNewCharacter in
    // place of DeathSystem's/InnateWeaponComponent's (both unsubscribed there
    // for this entity only) -- unlike every other HealthComponent-bearing
    // entity, the player must survive its own death with gear/Meseta/level
    // intact (see the class doc comment and OnRestartRequested), so this
    // mirrors DeathSystem's grid removal but removes PlayerControlledComponent
    // instead of destroying the entity: enough for TurnCoordinator's
    // live-player count (see TurnCoordinator::OnPlayerControlledDestroyed) to
    // report PlayerDefeated without losing anything else on the entity.
    void OnPlayerDeath(entt::entity self);

    // (Re)builds everything scene-shaped: destroys every world entity except
    // the player and everything reachable from its Inventory/Equipment/
    // Storage (see DestroyWorldEntities), then either instantiates the hub's
    // single authored piece directly (target == Hub, skipping
    // DungeonStitcher entirely) or generates+instantiates a Dungeon looked
    // up by dungeon_id_string (target == Dungeon). Two different rebuild
    // shapes among the per-world std::optional systems, both driven by the
    // same underlying fact -- m_registry/m_grid are no longer reset/replaced
    // by a scene swap (only DestroyWorldEntities' selective entity destroy
    // runs), so a Registry&/Grid&-holding system's references stay valid
    // forever, same reasoning m_turn_coordinator's KeyBindings() Grid&
    // binding already relies on:
    //  - m_room_map/m_room_visibility/m_spawn_wave_system/
    //    m_room_clear_door_system/m_enemy_ai_system/m_projectile_advance_action/
    //    m_tab_target_system hold genuinely per-dungeon *data* (this
    //    dungeon's room layout, pending spawn waves, which doors gate which
    //    room, ...) that a new dungeon invalidates outright -- these are
    //    rebuilt via .emplace() every call, exactly as the old LoadNewGame()
    //    did.
    //  - m_turn_coordinator/m_lifetime_system/m_combat_log_bridge/m_loot_drop_system/
    //    m_experience_system/m_visual_effects/m_miss_flash_effect_system/
    //    m_on_hit_effect_system/m_heal_effect_system/m_status_effect_markers/
    //    m_switch_trigger_system
    //    hold no such per-dungeon data -- just references plus, for several of them, a
    //    Subscribe(player) call. Since the player entity itself survives a
    //    scene swap (kept by DestroyWorldEntities) but
    //    EventHandlerComponent has no Unsubscribe-by-instance (only
    //    by-owner-type, see EventHandlerComponent.h), re-emplacing one of
    //    these and re-Subscribing the player would leave the OLD instance's
    //    handler dangling (bound to a destroyed `this`) rather than
    //    replacing it. So each is instead built lazily exactly once, on
    //    whichever TransitionToWorld call reaches it first (guarded by
    //    `if (!m_x)`, same idiom EnsureRenderResources already uses for GPU
    //    resources), and never rebuilt again -- only the player's own
    //    Subscribe call is once-only this way; every enemy still gets a
    //    fresh Subscribe each time it's spawned (on_enemy_spawned), since
    //    enemies are always brand-new entities, never the persisting
    //    player. See each member's own doc comment for specifics.
    // Relocates the player to the new entrance tile and fully restores
    // HP/TP. Called from SpawnNewCharacter (Hub), Mission Select (Dungeon),
    // mission-exit/abandon/death (Hub).
    void TransitionToWorld(SceneKind target, std::optional<std::string> dungeon_id_string);

    // Destroys every live entity except m_player and everything reachable
    // from its InventoryComponent/EquipmentComponent/StorageComponent --
    // every entity always carries EventHandlerComponent (see
    // Registry::CreateEntity), so that's what this walks via Each to find
    // "everything." Collects into a keep-set first, matching Each<T>'s own
    // "don't destroy from inside func" contract.
    void DestroyWorldEntities();

    // Republishes hotbar/player-status/Meseta/target-panel state after a
    // scene swap discards the previous scene's entities -- extracted from
    // the old OnRestartRequested's tail, now also used by every
    // TransitionToWorld call, not just a death restart.
    void RepublishHudStateAfterTransition();

    // Called with the player's current tile and the placed-piece index it
    // resolves to (via m_room_map->GetRoom), both once right after a
    // TransitionToWorld and every OnUpdate tick thereafter. Triggers
    // m_spawn_wave_system's room-entry-gated first wave exactly on a real
    // room change (never on every frame the player stays put, and safely
    // idempotent on re-entry -- see SpawnWaveSystem::TriggerRoomEntered's
    // own doc comment), then forwards to m_room_visibility->Update for fog
    // of war, same as before this existed. Holds off on all of that --
    // including the visibility update -- while player_tile is still one of
    // the new room's own entry doors (see
    // RoomClearDoorSystem::IsEntryThresholdTile), so the door doesn't seal
    // shut under the player's feet; the neighboring room is still rendered
    // Visible via room_adjacency in the meantime, so nothing looks hidden.
    void EnterRoom(Vec2 player_tile, std::optional<std::uint32_t> room);

    // Restores TP scaled by the player's effective MST (equipment/affixes
    // included, via ComputeEffectiveStats) on every real room change -- see
    // EnterRoom's caller of this. Not a PSO mechanic; a new per-room
    // "meditation" pickup-me-up so Force-leaning builds aren't fully TP-gated
    // between item pickups. Clamped to max_tp, no-ops if the player has no
    // TPComponent.
    void RegainTpOnRoomEntry();

    // Dispatched from OnEvent's dungeon-scene Space handling when
    // FindTeleporterAt resolves a hit on the player's tile (see
    // Missions/TeleporterInteraction.h) -- ReturnToHub bails out of the
    // mission with no completion credit (same as the 'H' abandon key);
    // AdvanceLevel records the current dungeon as completed in
    // m_run_progress, publishes MissionCompletedMessage, and transitions to
    // whatever Missions::NextDungeonInArea resolves (the next dungeon in this
    // dungeon's Area sequence, or the hub if there isn't one).
    void OnTeleporterActivated(TeleporterDestination destination);

    // Responds to RestartRequestedMessage (published by GameOverState on the
    // first key press while it's on top of the state stack) by restoring
    // PlayerControlledComponent (removed by OnPlayerDeath, never destroyed
    // along with the rest of the entity) before transitioning back to the hub
    // (full-heal, gear/Meseta/level intact -- see the class doc comment) and
    // popping GameOverState back off the stack.
    void OnRestartRequested(const RestartRequestedMessage& message);

    // If slot_index names a Technique/PhotonArt hotbar slot (see
    // HotbarComponent) the player can currently afford, constructs the
    // corresponding Action and requests targeting for it. Returns whether the
    // slot was consumed (Item/Empty slots, or an unaffordable ability,
    // return false).
    bool TryActivateSlot(int slot_index);

    // Routes a built TargetRequest either through the interactive
    // target-select detour (TurnCoordinator::RequestTargeting) or, for
    // TargetingMode::SelfTarget, straight to SetPendingAction with
    // SelectedTargetComponent pre-set to the caster's own tile -- the
    // reachable set for SelfTarget is always {origin} (see
    // TargetSelectionState::IsReachable), so making the player confirm a
    // cursor that can't move is a no-op detour, not a real choice.
    void BeginTargeting(TargetRequest request);

    void OnHotbarSlotActivated(const HotbarSlotActivatedMessage& message);

    // Responds to HudLayer's HudReadyMessage by re-publishing current hotbar/
    // player-status state -- see that message's own doc comment for why a
    // one-time publish from this layer's own OnAttach can't reach HudLayer
    // directly.
    void OnHudReady(const HudReadyMessage& message);

    // Resolves the player's current HotbarComponent into display names
    // (via m_techniques/m_photon_arts) and publishes a HotbarStateMessage.
    void PublishHotbarState();

    // Published by HudLayer when the player clicks a row on the Character
    // screen (only meaningful while m_character_screen_state is on top --
    // same stray-click guard OnHotbarSlotActivated already has). Calls
    // EquipItem/UnequipSlot (Items/Equip.h) directly -- free/instant, no
    // IAction, see CharacterScreenState's own doc comment for why -- and
    // republishes the screen's contents on success so the open list reflects
    // the change immediately.
    void OnInventoryItemActivated(const InventoryItemActivatedMessage& message);

    // Published by HudLayer whenever the Character screen's hovered/focused
    // inventory row changes -- see EquipPreview.h's ComputeEquipStatDelta and
    // InventoryItemHoverChangedMessage's own doc comment. Read-only (never
    // mutates player state), so unlike OnInventoryItemActivated this always
    // responds, even for a non-equippable row (with active=false).
    void OnInventoryItemHoverChanged(const InventoryItemHoverChangedMessage& message);
    void OnEquipmentSlotActivated(const EquipmentSlotActivatedMessage& message);

    // Equipment-panel sibling of OnInventoryItemHoverChanged -- see
    // ComputeUnequipStatDelta and EquipmentSlotHoverChangedMessage's own doc
    // comment. Same "always responds, even for an empty slot" contract.
    void OnEquipmentSlotHoverChanged(const EquipmentSlotHoverChangedMessage& message);

    // Published by HudLayer once the player selects a food item while
    // feeding the equipped mag (see HudLayer's "Feed" context-menu action
    // and its mag-food-selection sub-state). Free/instant, same reasoning as
    // OnEquipmentSlotActivated -- calls ApplyMagFood, consumes one unit of
    // the food item, and republishes the screen's contents.
    void OnMagFeedRequested(const MagFeedRequestedMessage& message);

    // Handles HudLayer's "Assign to Hotbar" flow -- free/instant, same
    // reasoning as OnEquipmentSlotActivated, but rewrites HotbarComponent
    // and republishes HotbarStateMessage instead of the Character screen.
    void OnHotbarSlotAssigned(const HotbarSlotAssignedMessage& message);

    // fed_mag threads straight into CharacterScreenMessage::fed_mag -- see
    // that field's doc comment for why OnMagFeedRequested passes true rather
    // than HudLayer inferring it from Publish's timing.
    void PublishCharacterScreenState(bool fed_mag = false);

    // Same "Assign to Hotbar" flow as OnHotbarSlotAssigned, but for the
    // Techniques/Photon Arts screen's rows (see AssignAbilityToHotbarSlot) --
    // gated on m_action_palette_state being on top instead of
    // m_character_screen_state.
    void OnActionPaletteSlotAssigned(const ActionPaletteSlotAssignedMessage& message);

    // Published by HudLayer when the player picks an unlocked Mission Select
    // row; re-validates IsDungeonUnlocked (defense in depth -- HudLayer
    // shouldn't let a locked row publish this at all) before transitioning.
    void OnMissionSelected(const MissionSelectedMessage& message);

    // Published by HudLayer when the player picks a Shop screen row; calls
    // Items/Shop.h's BuyItem/SellItem and republishes the screen's contents
    // (and Meseta) on success.
    void OnShopBuyRequested(const ShopBuyRequestedMessage& message);
    void OnShopSellRequested(const ShopSellRequestedMessage& message);

    // Published by HudLayer when the player picks a Storage screen row;
    // calls Items/Storage.h's StoreItem/WithdrawItem and republishes the
    // screen's contents on success.
    void OnStorageItemActivated(const StorageItemActivatedMessage& message);
    void OnStorageWithdrawRequested(const StorageWithdrawRequestedMessage& message);

    // Converts every currently-active m_floating_text instance to a screen
    // pixel (via TileToPixel, using m_camera and the window size cached from
    // the last OnRender call) and publishes a FloatingTextStateMessage for
    // HudLayer to render. Called from OnUpdate, right after
    // m_floating_text.Update() -- see that member's own doc comment for why
    // this can't just live inside TurnCoordinator::Step/ExploringState.
    void PublishFloatingTextState();

    // Resolves the player's TabTargetComponent (name/race label/HP) into a
    // TargetStateMessage for HudLayer's bottom-left target panel. Called
    // from OnUpdate, right after m_tab_target_system->Update(), and once
    // from OnHudReady for the same post-attach handshake reason
    // PublishHotbarState/PublishPlayerStatus already need it for.
    void PublishTargetState();

    // Resolves FindInteractableAt(m_registry, *m_grid, player_tile) into a
    // HubInteractionPromptMessage for HudLayer's "Press SPACE to ..." hint.
    // Called from OnUpdate, hub scene only.
    void PublishHubInteractionPrompt();

    // Dungeon-scene sibling of PublishHubInteractionPrompt -- resolves
    // FindTeleporterAt(m_registry, *m_grid, player_tile) into a
    // TeleporterPromptMessage. Called from OnUpdate, dungeon scene only.
    void PublishTeleporterPrompt();

    // Resolves a HudLayer-reported screen point to a world tile via
    // PixelToTile (TileVertexMath.h), using m_camera and the window size
    // cached from the last OnRender call -- the mouse's counterpart to
    // PublishFloatingTextState's own TileToPixel use, just inverted.
    Vec2 ResolveWorldMouseTile(float screen_x, float screen_y) const;

    // Routes a left-click (button 0) to whichever of click-to-move/click-to-
    // target applies, based on which GameState is on top: TargetSelection
    // (already pushed by an in-progress Photon Art/Technique cast) calls
    // m_target_selection_state.ConfirmTile directly, since mouse input never
    // becomes a semantic Event and so can't reach GameState::HandleEvent's
    // normal dispatch; Exploring recomputes m_pending_move_path via FindPath
    // and starts consuming it (see OnUpdate). Any other GameState (a modal
    // screen open) ignores the click -- HudLayer's own click listeners
    // handle screen UI separately from this world-mouse path entirely.
    void OnWorldMouseDown(const WorldMouseDownMessage& message);

    // Resolves the hovered tile's first occupant (if any) into a
    // WorldTileHoverMessage for HudLayer's tooltip -- see
    // WorldTileHoverMessage.h. Re-publishes only when the hovered tile
    // actually changes (m_last_hovered_tile), not on every pixel of mouse
    // jitter.
    void OnWorldMouseMove(const WorldMouseMoveMessage& message);

    // Maps wheel_delta_y onto the same Camera::SetZoom/kCameraZoomStep the
    // KP_PLUS/KP_MINUS binding already drives -- a second input for the same
    // existing zoom mechanic, not a new camera capability (Camera tracks the
    // player, not free-look, so there's no cursor-pivot to speak of here).
    void OnWorldMouseScroll(const WorldMouseScrollMessage& message);

    // The player's character-creation picks (see CharacterCreationLayer),
    // fixed for the whole run -- set once via the constructor, read by
    // SpawnNewCharacter.
    ClassId m_chosen_class;
    SectionId m_chosen_section_id;

    Registry m_registry;
    AreaLibrary m_areas;
    PieceLibrary m_pieces;
    DungeonLibrary m_dungeons;
    HubDefinition m_hub;
    ShopStock m_shop_stock;
    RunProgress m_run_progress;
    AffixLibrary m_affixes; // empty: no affix content authored yet (pending M8.2's drop-table work)
    PhotonArtLibrary m_photon_arts;
    TechniqueLibrary m_techniques;
    StatusEffectLibrary m_status_effects;
    // m_chosen_class's starting kit and level-up table -- see
    // ClassDefinition.h. Loaded via LoadClassDefinition (one hand-authored
    // App/Assets/Data/Classes/<class-id>.json document, not a per-item
    // content library like the ones above), replacing the old single
    // class-agnostic growth_curve.json now that ClassId exists.
    ClassDefinition m_class_definition;
    std::mt19937 m_rng{std::random_device{}()};

    SceneKind m_scene = SceneKind::Hub;
    std::string m_current_dungeon_id; // only meaningful while m_scene == Dungeon

    std::optional<Grid> m_grid;
    entt::entity m_player = entt::null;
    Camera m_camera;

    // Window size fetched by OnRender's own SDL_GetCurrentRenderOutputSize
    // call, cached here so OnUpdate's PublishFloatingTextState (which has no
    // renderer/window handle of its own) can still convert world tiles to
    // screen pixels. Zero until the first OnRender call -- harmless, since
    // nothing can have spawned floating text before then.
    int m_last_render_width = 0;
    int m_last_render_height = 0;

    // Click-to-move: remaining tiles to walk, next step at index 0; empty
    // means no click-to-move is in flight. Recomputed wholesale by
    // OnWorldMouseDown, consumed one step per turn from OnUpdate, and
    // cleared on a new click, a keyboard move key, the next step becoming
    // blocked, or arrival (see OnUpdate/OnEvent for each clear site).
    std::vector<Vec2> m_pending_move_path;

    // Hover-to-inspect: the tile OnWorldMouseMove last resolved a
    // WorldTileHoverMessage for, so it only re-publishes when the hovered
    // tile actually changes rather than on every pixel of mouse jitter.
    // nullopt before the first mouse-move.
    std::optional<Vec2> m_last_hovered_tile;

    // Generic short-lived colored-text-drifting-from-a-world-position system
    // (see FloatingTextSystem.h) -- damage numbers (m_damage_text_system
    // below) are its first consumer, not its only one. Advanced from
    // OnUpdate directly, not from inside TurnCoordinator::Step/
    // ExploringState::Update, so it keeps animating even while a modal
    // GameState (target selection, the Character screen) is on top and the
    // turn loop itself is paused.
    FloatingTextSystem m_floating_text;

    // Bridges AfterDamageEvent onto m_floating_text -- see DamageTextSystem.h.
    // Holds only a pointer into m_floating_text (declared just above), so
    // it's a plain long-lived member. Subscribe(player) is called exactly
    // once, from SpawnNewCharacter -- unlike the per-world systems below, the
    // player's own subscription must never be reissued on a later scene
    // swap (the player entity persists across TransitionToWorld, and
    // EventHandlerComponent has no Unsubscribe-by-instance, only
    // by-owner-type, so a second Subscribe call would either duplicate the
    // handler or leave a dangling one bound to a destroyed instance -- see
    // TransitionToWorld's own doc comment). Every enemy still gets a fresh
    // Subscribe call each time it's spawned (on_enemy_spawned), since enemies
    // are always brand-new entities, never the persisting player.
    DamageTextSystem m_damage_text_system{m_floating_text};

    // Bridges AfterHealEvent onto m_floating_text -- see HealTextSystem.h.
    // Same "plain long-lived member, player Subscribe() issued exactly once
    // from SpawnNewCharacter" shape as m_damage_text_system just above, for
    // the identical reason.
    HealTextSystem m_heal_text_system{m_floating_text};

    // Room-granularity fog of war: which placed piece each tile belongs to,
    // and which pieces are current/visited -- see RoomMap/RoomVisibilityTracker.
    // Hold no pointers into other members, so declaration order relative to
    // them doesn't matter. Re-populated via .emplace() (not reassigned) in
    // TransitionToWorld, same as m_grid, so FogOfWarRenderableLookup's
    // references into them stay valid across a scene swap.
    std::optional<RoomMap> m_room_map;
    std::optional<RoomVisibilityTracker> m_room_visibility;

    // Non-movable (binds registry component-lifecycle listeners to its own
    // address) -- must be constructed in place, after m_registry exists but
    // before the player's ActorComponent is spawned (TurnQueue membership
    // is driven by that construction order, see TurnCoordinator.cpp).
    // Declared after m_registry so it's destroyed first. Built lazily on the
    // very first TransitionToWorld call and never rebuilt after -- a scene
    // swap never touches the player's ActorComponent/TurnQueue membership,
    // so there's no reason to reconstruct this alongside the world it
    // schedules turns for.
    std::optional<TurnCoordinator> m_turn_coordinator;

    // Generic turn-counted "expire after N turns" system -- see
    // LifetimeSystem.h. Holds only a Registry* (no per-dungeon state), so
    // it's lazy-once/never-rebuilt like m_visual_effects; wired to
    // m_turn_coordinator's turn-clock sentinel via SetOnTurnPassed right
    // after m_turn_coordinator itself is constructed.
    std::optional<LifetimeSystem> m_lifetime_system;

    // Decides non-player actors' turns (installed onto m_turn_coordinator via
    // SetNpcDecision in OnAttach). Holds only pointers into m_grid/m_registry/
    // m_affixes/m_rng, so declaration order relative to them doesn't matter.
    std::optional<EnemyAiSystem> m_enemy_ai_system;

    // Advances an in-flight technique projectile by one hop per turn --
    // installed ahead of m_enemy_ai_system in the SetNpcDecision wrapper (see
    // ProjectileAdvanceAction.h). Stateless aside from its Grid/AffixLibrary/
    // rng pointers, so one instance serves every projectile actor; std::optional
    // only because it needs *m_grid, re-created each TransitionToWorld same as
    // m_enemy_ai_system.
    std::optional<ProjectileAdvanceAction> m_projectile_advance_action;

    // Drives the player's tab-lock target and its world marker -- see
    // TabTargetSystem.h. Holds only pointers into m_registry/m_grid, so
    // declaration order relative to them doesn't matter; std::optional only
    // because it needs *m_grid, re-created each TransitionToWorld same as
    // m_enemy_ai_system.
    std::optional<TabTargetSystem> m_tab_target_system;

    // Gates piece-authored spawn waves past their first: holds pointers into
    // m_registry/m_grid only (declaration order relative to them doesn't
    // matter for construction safety), but binds a component-lifecycle
    // listener to its own address like TurnCoordinator above, so it's still
    // non-movable and must be constructed in place.
    std::optional<SpawnWaveSystem> m_spawn_wave_system;

    // Unlocks RoomCleared-condition doors once their room's spawned enemies
    // all die -- see RoomClearDoorSystem.h. Same "re-created every
    // TransitionToWorld, non-movable component-lifecycle listener" reasoning
    // as m_spawn_wave_system just above (a fresh dungeon means fresh
    // room/door state).
    std::optional<RoomClearDoorSystem> m_room_clear_door_system;

    // Bridges the player's per-entity combat events onto the Layer
    // MessageBus for HudLayer to consume -- see CombatLogBridge.h. Holds
    // only pointers into m_registry/m_techniques/m_photon_arts (all
    // long-lived, never reset after SpawnNewCharacter), so unlike
    // m_room_map/m_enemy_ai_system/m_spawn_wave_system below, this holds no
    // per-dungeon state that a scene swap would invalidate -- built lazily
    // on the very first TransitionToWorld call and never rebuilt after,
    // same "build once, guarded by optional-empty check" idiom as
    // m_turn_coordinator, and for the same reason: Subscribe(player) must
    // never be reissued (see m_damage_text_system's doc comment above).
    // Every enemy still gets its own fresh Subscribe call each time it's
    // spawned, same as m_damage_text_system.
    std::optional<CombatLogBridge> m_combat_log_bridge;

    // Rolls loot when the player lands a killing blow -- see LootDropSystem.h.
    // Holds only pointers into m_registry/m_grid/m_rng -- Grid& stays valid
    // across a scene swap (m_grid's std::optional storage address is
    // stable, only its contents are replaced, same reasoning
    // m_turn_coordinator's KeyBindings() Grid& binding already relies on),
    // so this is lazy-once/never-rebuilt too, same reasoning as
    // m_combat_log_bridge (LootDropSystem::Subscribe is player-only, see
    // LootDropSystem.h).
    std::optional<LootDropSystem> m_loot_drop_system;

    // Awards XP and applies level-ups when the player lands a killing blow --
    // see ExperienceSystem.h. Holds only pointers into the Layer's own
    // MessageBus and m_class_definition.growth (both long-lived) -- lazy-once/
    // never-rebuilt, same reasoning as m_combat_log_bridge.
    std::optional<ExperienceSystem> m_experience_system;

    // Draws the player's active status effects as tinted markers in the
    // world -- see StatusEffectWorldMarkers.h. Holds only pointers into
    // m_registry/m_grid/m_status_effects (all long-lived/stable-address) --
    // lazy-once/never-rebuilt, same reasoning as m_combat_log_bridge.
    // ClearMarkers() (called from OnStatusEffectsChanged and the destructor)
    // guards each cached marker with Registry::IsValid before touching it,
    // since a scene swap's DestroyWorldEntities may have already destroyed
    // it out from under this system -- the same hazard VisualEffectSystem's
    // own Update() guards against, see its doc comment below.
    std::optional<StatusEffectWorldMarkers> m_status_effect_markers;

    // Activates Switch-condition doors' switches on walk-over -- see
    // SwitchTriggerSystem.h. Holds only pointers into m_registry/m_grid
    // (both stable-address across a scene swap) -- lazy-once/never-rebuilt,
    // same reasoning as m_status_effect_markers/m_loot_drop_system;
    // Subscribe(player) is player-only, issued once.
    std::optional<SwitchTriggerSystem> m_switch_trigger_system;

    // Generic short-lived, prefab-authored, fading world-effect entities (see
    // VisualEffectSystem.h) -- the player-miss flash (m_miss_flash_effect_system
    // below) is its first consumer, not its only one. Advanced from OnUpdate
    // directly, same unconditional-every-frame reasoning as m_floating_text.
    // Holds only pointers into m_registry/m_grid (both stable-address) --
    // lazy-once/never-rebuilt, same reasoning as m_combat_log_bridge. Its
    // own Update() guards each tracked instance with Registry::IsValid
    // before removing/destroying it, since a scene swap's
    // DestroyWorldEntities may already have destroyed the underlying entity
    // (an in-flight effect straddling a Hub<->Dungeon transition) -- without
    // that guard, a second DestroyEntity on an already-destroyed handle
    // would hit entt's own validity assert.
    std::optional<VisualEffectSystem> m_visual_effects;

    // Bridges AttackMissEvent onto m_visual_effects for a player-only flash --
    // see MissFlashEffectSystem.h. Holds only a pointer into m_visual_effects
    // (declared just above) plus the player's entt::entity -- lazy-once/
    // never-rebuilt, same reasoning as m_combat_log_bridge (Subscribe is
    // player-only here, unlike m_on_hit_effect_system below).
    std::optional<MissFlashEffectSystem> m_miss_flash_effect_system;

    // Bridges AfterDamageEvent onto m_visual_effects for a placeholder VFX at
    // whatever a hit's weapon/technique authored -- see OnHitEffectSystem.h.
    // Not player-filtered, unlike m_miss_flash_effect_system: every actor's
    // hit should show its effect, same breadth as m_damage_text_system's own
    // wiring -- lazy-once/never-rebuilt for the player's own Subscribe call,
    // same reasoning as m_combat_log_bridge; every enemy still gets its own
    // fresh Subscribe call each time it's spawned, same as
    // m_damage_text_system/m_combat_log_bridge.
    std::optional<OnHitEffectSystem> m_on_hit_effect_system;

    // Bridges AfterHealEvent onto m_visual_effects for a fixed heal glow --
    // see HealEffectSystem.h. Not player-filtered, same breadth as
    // m_on_hit_effect_system -- lazy-once/never-rebuilt for the player's own
    // Subscribe call, same reasoning as m_combat_log_bridge; every enemy
    // still gets its own fresh Subscribe call each time it's spawned, same
    // as m_on_hit_effect_system.
    std::optional<HealEffectSystem> m_heal_effect_system;

    // Kept alive across the interactive target-select flow -- RequestTargeting
    // only takes a non-owning IAction*, so whoever constructs the action
    // (this layer, for the reasons in the class doc comment) must own its
    // lifetime until it's either resolved (TurnCoordinator::SetPendingAction
    // then Step()'s normal resolution) or the player cancels. Also reused for
    // an activated Item slot's UseItemAction, which skips the target-select
    // detour entirely (self-only, see TryActivateSlot) but still needs the
    // same "stay alive until the next Step()" lifetime SetPendingAction's own
    // contract requires.
    std::unique_ptr<IAction> m_pending_slot_action;

    // GameStateMachine and its states this round -- declaration order
    // matters: m_target_selection_state/m_game_over_state/m_animation_state
    // must outlive m_exploring_state (which holds references to all three)
    // and all states must outlive m_state_machine's use of any of them.
    // m_character_screen_state/m_action_palette_state/
    // m_mission_select_state/m_shop_state/m_storage_state aren't referenced
    // by ExploringState's constructor (unlike the other three) -- each is
    // pushed directly from GameplayLayer::OnEvent's own key/interaction
    // handling instead of from inside ExploringState::Update -- but each
    // needs its own constructor arguments (m_affixes/m_techniques/
    // m_photon_arts/m_dungeons/m_run_progress/m_areas/m_shop_stock, all
    // declared well above this block) constructed first.
    TargetSelectionState m_target_selection_state;
    GameOverState m_game_over_state;
    AnimationState m_animation_state;
    CharacterScreenState m_character_screen_state{m_affixes, m_class_definition.growth, m_photon_arts,
                                                  m_status_effects};
    ActionPaletteState m_action_palette_state{m_techniques, m_photon_arts};
    MissionSelectState m_mission_select_state{m_dungeons, m_run_progress, m_areas};
    ShopState m_shop_state{m_shop_stock, m_affixes};
    StorageState m_storage_state{m_affixes};
    ExploringState m_exploring_state{m_target_selection_state, m_game_over_state, m_animation_state};
    GameStateMachine m_state_machine;

    std::optional<TextureAtlas> m_atlas;
    std::optional<TileGpuPipeline> m_gpu_pipeline;

    // Drives synced sprite-strip animation for every RenderableComponent
    // with frames > 1 (see AnimationClock.h). Advanced from OnUpdate,
    // same unconditional-every-frame reasoning as m_floating_text/
    // m_visual_effects. Holds no pointers into m_registry/m_grid, so
    // declaration order relative to them doesn't matter; must be
    // constructed before m_renderable_lookup, which holds a reference to it.
    AnimationClock m_animation_clock;
    std::optional<RegistryRenderableLookup> m_renderable_lookup;
    std::optional<FogOfWarRenderableLookup> m_fog_lookup;
    std::optional<TileRenderer> m_tile_renderer;
};

} // namespace psr
