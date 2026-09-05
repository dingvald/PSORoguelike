#include "Components/ActorComponent.h"
#include "Engine/ECS/ComponentSchemaRegistrar.h"
#include "Engine/ECS/JsonEntityLoader.h"

#include <catch2/catch_test_macros.hpp>
#include <entt/entt.hpp>

#include <atomic>
#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>

namespace {

// Matches JsonEntityLoaderTests.cpp's TempDirectory pattern.
struct TempDirectory
{
    std::filesystem::path path;

    TempDirectory()
    {
        static std::atomic<int> counter{0};
        path = std::filesystem::temp_directory_path() / "PSORoguelike-ActorComponentTests" /
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

std::uint32_t PrefabId(const std::string& id) { return entt::hashed_string::value(id.c_str()); }

} // namespace

TEST_CASE("ActorComponent registers as authorable with three integer fields", "[EntitySchema]")
{
    entt::meta_ctx ctx;
    psr::ComponentSchemaRegistrar reg{ctx};
    psr::ActorComponent::Register(reg);
    const psr::EntitySchemaModel model = reg.Model();

    REQUIRE(model.components.size() == 1);

    const psr::ComponentSchema& actor = model.components[0];
    CHECK(actor.id == "actor");
    CHECK(actor.authorable);
    REQUIRE(actor.fields.size() == 3);
    for (const psr::FieldSchema& field : actor.fields)
        CHECK(field.kind == psr::FieldKind::Integer);
    CHECK(actor.fields[0].name == "ap");
    CHECK(actor.fields[1].name == "movement_speed");
    CHECK(actor.fields[2].name == "act_speed");
}

TEST_CASE("JsonEntityLoader round-trips an actor entity's speed stats", "[JsonEntityLoader]")
{
    entt::meta_ctx ctx;
    psr::ComponentSchemaRegistrar reg{ctx};
    psr::ActorComponent::Register(reg);

    TempDirectory temp;
    WriteText(temp.path / "enemies" / "booma.json",
              R"json({
                  "schema_version": 1,
                  "components": {
                      "actor": { "ap": 0, "movement_speed": 200, "act_speed": 50 }
                  }
              })json");

    psr::JsonEntityLoader loader(ctx);
    REQUIRE(loader.Load(temp.path));

    entt::registry prefab_registry;
    std::unordered_map<std::uint32_t, entt::entity> prefab_ids;
    loader.Populate(prefab_registry, prefab_ids);

    auto it = prefab_ids.find(PrefabId("enemies.booma"));
    REQUIRE(it != prefab_ids.end());

    REQUIRE(prefab_registry.all_of<psr::ActorComponent>(it->second));
    const psr::ActorComponent& actor = prefab_registry.get<psr::ActorComponent>(it->second);
    CHECK(actor.ap == 0);
    CHECK(actor.movement_speed == 200);
    CHECK(actor.act_speed == 50);
}
