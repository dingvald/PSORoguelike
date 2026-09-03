#pragma once

#include "Combat/PhotonArtLibrary.h"
#include "Combat/StatusEffectLibrary.h"
#include "Combat/TechniqueLibrary.h"
#include "Engine/Dungeon/PieceLibrary.h"
#include "Engine/Dungeon/RoomMap.h"
#include "Engine/Dungeon/RoomVisibilityTracker.h"
#include "Engine/Dungeon/SpawnWaveSystem.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Layer.h"
#include "Engine/Render/Camera.h"
#include "Engine/Render/FloatingTextSystem.h"
#include "Engine/Render/TextureAtlas.h"
#include "Engine/Render/TileGpuPipeline.h"
#include "Engine/Render/TileRenderer.h"
#include "Engine/Render/VisualEffectSystem.h"
#include "Engine/World/Grid.h"
#include "Items/AffixLibrary.h"
#include "Render/FogOfWarRenderableLookup.h"
#include "Render/RegistryRenderableLookup.h"
#include "States/AnimationState.h"
#include "States/CharacterScreenState.h"
#include "States/ExploringState.h"
#include "States/GameOverState.h"
#include "States/GameStateMachine.h"
#include "States/TargetSelectionState.h"
#include "States/TechniquesScreenState.h"
#include "Systems/CombatLogBridge.h"
#include "Systems/DamageTextSystem.h"
#include "Systems/EnemyAiSystem.h"
#include "Systems/LootDropSystem.h"
#include "Systems/MissFlashEffectSystem.h"
#include "Systems/StatusEffectWorldMarkers.h"
#include "Systems/TurnCoordinator.h"

#include <entt/entt.hpp>

#include <memory>
#include <optional>
#include <random>

namespace psr {

class IAction;
struct HotbarSlotActivatedMessage;
struct HudReadyMessage;
struct RestartRequestedMessage;
struct InventoryItemActivatedMessage;
struct EquipmentSlotActivatedMessage;
struct HotbarSlotAssignedMessage;
struct TechniquesScreenSlotAssignedMessage;

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
// Technique/Item activation is driven by the player's HotbarComponent
// (number keys 1-9/0, or a HudLayer click forwarded as
// HotbarSlotActivatedMessage) -- see TryActivateSlot. Item activation skips
// the target-select detour (self-only) and submits its UseItemAction via
// TurnCoordinator::SetPendingAction directly.
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

    // (Re)builds the whole run: a fresh Registry/Grid, a newly generated
    // dungeon, the player, and every system/bridge that depends on them
    // (m_turn_coordinator, m_spawn_wave_system, m_enemy_ai_system,
    // m_combat_log_bridge, m_status_effect_markers). Called once from
    // OnAttach() and again from OnRestartRequested() -- everything it
    // touches is a std::optional/plain member re-populated in place, so
    // calling it twice is safe. Does not touch m_state_machine, the
    // HotbarSlotActivatedMessage/HudReadyMessage/RestartRequestedMessage
    // subscriptions, or HudLayer's overlay -- those are one-time-only setup
    // OnAttach() still owns.
    void LoadNewGame();

    // Responds to RestartRequestedMessage (published by GameOverState on the
    // first key press while it's on top of the state stack) by calling
    // LoadNewGame() and popping GameOverState back off the stack.
    void OnRestartRequested(const RestartRequestedMessage& message);

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

    // Published by HudLayer when the player clicks a row on the Character
    // screen (only meaningful while m_character_screen_state is on top --
    // same stray-click guard OnHotbarSlotActivated already has). Calls
    // EquipItem/UnequipSlot (Items/Equip.h) directly -- free/instant, no
    // IAction, see CharacterScreenState's own doc comment for why -- and
    // republishes the screen's contents on success so the open list reflects
    // the change immediately.
    void OnInventoryItemActivated(const InventoryItemActivatedMessage& message);
    void OnEquipmentSlotActivated(const EquipmentSlotActivatedMessage& message);

    // Handles HudLayer's "Assign to Hotbar" flow -- free/instant, same
    // reasoning as OnEquipmentSlotActivated, but rewrites HotbarComponent
    // and republishes HotbarStateMessage instead of the Character screen.
    void OnHotbarSlotAssigned(const HotbarSlotAssignedMessage& message);
    void PublishCharacterScreenState();

    // Same "Assign to Hotbar" flow as OnHotbarSlotAssigned, but for the
    // Techniques/Photon Arts screen's rows (see AssignAbilityToHotbarSlot) --
    // gated on m_techniques_screen_state being on top instead of
    // m_character_screen_state.
    void OnTechniquesScreenSlotAssigned(const TechniquesScreenSlotAssignedMessage& message);

    // Converts every currently-active m_floating_text instance to a screen
    // pixel (via TileToPixel, using m_camera and the window size cached from
    // the last OnRender call) and publishes a FloatingTextStateMessage for
    // HudLayer to render. Called from OnUpdate, right after
    // m_floating_text.Update() -- see that member's own doc comment for why
    // this can't just live inside TurnCoordinator::Step/ExploringState.
    void PublishFloatingTextState();

