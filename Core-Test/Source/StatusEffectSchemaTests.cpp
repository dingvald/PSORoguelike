#include "Engine/Combat/StatusEffectLibraryFile.h"

#include "Engine/Combat/StatusEffectError.h"
#include "Engine/Combat/StatusEffectSchema.h"
#include "Engine/Persistence/JsonFile.h" // JsonFileError

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <fstream>
#include <string>

namespace {

struct TempDirectory
{
    std::filesystem::path path;

    TempDirectory()
    {
        static std::atomic<int> counter{0};
        path = std::filesystem::temp_directory_path() / "PSORoguelike-StatusEffectSchemaTests" /
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

} // namespace

TEST_CASE("BuildStatusEffectSchemaModel reflects name/type/magnitude/duration with the expected field kinds",
          "[StatusEffectSchema]")
{
    const psr::StatusEffectSchemaModel model = psr::BuildStatusEffectSchemaModel();

    auto find = [&](const std::string& name) -> const psr::FieldSchema*
    {
        for (const psr::FieldSchema& field : model.fields)
            if (field.name == name)
                return &field;
        return nullptr;
    };

    const psr::FieldSchema* name = find("name");
    REQUIRE(name != nullptr);
    CHECK(name->kind == psr::FieldKind::String);

    const psr::FieldSchema* type = find("type");
    REQUIRE(type != nullptr);
    CHECK(type->kind == psr::FieldKind::Enum);
    CHECK(type->enum_values == std::vector<std::string>{"poison", "burn", "freeze", "shock", "confuse"});

    const psr::FieldSchema* magnitude = find("magnitude");
    REQUIRE(magnitude != nullptr);
    CHECK(magnitude->kind == psr::FieldKind::Integer);

    const psr::FieldSchema* duration = find("duration");
    REQUIRE(duration != nullptr);
    CHECK(duration->kind == psr::FieldKind::Integer);
}

TEST_CASE("SaveStatusEffect + LoadStatusEffectLibrary round-trips name/type/magnitude/duration",
          "[StatusEffectSchema]")
{
    psr::StatusEffect status_effect;
    status_effect.name = "Weak Poison";
    status_effect.type = psr::StatusEffectType::Poison;
    status_effect.magnitude = 2;
    status_effect.duration = 5;

    TempDirectory temp;
    const std::filesystem::path path = temp.path / "weak_poison.json";
    psr::SaveStatusEffect(path, status_effect);

    psr::StatusEffectLibrary library = psr::LoadStatusEffectLibrary(temp.path);
    REQUIRE(library.All().size() == 1);
    const psr::StatusEffect& loaded = library.All().front();

    CHECK(loaded.id_string == "weak_poison");
    CHECK(loaded.name == "Weak Poison");
    CHECK(loaded.type == psr::StatusEffectType::Poison);
    CHECK(loaded.magnitude == 2);
    CHECK(loaded.duration == 5);
}

TEST_CASE("LoadStatusEffectLibrary throws StatusEffectError for an unknown type name", "[StatusEffectSchema]")
{
    TempDirectory temp;
    WriteText(temp.path / "bad.json",
              R"json({ "schema_version": 1, "name": "Bad", "type": "xyz", "magnitude": 1, "duration": 1 })json");

    REQUIRE_THROWS_AS(psr::LoadStatusEffectLibrary(temp.path), psr::StatusEffectError);
}

TEST_CASE("LoadStatusEffectLibrary throws JsonFileError for a schema_version mismatch", "[StatusEffectSchema]")
{
    TempDirectory temp;
    WriteText(temp.path / "bad.json", R"json({ "schema_version": 999, "name": "x" })json");

    REQUIRE_THROWS_AS(psr::LoadStatusEffectLibrary(temp.path), psr::JsonFileError);
}
