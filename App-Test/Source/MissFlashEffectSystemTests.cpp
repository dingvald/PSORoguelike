#include "Systems/MissFlashEffectSystem.h"

#include "Actions/AttackAction.h"
#include "CombatRegistrySetup.h"
#include "Components/EquipmentComponent.h"
#include "Components/PlayerControlledComponent.h"
#include "Components/RegisterComponents.h"
#include "Components/RenderableComponent.h"
#include "Components/StatsComponent.h"
#include "Components/WeaponComponent.h"
#include "Engine/Combat/DamageEvent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/HealthComponent.h"
#include "Engine/ECS/IEntityLoader.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Render/VisualEffectSystem.h"
#include "Engine/World/Grid.h"
#include "Systems/TweenSystem.h"

#include <catch2/catch_test_macros.hpp>
#include <entt/core/hashed_string.hpp>
#include <random>

namespace {

// Must match MissFlashEffectSystem.cpp's own kMissFlashPrefabId -- this test
// stands in for the real "vfx.miss_flash" prefab JSON (see
// App/Assets/Data/Entities/vfx/miss_flash.json), registered here directly so
// the test doesn't depend on the content-loading pipeline.
const std::uint32_t kFlashPrefabId = entt::hashed_string::value("vfx.miss_flash");

class TestEntityLoader : public psr::IEntityLoader
{
public:
    bool Load(std::filesystem::path /*path*/) override { return true; }

    void Populate(entt::registry& prefab_registry,
                  std::unordered_map<std::uint32_t, entt::entity>& out_prefab_ids) override
    {
        out_prefab_ids.emplace(kFlashPrefabId, prefab_registry.create());
    }
};

} // namespace

TEST_CASE("MissFlashEffectSystem spawns a visual effect at the player's tile when the player is missed",
          "[MissFlashEffectSystem]")
{
    psr::Registry registry;
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);
    psr::Grid grid{3, 3};

    psr::VisualEffectSystem visual_effects(registry, grid, [](entt::entity, std::uint8_t) {});

    entt::entity player_handle = registry.CreateEntity();
    psr::Entity player(registry, player_handle);
    player.Emplace<psr::Position>(psr::Vec2{2, 2});

    psr::MissFlashEffectSystem miss_flash(visual_effects, player_handle);

    entt::entity actor_handle = registry.CreateEntity();
    psr::Entity actor(registry, actor_handle);
    miss_flash.Subscribe(actor);

    psr::AttackMissEvent event{player};
    actor.Dispatch(event);

    REQUIRE(grid.GetEntities(psr::Vec2{2, 2}).size() == 1);
}

TEST_CASE("MissFlashEffectSystem is a no-op when the miss's target isn't the player", "[MissFlashEffectSystem]")
{
    psr::Registry registry;
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);
    psr::Grid grid{3, 3};

    psr::VisualEffectSystem visual_effects(registry, grid, [](entt::entity, std::uint8_t) {});

    entt::entity player_handle = registry.CreateEntity();
    psr::MissFlashEffectSystem miss_flash(visual_effects, player_handle);

    entt::entity actor_handle = registry.CreateEntity();
    psr::Entity actor(registry, actor_handle);
    miss_flash.Subscribe(actor);

    entt::entity enemy_handle = registry.CreateEntity();
    psr::Entity enemy(registry, enemy_handle);
    enemy.Emplace<psr::Position>(psr::Vec2{1, 1});

    psr::AttackMissEvent event{enemy};
    actor.Dispatch(event);

    REQUIRE(grid.GetEntities(psr::Vec2{1, 1}).empty());
}

TEST_CASE("MissFlashEffectSystem is a no-op when the player has no Position", "[MissFlashEffectSystem]")
{
    psr::Registry registry;
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);
    psr::Grid grid{3, 3};

    psr::VisualEffectSystem visual_effects(registry, grid, [](entt::entity, std::uint8_t) {});

    entt::entity player_handle = registry.CreateEntity();
    psr::Entity player(registry, player_handle);
    psr::MissFlashEffectSystem miss_flash(visual_effects, player_handle);

    entt::entity actor_handle = registry.CreateEntity();
    psr::Entity actor(registry, actor_handle);
    miss_flash.Subscribe(actor);

    psr::AttackMissEvent event{player};
    actor.Dispatch(event);

    REQUIRE(grid.GetEntities(psr::Vec2{0, 0}).empty());
}

namespace {

class RenderableEntityLoader : public psr::IEntityLoader
{
public:
    bool Load(std::filesystem::path /*path*/) override { return true; }

    void Populate(entt::registry& prefab_registry,
                  std::unordered_map<std::uint32_t, entt::entity>& out_prefab_ids) override
    {
        const entt::entity prefab = prefab_registry.create();
        prefab_registry.emplace<psr::RenderableComponent>(prefab);
        out_prefab_ids.emplace(kFlashPrefabId, prefab);
    }
};

} // namespace