    Registry m_registry;
    PieceLibrary m_pieces;
    AffixLibrary m_affixes; // empty: no affix content authored yet (pending M8.2's drop-table work)
    PhotonArtLibrary m_photon_arts;
    TechniqueLibrary m_techniques;
    StatusEffectLibrary m_status_effects;
    std::mt19937 m_rng{std::random_device{}()};

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
    // unlike CombatLogBridge/LootDropSystem it's a plain long-lived member,
    // not an std::optional rebuilt every LoadNewGame(); only its Subscribe()
    // calls need reissuing after a restart, same as CombatLogBridge's.
    DamageTextSystem m_damage_text_system{m_floating_text};

    // Room-granularity fog of war: which placed piece each tile belongs to,
    // and which pieces are current/visited -- see RoomMap/RoomVisibilityTracker.
    // Hold no pointers into other members, so declaration order relative to
    // them doesn't matter. Re-populated via .emplace() (not reassigned) in
    // LoadNewGame, same as m_grid, so FogOfWarRenderableLookup's references
    // into them stay valid across a restart.
    std::optional<RoomMap> m_room_map;
    std::optional<RoomVisibilityTracker> m_room_visibility;

    // Non-movable (binds registry component-lifecycle listeners to its own
    // address) -- must be constructed in place, after m_registry exists but
    // before the player's EnergyComponent is spawned (TurnQueue membership
    // is driven by that construction order, see TurnCoordinator.cpp).
    // Declared after m_registry so it's destroyed first.
    std::optional<TurnCoordinator> m_turn_coordinator;

    // Decides non-player actors' turns (installed onto m_turn_coordinator via
    // SetNpcDecision in OnAttach). Holds only pointers into m_grid/m_registry/
    // m_affixes/m_rng, so declaration order relative to them doesn't matter.
    std::optional<EnemyAiSystem> m_enemy_ai_system;

    // Gates piece-authored spawn waves past their first: holds pointers into
    // m_registry/m_grid only (declaration order relative to them doesn't
    // matter for construction safety), but binds a component-lifecycle
    // listener to its own address like TurnCoordinator above, so it's still
    // non-movable and must be constructed in place.
    std::optional<SpawnWaveSystem> m_spawn_wave_system;

    // Bridges the player's per-entity combat events onto the Layer
    // MessageBus for HudLayer to consume -- see CombatLogBridge.h. Holds
    // only pointers into m_registry/m_techniques/m_photon_arts, so its
    // declaration order relative to them doesn't matter for construction/
    // destruction safety.
    std::optional<CombatLogBridge> m_combat_log_bridge;

    // Rolls loot when the player lands a killing blow -- see LootDropSystem.h.
    // Holds only pointers into m_registry/m_grid/m_rng, so its
    // declaration order relative to them doesn't matter for construction/
    // destruction safety.
    std::optional<LootDropSystem> m_loot_drop_system;

    // Draws the player's active status effects as tinted markers in the
    // world -- see StatusEffectWorldMarkers.h. Holds only pointers into
    // m_registry/m_grid/m_status_effects (declaration order doesn't matter
    // for those), but its own destructor removes any outstanding marker
    // entities from *m_grid, so it must be destroyed before m_grid is --
    // declared after m_grid (members destroy in reverse declaration order).
    std::optional<StatusEffectWorldMarkers> m_status_effect_markers;

    // Generic short-lived, prefab-authored, fading world-effect entities (see
    // VisualEffectSystem.h) -- the player-miss flash (m_miss_flash_effect_system
    // below) is its first consumer, not its only one. Advanced from OnUpdate
    // directly, same unconditional-every-frame reasoning as m_floating_text.
    // Holds only pointers into m_registry/m_grid, so declaration order
    // relative to them doesn't matter for construction/destruction safety.
    std::optional<VisualEffectSystem> m_visual_effects;

    // Bridges AttackMissEvent onto m_visual_effects for a player-only flash --
    // see MissFlashEffectSystem.h. Holds only a pointer into m_visual_effects
    // (declared just above) plus the player's entt::entity, so it must be
    // constructed after m_visual_effects but declaration order relative to
    // m_registry/m_grid doesn't matter.
    std::optional<MissFlashEffectSystem> m_miss_flash_effect_system;

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

    // GameStateMachine and its five states this round -- declaration order
    // matters: m_target_selection_state/m_game_over_state/m_animation_state
    // must outlive m_exploring_state (which holds references to all three)
    // and all five must outlive m_state_machine's use of any of them.
    // m_character_screen_state isn't referenced by ExploringState's
    // constructor (unlike the other three) -- it's pushed directly from
    // GameplayLayer::OnEvent's own 'C'-key interception instead, not from
    // inside ExploringState::Update -- but it does need m_affixes (declared
    // well above this block) constructed first for its own constructor
    // argument. m_techniques_screen_state follows the same pattern, pushed on
    // its own 'T'-key interception, needing m_techniques/m_photon_arts
    // (declared well above this block) constructed first.
    TargetSelectionState m_target_selection_state;
    GameOverState m_game_over_state;
    AnimationState m_animation_state;
    CharacterScreenState m_character_screen_state{m_affixes};
    TechniquesScreenState m_techniques_screen_state{m_techniques, m_photon_arts};
    ExploringState m_exploring_state{m_target_selection_state, m_game_over_state, m_animation_state};
    GameStateMachine m_state_machine;

    std::optional<TextureAtlas> m_atlas;
    std::optional<TileGpuPipeline> m_gpu_pipeline;
    std::optional<RegistryRenderableLookup> m_renderable_lookup;
    std::optional<FogOfWarRenderableLookup> m_fog_lookup;
    std::optional<TileRenderer> m_tile_renderer;
};

} // namespace psr
