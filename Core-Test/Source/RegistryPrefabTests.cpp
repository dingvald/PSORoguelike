#include "Engine/ECS/ComponentMeta.h"
#include "Engine/ECS/EventHandlerComponent.h"
#include "Engine/ECS/Registry.h"

#include <catch2/catch_test_macros.hpp>

namespace {

struct TestComponent
{
    int value = 0;

    static void Register(entt::meta_ctx& ctx)
    {
        using namespace entt::literals;
        entt::meta_factory<TestComponent>(ctx)
            .data<&TestComponent::value>("value"_hs)
            .func<&psr::CloneComponent<TestComponent>>("clone"_hs);
    }
};

struct EmptyTagComponent
{
    static void Register(entt::meta_ctx& ctx)
    {
        using namespace entt::literals;
        entt::meta_factory<EmptyTagComponent>(ctx).func<&psr::CloneComponent<EmptyTagComponent>>("clone"_hs);
    }
};

constexpr std::uint32_t kTestPrefabId = 1;
constexpr std::uint32_t kTagPrefabId = 2;

class TestEntityLoader : public psr::IEntityLoader
{
public:
    bool Load(std::filesystem::path /*path*/) override { return true; }

    void Populate(entt::registry& prefab_registry,
                  std::unordered_map<std::uint32_t, entt::entity>& out_prefab_ids) override
    {
        entt::entity prefab = prefab_registry.create();
        prefab_registry.emplace<TestComponent>(prefab, 42);
        out_prefab_ids.emplace(kTestPrefabId, prefab);

        entt::entity tag_prefab = prefab_registry.create();
        prefab_registry.emplace<EmptyTagComponent>(tag_prefab);
        out_prefab_ids.emplace(kTagPrefabId, tag_prefab);
    }
};

constexpr std::uint32_t kReloadedPrefabId = 3;

// Second call's content differs from the first (kTestPrefabId/kTagPrefabId
// gone, a new id present) -- exercises RegisterPrefabs replacing rather than
// appending to the previous prefab set, as a ContentWatcher-driven reload
// would.
class ReloadingEntityLoader : public psr::IEntityLoader
{
public:
    bool Load(std::filesystem::path /*path*/) override { return true; }

    void Populate(entt::registry& prefab_registry,
                  std::unordered_map<std::uint32_t, entt::entity>& out_prefab_ids) override
    {
        entt::entity prefab = prefab_registry.create();
        prefab_registry.emplace<TestComponent>(prefab, 99);
        out_prefab_ids.emplace(kReloadedPrefabId, prefab);
    }
};

} // namespace

TEST_CASE("Registry CreateEntity(prefab_id) clones the prefab's components onto a new runtime entity",
          "[Registry][Prefab]")
{
    psr::Registry registry;
    TestComponent::Register(registry.GetMetaContext());
    EmptyTagComponent::Register(registry.GetMetaContext());

    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    entt::entity entity = registry.CreateEntity(kTestPrefabId);

    REQUIRE(registry.IsValid(entity));
    REQUIRE(registry.HasComponent<TestComponent>(entity));
    REQUIRE(registry.GetComponent<TestComponent>(entity).value == 42);
}

TEST_CASE("Registry CreateEntity(prefab_id) clones an empty tag component (no per-entity value storage)",
          "[Registry][Prefab]")
{
    // Regression test: entt's empty-type storage optimization never exposes
    // a per-entity value pointer (unlike a normal component), so a clone
    // path that relies on wrapping that pointer as a meta instance silently
    // fails to clone tag components -- see CloneComponent<T>'s doc comment
    // in ComponentMeta.h for the fix (static-style dispatch, no
    // TComponent-typed parameter).
    psr::Registry registry;
    TestComponent::Register(registry.GetMetaContext());
    EmptyTagComponent::Register(registry.GetMetaContext());

    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    entt::entity entity = registry.CreateEntity(kTagPrefabId);

    REQUIRE(registry.IsValid(entity));
    REQUIRE(registry.HasComponent<EmptyTagComponent>(entity));
}

TEST_CASE("Registry CreateEntity(prefab_id) produces independent instances that don't affect the prefab",
          "[Registry][Prefab]")
{
    psr::Registry registry;
    TestComponent::Register(registry.GetMetaContext());

    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    entt::entity first = registry.CreateEntity(kTestPrefabId);
    registry.GetComponent<TestComponent>(first).value = 100;

    entt::entity second = registry.CreateEntity(kTestPrefabId);

    REQUIRE(first != second);
    REQUIRE(registry.GetComponent<TestComponent>(second).value == 42);
}

TEST_CASE("Registry CreateEntity(prefab_id) still attaches EventHandlerComponent as the first component",
          "[Registry][Prefab][Events]")
{
    // EventHandlerComponent must exist before any cloned component's
    // on_construct<T> handler could fire -- see Registry::CreateEntity's own
    // ordering comment.
    psr::Registry registry;
    TestComponent::Register(registry.GetMetaContext());

    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    entt::entity entity = registry.CreateEntity(kTestPrefabId);

    REQUIRE(registry.HasComponent<psr::EventHandlerComponent>(entity));
}

TEST_CASE("Registry runtime and prefab registries stay disjoint", "[Registry][Prefab]")
{
    psr::Registry registry;
    TestComponent::Register(registry.GetMetaContext());

    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    entt::entity plain = registry.CreateEntity();

    REQUIRE(registry.IsValid(plain));
    REQUIRE_FALSE(registry.HasComponent<TestComponent>(plain));
}

TEST_CASE("Registry HasPrefab reports whether a prefab id was registered", "[Registry][Prefab]")
{
    psr::Registry registry;
    TestComponent::Register(registry.GetMetaContext());

    REQUIRE_FALSE(registry.HasPrefab(kTestPrefabId));

    TestEntityLoader loader;
    registry.RegisterPrefabs(loader);

    REQUIRE(registry.HasPrefab(kTestPrefabId));
    REQUIRE_FALSE(registry.HasPrefab(999));
}

TEST_CASE("Registry RegisterPrefabs called again fully replaces the previous prefab set", "[Registry][Prefab]")
{
    psr::Registry registry;
    TestComponent::Register(registry.GetMetaContext());
    EmptyTagComponent::Register(registry.GetMetaContext());

    TestEntityLoader first_loader;
    registry.RegisterPrefabs(first_loader);
    REQUIRE(registry.HasPrefab(kTestPrefabId));
    REQUIRE(registry.HasPrefab(kTagPrefabId));

    ReloadingEntityLoader second_loader;
    registry.RegisterPrefabs(second_loader);

    // Old ids are gone -- not just unreferenced, but no longer registered.
    REQUIRE_FALSE(registry.HasPrefab(kTestPrefabId));
    REQUIRE_FALSE(registry.HasPrefab(kTagPrefabId));

    // New id from the second load works normally.
    REQUIRE(registry.HasPrefab(kReloadedPrefabId));
    entt::entity entity = registry.CreateEntity(kReloadedPrefabId);
    REQUIRE(registry.IsValid(entity));
    REQUIRE(registry.GetComponent<TestComponent>(entity).value == 99);
}
