#include "Engine/Items/AffixLibraryFile.h"

#include "Engine/Items/AffixError.h"
#include "Engine/Items/AffixSchema.h"
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
        path = std::filesystem::temp_directory_path() / "PSORoguelike-AffixSchemaTests" /
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

TEST_CASE("BuildAffixSchemaModel reflects name/kind/stat/amount with the expected field kinds", "[AffixSchema]")
{
    const psr::AffixSchemaModel model = psr::BuildAffixSchemaModel();

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

    const psr::FieldSchema* kind = find("kind");
    REQUIRE(kind != nullptr);
    CHECK(kind->kind == psr::FieldKind::Enum);
    CHECK(kind->enum_values == std::vector<std::string>{"prefix", "suffix"});

    const psr::FieldSchema* stat = find("stat");
    REQUIRE(stat != nullptr);
    CHECK(stat->kind == psr::FieldKind::Enum);
    CHECK(stat->enum_values == std::vector<std::string>{"atp", "ata", "mst", "dfp", "evp", "lck"});

    const psr::FieldSchema* amount = find("amount");
    REQUIRE(amount != nullptr);
    CHECK(amount->kind == psr::FieldKind::Integer);
}

TEST_CASE("SaveAffix + LoadAffixLibrary round-trips name/kind/stat/amount", "[AffixSchema]")
{
    psr::Affix affix;
    affix.name = "Power";
    affix.kind = psr::AffixKind::Prefix;
    affix.stat = psr::AffixStat::Atp;
    affix.amount = 15;

    TempDirectory temp;
    const std::filesystem::path path = temp.path / "power.json";
    psr::SaveAffix(path, affix);

    psr::AffixLibrary library = psr::LoadAffixLibrary(temp.path);
    REQUIRE(library.All().size() == 1);
    const psr::Affix& loaded = library.All().front();

    CHECK(loaded.id_string == "power");
    CHECK(loaded.name == "Power");
    CHECK(loaded.kind == psr::AffixKind::Prefix);
    CHECK(loaded.stat == psr::AffixStat::Atp);
    CHECK(loaded.amount == 15);
}

TEST_CASE("LoadAffixLibrary throws AffixError for an unknown stat name", "[AffixSchema]")
{
    TempDirectory temp;
    WriteText(temp.path / "bad.json",
              R"json({ "schema_version": 1, "name": "Bad", "kind": "prefix", "stat": "xyz", "amount": 1 })json");

    REQUIRE_THROWS_AS(psr::LoadAffixLibrary(temp.path), psr::AffixError);
}

TEST_CASE("LoadAffixLibrary throws JsonFileError for a schema_version mismatch", "[AffixSchema]")
{
    TempDirectory temp;
    WriteText(temp.path / "bad.json", R"json({ "schema_version": 999, "name": "x" })json");

    REQUIRE_THROWS_AS(psr::LoadAffixLibrary(temp.path), psr::JsonFileError);
}
