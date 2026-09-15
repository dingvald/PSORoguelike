#include "Progression/ClassDefinitionFile.h"

#include "Engine/Persistence/JsonFile.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <filesystem>

namespace {

// A fresh, empty subdirectory per TempDirectory instance, removed on
// destruction -- same pattern HubDefinitionFileTests.cpp uses.
struct TempDirectory
{
    std::filesystem::path path;

    TempDirectory()
    {
        static std::atomic<int> counter{0};
        path = std::filesystem::temp_directory_path() / "PSORoguelike-ClassDefinitionFileTests" /
               ("run-" + std::to_string(counter++));
        std::filesystem::create_directories(path);
    }

    ~TempDirectory()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

rapidjson::Value StringValue(const char* text, rapidjson::Document::AllocatorType& allocator)
{
    rapidjson::Value value;
    value.SetString(text, allocator);
    return value;
}

// Builds a minimal-but-valid class document (one level, no starting
// techniques) that callers mutate/extend per test case.
rapidjson::Document BuildValidDocument(const char* class_id)
{
    rapidjson::Document document;
    document.SetObject();
    auto& allocator = document.GetAllocator();

    document.AddMember("schema_version", psr::kClassDefinitionVersion, allocator);
    document.AddMember("class_id", StringValue(class_id, allocator), allocator);
    document.AddMember("name", StringValue("Hunter", allocator), allocator);
    document.AddMember("starting_weapon_prefab_id", StringValue("weapons.saber", allocator), allocator);
    document.AddMember("base_hp", 50, allocator);
    document.AddMember("base_tp", 15, allocator);

    auto stat_curve = [&](int base)
    {
        rapidjson::Value curve(rapidjson::kObjectType);
        curve.AddMember("base", base, allocator);
        return curve;
    };

    rapidjson::Value curves(rapidjson::kObjectType);
    curves.AddMember("xp_to_next", stat_curve(50), allocator);
    curves.AddMember("max_hp", stat_curve(48), allocator);
    curves.AddMember("max_tp", stat_curve(24), allocator);
    curves.AddMember("atp", stat_curve(48), allocator);
    curves.AddMember("ata", stat_curve(70), allocator);
    curves.AddMember("mst", stat_curve(31), allocator);
    curves.AddMember("dfp", stat_curve(19), allocator);
    curves.AddMember("evp", stat_curve(46), allocator);
    curves.AddMember("lck", stat_curve(11), allocator);
    document.AddMember("curves", curves, allocator);

    return document;
}

} // namespace

TEST_CASE("LoadClassDefinition loads a valid document", "[ClassDefinitionFile]")
{
    TempDirectory temp;
    std::filesystem::path file = temp.path / "hunter.json";
    psr::WriteJsonFile(file, BuildValidDocument("hunter"));

    const psr::ClassDefinition definition = psr::LoadClassDefinition(file, psr::ClassId::Hunter);
    REQUIRE(definition.class_id == psr::ClassId::Hunter);
    REQUIRE(definition.name == "Hunter");
    REQUIRE(definition.starting_weapon_prefab_id == "weapons.saber");
    REQUIRE(definition.starting_technique_id_strings.empty());
    REQUIRE(definition.starting_armor_prefab_id.empty());
    REQUIRE(definition.starting_inventory.empty());
    REQUIRE(definition.base_hp == 50);
    REQUIRE(definition.base_tp == 15);

    REQUIRE(definition.growth.Evaluate(2).max_hp == 48);
}

TEST_CASE("LoadClassDefinition reads starting_technique_id_strings when present", "[ClassDefinitionFile]")
{
    TempDirectory temp;
    std::filesystem::path file = temp.path / "force.json";

    rapidjson::Document document = BuildValidDocument("force");
    auto& allocator = document.GetAllocator();
    document["class_id"] = StringValue("force", allocator);

    rapidjson::Value techniques(rapidjson::kArrayType);
    techniques.PushBack(StringValue("foie", allocator), allocator);
    document.AddMember("starting_technique_id_strings", techniques, allocator);

    psr::WriteJsonFile(file, document);

    const psr::ClassDefinition definition = psr::LoadClassDefinition(file, psr::ClassId::Force);
    REQUIRE(definition.starting_technique_id_strings == std::vector<std::string>{"foie"});
}

TEST_CASE("LoadClassDefinition reads starting_armor_prefab_id and starting_inventory when present",
          "[ClassDefinitionFile]")
{
    TempDirectory temp;
    std::filesystem::path file = temp.path / "force.json";

    rapidjson::Document document = BuildValidDocument("force");
    auto& allocator = document.GetAllocator();
    document["class_id"] = StringValue("force", allocator);
    document.AddMember("starting_armor_prefab_id", StringValue("armor.frame", allocator), allocator);

    rapidjson::Value inventory(rapidjson::kArrayType);
    rapidjson::Value monofluid(rapidjson::kObjectType);
    monofluid.AddMember("item_prefab_id", StringValue("monofluid", allocator), allocator);
    monofluid.AddMember("quantity", 3, allocator);
    inventory.PushBack(monofluid, allocator);
    rapidjson::Value disk(rapidjson::kObjectType);
    disk.AddMember("item_prefab_id", StringValue("TechniqueDisks.resta_disk", allocator), allocator);
    inventory.PushBack(disk, allocator);
    document.AddMember("starting_inventory", inventory, allocator);

    psr::WriteJsonFile(file, document);

    const psr::ClassDefinition definition = psr::LoadClassDefinition(file, psr::ClassId::Force);
    REQUIRE(definition.starting_armor_prefab_id == "armor.frame");
    REQUIRE(definition.starting_inventory.size() == 2);
    REQUIRE(definition.starting_inventory[0].item_prefab_id == "monofluid");
    REQUIRE(definition.starting_inventory[0].quantity == 3);
    REQUIRE(definition.starting_inventory[1].item_prefab_id == "TechniqueDisks.resta_disk");
    REQUIRE(definition.starting_inventory[1].quantity == 1); // defaults when 'quantity' is omitted
}

TEST_CASE("LoadClassDefinition throws when a starting_inventory entry has no item_prefab_id",
          "[ClassDefinitionFile]")
{
    TempDirectory temp;
    std::filesystem::path file = temp.path / "hunter.json";

    rapidjson::Document document = BuildValidDocument("hunter");
    auto& allocator = document.GetAllocator();
    rapidjson::Value inventory(rapidjson::kArrayType);
    rapidjson::Value entry(rapidjson::kObjectType);
    entry.AddMember("quantity", 2, allocator);
    inventory.PushBack(entry, allocator);
    document.AddMember("starting_inventory", inventory, allocator);

    psr::WriteJsonFile(file, document);

    REQUIRE_THROWS_AS(psr::LoadClassDefinition(file, psr::ClassId::Hunter), psr::JsonFileError);
}

TEST_CASE("LoadClassDefinition throws on a missing file", "[ClassDefinitionFile]")
{
    TempDirectory temp;
    REQUIRE_THROWS_AS(psr::LoadClassDefinition(temp.path / "missing.json", psr::ClassId::Hunter), psr::JsonFileError);
}

TEST_CASE("LoadClassDefinition throws on a schema_version mismatch", "[ClassDefinitionFile]")
{
    TempDirectory temp;
    std::filesystem::path file = temp.path / "hunter.json";

    rapidjson::Document document = BuildValidDocument("hunter");
    document["schema_version"] = psr::kClassDefinitionVersion + 1;
    psr::WriteJsonFile(file, document);

    REQUIRE_THROWS_AS(psr::LoadClassDefinition(file, psr::ClassId::Hunter), psr::JsonFileError);
}

TEST_CASE("LoadClassDefinition throws when class_id does not match the expected class", "[ClassDefinitionFile]")
{
    TempDirectory temp;
    std::filesystem::path file = temp.path / "hunter.json";
    psr::WriteJsonFile(file, BuildValidDocument("hunter"));

    REQUIRE_THROWS_AS(psr::LoadClassDefinition(file, psr::ClassId::Ranger), psr::JsonFileError);
}

TEST_CASE("LoadClassDefinition throws when class_id is missing", "[ClassDefinitionFile]")
{
    TempDirectory temp;
    std::filesystem::path file = temp.path / "hunter.json";

    rapidjson::Document document = BuildValidDocument("hunter");
    document.RemoveMember("class_id");
    psr::WriteJsonFile(file, document);

    REQUIRE_THROWS_AS(psr::LoadClassDefinition(file, psr::ClassId::Hunter), psr::JsonFileError);
}

TEST_CASE("LoadClassDefinition throws when curves is missing", "[ClassDefinitionFile]")
{
    TempDirectory temp;
    std::filesystem::path file = temp.path / "hunter.json";

    rapidjson::Document document = BuildValidDocument("hunter");
    document.RemoveMember("curves");
    psr::WriteJsonFile(file, document);

    REQUIRE_THROWS_AS(psr::LoadClassDefinition(file, psr::ClassId::Hunter), psr::JsonFileError);
}