TEST_CASE("Killing an EquipmentComponent+HealthComponent entity does not corrupt EventHandlerComponent "
          "storage for its recycled index",
          "[MissFlashEffectSystem][Registry]")
{
    // Regression test for a real "Slot not available" entt sparse_set assert
    // hit in play: SetUpCombatRegistry's BindComponentEvents<EquipmentComponent>/
    // BindSystemEvents<HealthComponent, HealthSystem/DeathSystem> calls run
    // before any entity exists, so those components' storage gets registered
    // in entt's internal pool list *before* EventHandlerComponent's (which
    // only gets lazily created on the first Registry::CreateEntity() call).
    // entt::registry::destroy() removes an entity from its pools in *reverse*
    // registration order, so EventHandlerComponent is removed before
    // Equipment/HealthComponent's on_destroy<T> handlers (EquipmentComponent::
    // DetachHandlers, HealthSystem::DetachHandlers, DeathSystem::
    // DetachHandlers) even run. Those handlers used to fetch the entity's
    // EventHandlerComponent via GetOrEmplace -- which, since it had *just*
    // been removed, silently re-created it mid-destroy(), corrupting that
    // pool's bookkeeping for the entity's index. The corruption stayed latent
    // until the index was recycled by a later CreateEntity(), which then hit
    // the "Slot not available" assert on emplace<EventHandlerComponent>.
    psr::Registry registry;
    psr::Grid grid{3, 3};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);

    entt::entity doomed = registry.CreateEntity();
    entt::entity weapon = registry.CreateEntity();
    registry.Emplace<psr::WeaponComponent>(weapon);
    registry.Emplace<psr::EquipmentComponent>(doomed, psr::EquipmentComponent{weapon});
    registry.Emplace<psr::HealthComponent>(doomed);

    registry.DestroyEntity(doomed);

    // Recycles `doomed`'s entity index (entt's free list is LIFO) -- this is
    // the emplace<EventHandlerComponent> call that used to assert.
    entt::entity recycled = registry.CreateEntity();

    REQUIRE(registry.IsValid(recycled));
}

TEST_CASE("MissFlashEffectSystem still spawns after the attacking enemy dies mid-turn from the player's "
          "own counterattack",
          "[MissFlashEffectSystem][stress]")
{
    // End-to-end version of the Registry-level regression test above, through
    // the real AttackAction/EventHandlerComponent::Dispatch/HealthSystem/
    // DeathSystem/MissFlashEffectSystem pipeline: a nested DeathEvent
    // dispatch on the killed entity (see HealthSystem::ApplyIncomingDamage)
    // triggers the same destroy()-mid-iteration corruption, and the
    // recycled index later gets handed to MissFlashEffectSystem's own
    // CreateEntity(vfx.miss_flash) call.
    psr::Registry registry;
    psr::RegisterComponents(registry);
    RenderableEntityLoader loader;
    registry.RegisterPrefabs(loader);
    psr::Grid grid{5, 5};
    psr::AffixLibrary affixes;
    psr::StatusEffectLibrary status_effects;
    psr::SetUpCombatRegistry(registry, grid, affixes, status_effects);

    psr::VisualEffectSystem visual_effects(registry, grid, [](entt::entity, std::uint8_t) {});

    entt::entity player_handle = registry.CreateEntity();
    psr::Entity player(registry, player_handle);
    player.Emplace<psr::Position>(psr::Vec2{2, 2});
    player.Emplace<psr::PlayerControlledComponent>();
    psr::StatsComponent player_stats;
    player_stats.atp = 100;
    player_stats.ata = 200;
    player.Emplace<psr::StatsComponent>(player_stats);
    psr::HealthComponent player_health;
    player_health.current_hp = 1000;
    player_health.max_hp = 1000;
    player.Emplace<psr::HealthComponent>(player_health);
    grid.AddEntity(psr::Vec2{2, 2}, player_handle);

    entt::entity player_weapon = registry.CreateEntity();
    psr::WeaponComponent player_weapon_component;
    player_weapon_component.hits_per_turn = 1;
    registry.Emplace<psr::WeaponComponent>(player_weapon, player_weapon_component);
    registry.Emplace<psr::StatsComponent>(player_weapon);
    player.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{player_weapon});

    psr::MissFlashEffectSystem miss_flash(visual_effects, player_handle);
    miss_flash.Subscribe(player);

    entt::entity enemy_handle = registry.CreateEntity();
    psr::Entity enemy(registry, enemy_handle);
    enemy.Emplace<psr::Position>(psr::Vec2{3, 2});
    psr::StatsComponent enemy_stats;
    enemy_stats.dfp = 0;
    enemy_stats.evp = 0;
    enemy.Emplace<psr::StatsComponent>(enemy_stats);
    psr::HealthComponent enemy_health;
    enemy_health.current_hp = 1; // dies to the player's first landed hit
    enemy_health.max_hp = 1;
    enemy.Emplace<psr::HealthComponent>(enemy_health);
    grid.AddEntity(psr::Vec2{3, 2}, enemy_handle);

    entt::entity enemy_weapon = registry.CreateEntity();
    psr::WeaponComponent enemy_weapon_component;
    enemy_weapon_component.hits_per_turn = 1;
    registry.Emplace<psr::WeaponComponent>(enemy_weapon, enemy_weapon_component);
    registry.Emplace<psr::StatsComponent>(enemy_weapon);
    enemy.Emplace<psr::EquipmentComponent>(psr::EquipmentComponent{enemy_weapon});
    miss_flash.Subscribe(enemy);

    std::mt19937 rng{1};
    psr::AttackAction player_attack(grid, affixes, psr::Vec2{1, 0}, rng);
    for (int attempt = 0; attempt < 50 && registry.IsValid(enemy_handle); ++attempt)
    {
        player_attack.Perform(player);
        psr::UpdateTweens(registry, 999.0f); // eventually kills the enemy, recycling its entity index
    }

    REQUIRE_FALSE(registry.IsValid(enemy_handle));

    // A fresh actor, likely reusing the just-freed index, whose own miss
    // needs to CreateEntity(vfx.miss_flash) without hitting the corrupted
    // EventHandlerComponent slot.
    entt::entity new_attacker = registry.CreateEntity();
    psr::Entity attacker(registry, new_attacker);
    miss_flash.Subscribe(attacker);

    psr::AttackMissEvent miss{player};
    attacker.Dispatch(miss);

    REQUIRE(grid.GetEntities(psr::Vec2{2, 2}).size() == 2); // player + the miss-flash effect
}
