#include "Systems/HealEffectSystem.h"

#include "Engine/Combat/HealEvent.h"
#include "Engine/ECS/Entity.h"
#include "Engine/ECS/IEntityLoader.h"
#include "Engine/ECS/Position.h"
#include "Engine/ECS/Registry.h"
#include "Engine/Render/VisualEffectSystem.h"
#include "Engine/World/Grid.h"

#include <catch2/catch_test_macros.hpp>
#include <entt/core/hashed_string.hpp>

namespace {

// Must match HealEffectSystem.cpp's own kHealEffectPrefabId -- this test
// stands in for the real "vfx.heal_glow" prefab JSON (see
// App/Assets/Data/Entities/vfx/heal_glow.json), registered here directly so
// the test doesn't depend on the content-loading pipeline.
const std::uint32_t kHealGlowPrefabId = entt::hashed_string::value("vfx.heal_glow");

class TestEntityLoader : public psr::IEntityLoader
{
public:
    bool Load(std::filesystem::path /*path*/) override { return true; }

    void Populate(entt::registry& prefab_registry,
                  std::unordered_map<std::uint32_t, entt::entity>& out_prefab_ids) override
    {
        out_prefab_ids.emplace(kHealGlowPrefabId, prefab_registry.create());
    }
};

} // namespace

TEST_CASE("HealEffectSystem spawns a visual effect at the healed entity's tile", "[HealEffectSystem]")
{
    psr::Registry registry;
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);
    psr::Grid grid{3, 3};

    psr::VisualEffectSystem visual_effects(registry, grid, [](entt::entity, std::uint8_t) {});
    psr::HealEffectSystem heal_effect(visual_effects);

    entt::entity source_handle = registry.CreateEntity();
    psr::Entity source(registry, source_handle);
    heal_effect.Subscribe(source);

    entt::entity target_handle = registry.CreateEntity();
    psr::Entity target(registry, target_handle);
    target.Emplace<psr::Position>(psr::Vec2{2, 2});

    psr::AfterHealEvent event{target, /*amount=*/25};
    source.Dispatch(event);

    REQUIRE(grid.GetEntities(psr::Vec2{2, 2}).size() == 1);
}

TEST_CASE("HealEffectSystem is a no-op when the target has no Position", "[HealEffectSystem]")
{
    psr::Registry registry;
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);
    psr::Grid grid{3, 3};

    psr::VisualEffectSystem visual_effects(registry, grid, [](entt::entity, std::uint8_t) {});
    psr::HealEffectSystem heal_effect(visual_effects);

    entt::entity source_handle = registry.CreateEntity();
    psr::Entity source(registry, source_handle);
    heal_effect.Subscribe(source);

    entt::entity target_handle = registry.CreateEntity();
    psr::Entity target(registry, target_handle);

    psr::AfterHealEvent event{target, /*amount=*/10};
    source.Dispatch(event);

    REQUIRE(grid.GetEntities(psr::Vec2{0, 0}).empty());
}

TEST_CASE("HealEffectSystem is a no-op when the actually-applied amount is zero", "[HealEffectSystem]")
{
    psr::Registry registry;
    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);
    psr::Grid grid{3, 3};

    psr::VisualEffectSystem visual_effects(registry, grid, [](entt::entity, std::uint8_t) {});
    psr::HealEffectSystem heal_effect(visual_effects);

    entt::entity source_handle = registry.CreateEntity();
    psr::Entity source(registry, source_handle);
    heal_effect.Subscribe(source);

    entt::entity target_handle = registry.CreateEntity();
    psr::Entity target(registry, target_handle);
    target.Emplace<psr::Position>(psr::Vec2{1, 1});

    psr::AfterHealEvent event{target, /*amount=*/0}; // e.g. an item used at full HP
    source.Dispatch(event);

    REQUIRE(grid.GetEntities(psr::Vec2{1, 1}).empty());
}
