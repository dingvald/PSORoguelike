#include "Engine/Combat/PhotonArtLibraryFile.h"

#include "Engine/Combat/PhotonArtError.h"
#include "Engine/Combat/PhotonArtSchema.h"
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
        path = std::filesystem::temp_directory_path() / "PSORoguelike-PhotonArtSchemaTests" /
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

TEST_CASE("BuildPhotonArtSchemaModel reflects fields with the expected field kinds", "[PhotonArtSchema]")
{
    const psr::PhotonArtSchemaModel model = psr::BuildPhotonArtSchemaModel();

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

    const psr::FieldSchema* pp_cost = find("pp_cost");
    REQUIRE(pp_cost != nullptr);
    CHECK(pp_cost->kind == psr::FieldKind::Integer);

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

    const psr::FieldSchema* tiers = find("tiers");
    REQUIRE(tiers != nullptr);
    CHECK(tiers->kind == psr::FieldKind::Array);
    REQUIRE(tiers->children.size() == 1);
    CHECK(tiers->children.front().kind == psr::FieldKind::Object);
}

TEST_CASE("SavePhotonArt + LoadPhotonArtLibrary round-trips every field", "[PhotonArtSchema]")
{
    psr::PhotonArt photon_art;
    photon_art.name = "Rising Strike";
    photon_art.pp_cost = 12;
    photon_art.targeting_mode = psr::TargetingMode::Directional;
    photon_art.range_shape = psr::WeaponRangeShape::Cone3;
    photon_art.range = 2;
    photon_art.hits_per_turn = 3;
    photon_art.effect_family = psr::EffectFamily::Drain;
    photon_art.drain_percent = 25;
    photon_art.tiers.push_back(psr::PhotonArtTier{2, 1.5f});

    TempDirectory temp;
    const std::filesystem::path path = temp.path / "rising_strike.json";
    psr::SavePhotonArt(path, photon_art);

    psr::PhotonArtLibrary library = psr::LoadPhotonArtLibrary(temp.path);
    REQUIRE(library.All().size() == 1);
    const psr::PhotonArt& loaded = library.All().front();

    CHECK(loaded.id_string == "rising_strike");
    CHECK(loaded.name == "Rising Strike");
    CHECK(loaded.pp_cost == 12);
    CHECK(loaded.targeting_mode == psr::TargetingMode::Directional);
    CHECK(loaded.range_shape == psr::WeaponRangeShape::Cone3);
    CHECK(loaded.range == 2);
    CHECK(loaded.hits_per_turn == 3);
    CHECK(loaded.effect_family == psr::EffectFamily::Drain);
    CHECK(loaded.drain_percent == 25);
    REQUIRE(loaded.tiers.size() == 1);
    CHECK(loaded.tiers.front().tier == 2);
    CHECK(loaded.tiers.front().power_multiplier == 1.5f);
}

TEST_CASE("LoadPhotonArtLibrary throws PhotonArtError for an unknown targeting mode", "[PhotonArtSchema]")
{
    TempDirectory temp;
    WriteText(temp.path / "bad.json",
              R"json({ "schema_version": 1, "name": "Bad", "targeting_mode": "xyz" })json");

    REQUIRE_THROWS_AS(psr::LoadPhotonArtLibrary(temp.path), psr::PhotonArtError);
}

TEST_CASE("LoadPhotonArtLibrary throws JsonFileError for a schema_version mismatch", "[PhotonArtSchema]")
{
    TempDirectory temp;
    WriteText(temp.path / "bad.json", R"json({ "schema_version": 999, "name": "x" })json");

    REQUIRE_THROWS_AS(psr::LoadPhotonArtLibrary(temp.path), psr::JsonFileError);
}
