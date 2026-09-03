#include "Engine/Render/VisualEffectSystem.h"

#include "Engine/ECS/IEntityLoader.h"
#include "Engine/ECS/Registry.h"
#include "Engine/World/Grid.h"

#include <catch2/catch_test_macros.hpp>

namespace {

constexpr std::uint32_t kEffectPrefabId = 1;
constexpr std::uint32_t kUnregisteredPrefabId = 999;

class TestEntityLoader : public psr::IEntityLoader
{
public:
    bool Load(std::filesystem::path /*path*/) override { return true; }

    void Populate(entt::registry& prefab_registry,
                  std::unordered_map<std::uint32_t, entt::entity>& out_prefab_ids) override
    {
        out_prefab_ids.emplace(kEffectPrefabId, prefab_registry.create());
    }
};

struct Fixture
{
    psr::Registry registry;
    psr::Grid grid{3, 3};
    std::vector<std::pair<entt::entity, std::uint8_t>> alpha_calls;

    psr::VisualEffectSystem MakeSystem()
    {
        TestEntityLoader loader;
        registry.RegisterPrefabs(loader);
        return psr::VisualEffectSystem(registry, grid,
                                       [this](entt::entity entity, std::uint8_t alpha)
                                       { alpha_calls.emplace_back(entity, alpha); });
    }
};

} // namespace

TEST_CASE("VisualEffectSystem::Spawn places the entity on the grid at the given tile", "[VisualEffectSystem]")
{
    Fixture fixture;
    psr::VisualEffectSystem system = fixture.MakeSystem();

    const entt::entity entity =
        system.Spawn(kEffectPrefabId, psr::Vec2{1, 2}, 0.2f, psr::EasingCurve::Linear);

    REQUIRE((entity != entt::null));
    REQUIRE(fixture.registry.IsValid(entity));
    REQUIRE(fixture.grid.GetEntities(psr::Vec2{1, 2}) == std::vector<entt::entity>{entity});
}

TEST_CASE("VisualEffectSystem::Spawn returns entt::null for an unregistered prefab id", "[VisualEffectSystem]")
{
    Fixture fixture;
    psr::VisualEffectSystem system = fixture.MakeSystem();

    const entt::entity entity =
        system.Spawn(kUnregisteredPrefabId, psr::Vec2{0, 0}, 0.2f, psr::EasingCurve::Linear);

    REQUIRE((entity == entt::null));
}

TEST_CASE("VisualEffectSystem::Spawn immediately pushes start_alpha through the sink", "[VisualEffectSystem]")
{
    Fixture fixture;
    psr::VisualEffectSystem system = fixture.MakeSystem();

    const entt::entity entity =
        system.Spawn(kEffectPrefabId, psr::Vec2{0, 0}, 1.0f, psr::EasingCurve::Linear, 200, 0);

    REQUIRE(fixture.alpha_calls.size() == 1);
    REQUIRE(fixture.alpha_calls[0] == std::pair{entity, std::uint8_t{200}});
}

TEST_CASE("VisualEffectSystem::Update eases alpha from start_alpha to end_alpha over duration",
          "[VisualEffectSystem]")
{
    Fixture fixture;
    psr::VisualEffectSystem system = fixture.MakeSystem();

    const entt::entity entity =
        system.Spawn(kEffectPrefabId, psr::Vec2{0, 0}, 1.0f, psr::EasingCurve::Linear, 255, 0);
    fixture.alpha_calls.clear();

    system.Update(0.5f);

    REQUIRE(fixture.alpha_calls.size() == 1);
    REQUIRE(fixture.alpha_calls[0].first == entity);
    REQUIRE(fixture.alpha_calls[0].second == std::uint8_t{128});
}

TEST_CASE("VisualEffectSystem::Update removes and destroys an instance once elapsed reaches duration",
          "[VisualEffectSystem]")
{
    Fixture fixture;
    psr::VisualEffectSystem system = fixture.MakeSystem();

    const entt::entity entity = system.Spawn(kEffectPrefabId, psr::Vec2{1, 1}, 1.0f, psr::EasingCurve::Linear);

    system.Update(0.9f);
    REQUIRE(fixture.registry.IsValid(entity));
    REQUIRE(fixture.grid.GetEntities(psr::Vec2{1, 1}) == std::vector<entt::entity>{entity});

    system.Update(0.2f); // elapsed now 1.1s, past the 1.0s duration

    REQUIRE_FALSE(fixture.registry.IsValid(entity));
    REQUIRE(fixture.grid.GetEntities(psr::Vec2{1, 1}).empty());
}

TEST_CASE("VisualEffectSystem tracks and expires multiple instances independently", "[VisualEffectSystem]")
{
    Fixture fixture;
    psr::VisualEffectSystem system = fixture.MakeSystem();

    const entt::entity short_lived = system.Spawn(kEffectPrefabId, psr::Vec2{0, 0}, 0.5f, psr::EasingCurve::Linear);
    const entt::entity long_lived = system.Spawn(kEffectPrefabId, psr::Vec2{1, 1}, 2.0f, psr::EasingCurve::Linear);

    system.Update(0.6f); // past the short instance's duration, not the long one's

    REQUIRE_FALSE(fixture.registry.IsValid(short_lived));
    REQUIRE(fixture.registry.IsValid(long_lived));
}
