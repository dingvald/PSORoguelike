#include "Engine/ECS/JsonEntityLoader.h"

#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/NameIdRegistry.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstdint>
#include <fstream>
#include <string>

namespace {

// A fresh, empty subdirectory per TempDirectory instance, removed on
// destruction -- matches JsonDirectoryLoaderTests.cpp's pattern.
struct TempDirectory
{
    std::filesystem::path path;

    TempDirectory()
    {
        static std::atomic<int> counter{0};
        path = std::filesystem::temp_directory_path() / "PSORoguelike-JsonEntityLoaderTests" /
               ("run-" + std::to_string(counter++));
        std::filesystem::create_directories(path);
    }

    ~TempDirectory()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

void WriteText(const std::filesystem::path& path, const std::string& contents)
{
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << contents;
}

struct TestComponent
{
    int value = 0;
};

struct EmptyTagComponent
{
};

struct NameIdComponent
{
    std::uint32_t texture_id = 0;
};

std::uint32_t PrefabId(const std::string& id) { return entt::hashed_string::value(id.c_str()); }

} // namespace

TEST_CASE("JsonEntityLoader Load+Populate creates a prefab carrying the JSON-authored field value",
          "[JsonEntityLoader]")
{
    entt::meta_ctx ctx;
    psr::ComponentSchemaRegistrar reg{ctx};
    reg.Component<TestComponent>("test_component").Data<&TestComponent::value>("value");

    TempDirectory temp;
    WriteText(temp.path / "creatures" / "goblin.json",
              R"json({ "schema_version": 1, "components": { "test_component": { "value": 42 } } })json");

    psr::JsonEntityLoader loader(ctx);
    REQUIRE(loader.Load(temp.path));

    entt::registry prefab_registry;
    std::unordered_map<std::uint32_t, entt::entity> prefab_ids;
    loader.Populate(prefab_registry, prefab_ids);

    auto it = prefab_ids.find(PrefabId("creatures.goblin"));
    REQUIRE(it != prefab_ids.end());
    REQUIRE(prefab_registry.all_of<TestComponent>(it->second));
    REQUIRE(prefab_registry.get<TestComponent>(it->second).value == 42);
}

TEST_CASE("JsonEntityLoader Populate accepts a tag component's empty {} body", "[JsonEntityLoader]")
{
    entt::meta_ctx ctx;
    psr::ComponentSchemaRegistrar reg{ctx};
    reg.Component<EmptyTagComponent>("empty_tag");

    TempDirectory temp;
    WriteText(temp.path / "flags" / "blocked.json",
              R"json({ "schema_version": 1, "components": { "empty_tag": {} } })json");

    psr::JsonEntityLoader loader(ctx);
    loader.Load(temp.path);

    entt::registry prefab_registry;
    std::unordered_map<std::uint32_t, entt::entity> prefab_ids;
    loader.Populate(prefab_registry, prefab_ids);

    auto it = prefab_ids.find(PrefabId("flags.blocked"));
    REQUIRE(it != prefab_ids.end());
    REQUIRE(prefab_registry.all_of<EmptyTagComponent>(it->second));
}

TEST_CASE("JsonEntityLoader hashes a name string on a NameId field and registers its label", "[JsonEntityLoader]")
{
    entt::meta_ctx ctx;
    psr::ComponentSchemaRegistrar reg{ctx};
    reg.Component<NameIdComponent>("name_id_component").Data<&NameIdComponent::texture_id>("texture_id");

    TempDirectory temp;
    WriteText(temp.path / "props" / "crate.json",
              R"json({ "schema_version": 1, "components": { "name_id_component": { "texture_id": "crate_wood" } } })json");

    psr::JsonEntityLoader loader(ctx);
    loader.Load(temp.path);

    entt::registry prefab_registry;
    std::unordered_map<std::uint32_t, entt::entity> prefab_ids;
    loader.Populate(prefab_registry, prefab_ids);

    auto it = prefab_ids.find(PrefabId("props.crate"));
    REQUIRE(it != prefab_ids.end());
    const std::uint32_t texture_hash = prefab_registry.get<NameIdComponent>(it->second).texture_id;
    REQUIRE(texture_hash == entt::hashed_string::value("crate_wood"));

    std::optional<std::string> label = psr::NameIdRegistry::Find(texture_hash);
    REQUIRE(label.has_value());
    REQUIRE(*label == "crate_wood");
}

TEST_CASE("JsonEntityLoader Populate throws EntityLoaderError for an unregistered component name",
          "[JsonEntityLoader]")
{
    entt::meta_ctx ctx;
    psr::ComponentSchemaRegistrar reg{ctx};
    reg.Component<TestComponent>("test_component").Data<&TestComponent::value>("value");

    TempDirectory temp;
    WriteText(temp.path / "bad.json",
              R"json({ "schema_version": 1, "components": { "not_registered": { "value": 1 } } })json");

    psr::JsonEntityLoader loader(ctx);
    loader.Load(temp.path);

    entt::registry prefab_registry;
    std::unordered_map<std::uint32_t, entt::entity> prefab_ids;
    REQUIRE_THROWS_AS(loader.Populate(prefab_registry, prefab_ids), psr::EntityLoaderError);
}

TEST_CASE("JsonEntityLoader Populate throws EntityLoaderError on a duplicate prefab id", "[JsonEntityLoader]")
{
    entt::meta_ctx ctx;
    psr::ComponentSchemaRegistrar reg{ctx};
    reg.Component<TestComponent>("test_component").Data<&TestComponent::value>("value");

    TempDirectory temp;
    WriteText(temp.path / "creatures" / "goblin.json",
              R"json({ "schema_version": 1, "components": { "test_component": { "value": 1 } } })json");

    psr::JsonEntityLoader loader(ctx);
    loader.Load(temp.path);

    entt::registry prefab_registry;
    std::unordered_map<std::uint32_t, entt::entity> prefab_ids;
    loader.Populate(prefab_registry, prefab_ids);

    // Re-populating into the same id map (as if a second loader authored the
    // same id) must be rejected rather than silently overwriting.
    REQUIRE_THROWS_AS(loader.Populate(prefab_registry, prefab_ids), psr::EntityLoaderError);
}
