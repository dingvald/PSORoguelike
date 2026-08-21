#include "Engine/ECS/Registry.h"

#include "Engine/ECS/ComponentSchemaRegistrar.h"

#include <catch2/catch_test_macros.hpp>

namespace {

struct PositionComponent
{
    int x = 0;
    int y = 0;
};

struct TagComponent
{
};

struct CountingComponent
{
    static inline int attach_count = 0;
    static inline int detach_count = 0;

    static void AttachHandlers(entt::registry&, entt::entity) { ++attach_count; }
    static void DetachHandlers(entt::registry&, entt::entity) { ++detach_count; }
};

struct FromEnttTestComponent
{
    static inline psr::Registry* recovered = nullptr;

    static void AttachHandlers(entt::registry& registry, entt::entity) { recovered = &psr::Registry::FromEntt(registry); }
    static void DetachHandlers(entt::registry&, entt::entity) {}
};

} // namespace

TEST_CASE("Registry creates and destroys entities", "[Registry]")
{
    psr::Registry registry;

    entt::entity entity = registry.CreateEntity();
    REQUIRE(registry.IsValid(entity));

    registry.DestroyEntity(entity);
    REQUIRE_FALSE(registry.IsValid(entity));
}

TEST_CASE("Registry Emplace/GetComponent/HasComponent round-trip", "[Registry]")
{
    psr::Registry registry;
    entt::entity entity = registry.CreateEntity();

    REQUIRE_FALSE(registry.HasComponent<PositionComponent>(entity));

    registry.Emplace<PositionComponent>(entity, 3, 4);

    REQUIRE(registry.HasComponent<PositionComponent>(entity));
    const PositionComponent& position = registry.GetComponent<PositionComponent>(entity);
    REQUIRE(position.x == 3);
    REQUIRE(position.y == 4);

    registry.Remove<PositionComponent>(entity);
    REQUIRE_FALSE(registry.HasComponent<PositionComponent>(entity));
}

TEST_CASE("Registry TryGetComponent returns nullptr when absent", "[Registry]")
{
    psr::Registry registry;
    entt::entity entity = registry.CreateEntity();

    REQUIRE(registry.TryGetComponent<PositionComponent>(entity) == nullptr);

    registry.Emplace<PositionComponent>(entity, 1, 2);
    REQUIRE(registry.TryGetComponent<PositionComponent>(entity) != nullptr);
}

TEST_CASE("Registry Each visits every entity with the component", "[Registry]")
{
    psr::Registry registry;

    entt::entity with_tag_a = registry.CreateEntity();
    entt::entity with_tag_b = registry.CreateEntity();
    entt::entity without_tag = registry.CreateEntity();

    registry.Emplace<TagComponent>(with_tag_a);
    registry.Emplace<TagComponent>(with_tag_b);
    (void)without_tag;

    int visit_count = 0;
    registry.Each<TagComponent>([&visit_count](entt::entity) { ++visit_count; });

    REQUIRE(visit_count == 2);
}

TEST_CASE("Registry Each with a filter type only visits entities that have both", "[Registry]")
{
    psr::Registry registry;

    entt::entity both = registry.CreateEntity();
    entt::entity only_position = registry.CreateEntity();

    registry.Emplace<PositionComponent>(both, 1, 1);
    registry.Emplace<TagComponent>(both);
    registry.Emplace<PositionComponent>(only_position, 2, 2);

    int visit_count = 0;
    registry.Each<PositionComponent, TagComponent>([&visit_count](entt::entity, PositionComponent&) { ++visit_count; });

    REQUIRE(visit_count == 1);
}

TEST_CASE("Registry Any reports whether any live entity has the component", "[Registry]")
{
    psr::Registry registry;

    REQUIRE_FALSE(registry.Any<TagComponent>());

    entt::entity entity = registry.CreateEntity();
    registry.Emplace<TagComponent>(entity);

    REQUIRE(registry.Any<TagComponent>());
}

TEST_CASE("Registry GetOrEmplace default-constructs a component on first access", "[Registry]")
{
    psr::Registry registry;
    entt::entity entity = registry.CreateEntity();

    REQUIRE_FALSE(registry.HasComponent<PositionComponent>(entity));

    PositionComponent& position = registry.GetOrEmplace<PositionComponent>(entity, 5, 6);
    REQUIRE(position.x == 5);
    REQUIRE(position.y == 6);

    // A second call must not overwrite the existing value.
    PositionComponent& again = registry.GetOrEmplace<PositionComponent>(entity, 9, 9);
    REQUIRE(again.x == 5);
    REQUIRE(again.y == 6);
}

TEST_CASE("Registry Clear removes the component from every entity without destroying them", "[Registry]")
{
    psr::Registry registry;

    entt::entity first = registry.CreateEntity();
    entt::entity second = registry.CreateEntity();
    registry.Emplace<TagComponent>(first);
    registry.Emplace<TagComponent>(second);

    registry.Clear<TagComponent>();

    REQUIRE_FALSE(registry.HasComponent<TagComponent>(first));
    REQUIRE_FALSE(registry.HasComponent<TagComponent>(second));
    REQUIRE(registry.IsValid(first));
    REQUIRE(registry.IsValid(second));
}

TEST_CASE("Registry BindComponentEvents fires AttachHandlers/DetachHandlers on construct/destroy", "[Registry]")
{
    psr::Registry registry;
    registry.BindComponentEvents<CountingComponent>();

    entt::entity entity = registry.CreateEntity();
    CountingComponent::attach_count = 0;
    CountingComponent::detach_count = 0;

    registry.Emplace<CountingComponent>(entity);
    REQUIRE(CountingComponent::attach_count == 1);
    REQUIRE(CountingComponent::detach_count == 0);

    registry.Remove<CountingComponent>(entity);
    REQUIRE(CountingComponent::attach_count == 1);
    REQUIRE(CountingComponent::detach_count == 1);
}

TEST_CASE("Registry FromEntt recovers the owning Registry from within a bound component handler", "[Registry]")
{
    psr::Registry registry;
    registry.BindComponentEvents<FromEnttTestComponent>();
    FromEnttTestComponent::recovered = nullptr;

    entt::entity entity = registry.CreateEntity();
    registry.Emplace<FromEnttTestComponent>(entity);

    REQUIRE(FromEnttTestComponent::recovered == &registry);
}

TEST_CASE("Registry DescribeEntity reads a live entity's fields via the registered schema", "[Registry]")
{
    psr::Registry registry;
    psr::ComponentSchemaRegistrar reg{registry.GetMetaContext()};
    reg.Component<PositionComponent>("position")
        .Data<&PositionComponent::x>("x")
        .Data<&PositionComponent::y>("y");
    reg.Component<TagComponent>("tag");
    const psr::EntitySchemaModel model = reg.Model();

    entt::entity entity = registry.CreateEntity();
    registry.Emplace<PositionComponent>(entity, 7, 8);

    const std::vector<psr::ComponentValue> described = registry.DescribeEntity(entity, model);

    // Only "position" is present on the entity -- "tag" is silently skipped.
    REQUIRE(described.size() == 1);
    CHECK(described[0].component_id == "position");
    REQUIRE(described[0].fields.size() == 2);
    CHECK(described[0].fields[0].name == "x");
    CHECK(described[0].fields[0].text == "7");
    CHECK(described[0].fields[1].name == "y");
    CHECK(described[0].fields[1].text == "8");
}
