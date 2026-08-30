#include "Combat/TechniqueLibraryFile.h"

#include "Combat/TechniqueError.h"
#include "Combat/TechniqueSchema.h"
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
        path = std::filesystem::temp_directory_path() / "PSORoguelike-TechniqueSchemaTests" /
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

TEST_CASE("BuildTechniqueSchemaModel reflects fields with the expected field kinds", "[TechniqueSchema]")
{
    const psr::TechniqueSchemaModel model = psr::BuildTechniqueSchemaModel();

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

    const psr::FieldSchema* tp_cost = find("tp_cost");
    REQUIRE(tp_cost != nullptr);
    CHECK(tp_cost->kind == psr::FieldKind::Integer);

    const psr::FieldSchema* element = find("element");
    REQUIRE(element != nullptr);
    CHECK(element->kind == psr::FieldKind::Enum);
    CHECK(element->enum_values == std::vector<std::string>{"none", "fire", "ice", "lightning", "light", "dark"});

    const psr::FieldSchema* targeting_mode = find("targeting_mode");
    REQUIRE(targeting_mode != nullptr);
    CHECK(targeting_mode->kind == psr::FieldKind::Enum);
    CHECK(targeting_mode->enum_values == std::vector<std::string>{"directional", "target_square", "self_target"});

    const psr::FieldSchema* range_shape = find("range_shape");
    REQUIRE(range_shape != nullptr);
    CHECK(range_shape->kind == psr::FieldKind::Enum);
    CHECK(range_shape->enum_values == std::vector<std::string>{"single_target", "cone_3", "surrounding", "line"});

    const psr::FieldSchema* effect_family = find("effect_family");
    REQUIRE(effect_family != nullptr);
    CHECK(effect_family->kind == psr::FieldKind::Enum);
    CHECK(effect_family->enum_values == std::vector<std::string>{"damage", "drain", "status"});

    const psr::FieldSchema* status_effect_id = find("status_effect_id");
    REQUIRE(status_effect_id != nullptr);
    CHECK(status_effect_id->kind == psr::FieldKind::NameId);

    const psr::FieldSchema* status_chance_percent = find("status_chance_percent");
    REQUIRE(status_chance_percent != nullptr);
    CHECK(status_chance_percent->kind == psr::FieldKind::Integer);

    const psr::FieldSchema* tiers = find("tiers");
    REQUIRE(tiers != nullptr);
    CHECK(tiers->kind == psr::FieldKind::Array);
    REQUIRE(tiers->children.size() == 1);
    CHECK(tiers->children.front().kind == psr::FieldKind::Object);
}

TEST_CASE("SaveTechnique + LoadTechniqueLibrary round-trips every field", "[TechniqueSchema]")
{
    psr::Technique technique;
    technique.name = "Foie";
    technique.tp_cost = 8;
    technique.element = psr::Element::Fire;
    technique.targeting_mode = psr::TargetingMode::TargetSquare;
    technique.range_shape = psr::WeaponRangeShape::SingleTarget;
    technique.range = 5;
    technique.effect_family = psr::EffectFamily::Damage;
    technique.status_chance_percent = 25;
    technique.tiers.push_back(psr::TechniqueTier{3, 2.0f});

    TempDirectory temp;
    const std::filesystem::path path = temp.path / "foie.json";
    psr::SaveTechnique(path, technique);

    psr::TechniqueLibrary library = psr::LoadTechniqueLibrary(temp.path);
    REQUIRE(library.All().size() == 1);
    const psr::Technique& loaded = library.All().front();

    CHECK(loaded.id_string == "foie");
    CHECK(loaded.name == "Foie");
    CHECK(loaded.tp_cost == 8);
    CHECK(loaded.element == psr::Element::Fire);
    CHECK(loaded.targeting_mode == psr::TargetingMode::TargetSquare);
    CHECK(loaded.range_shape == psr::WeaponRangeShape::SingleTarget);
    CHECK(loaded.range == 5);
    CHECK(loaded.effect_family == psr::EffectFamily::Damage);
    CHECK(loaded.status_chance_percent == 25);
    REQUIRE(loaded.tiers.size() == 1);
    CHECK(loaded.tiers.front().tier == 3);
    CHECK(loaded.tiers.front().power_multiplier == 2.0f);
}

TEST_CASE("LoadTechniqueLibrary throws TechniqueError for an unknown range shape", "[TechniqueSchema]")
{
    TempDirectory temp;
    WriteText(temp.path / "bad.json", R"json({ "schema_version": 1, "name": "Bad", "range_shape": "xyz" })json");

    REQUIRE_THROWS_AS(psr::LoadTechniqueLibrary(temp.path), psr::TechniqueError);
}

TEST_CASE("LoadTechniqueLibrary throws JsonFileError for a schema_version mismatch", "[TechniqueSchema]")
{
    TempDirectory temp;
    WriteText(temp.path / "bad.json", R"json({ "schema_version": 999, "name": "x" })json");

    REQUIRE_THROWS_AS(psr::LoadTechniqueLibrary(temp.path), psr::JsonFileError);
}
