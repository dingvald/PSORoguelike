#include "Hub/HubDefinitionFile.h"

#include "Engine/Persistence/JsonFile.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>

namespace {

// A fresh, empty subdirectory per TempDirectory instance, removed on
// destruction -- same pattern JsonFileTests.cpp/DungeonSchemaTests.cpp use.
struct TempDirectory
{
    std::filesystem::path path;

    TempDirectory()
    {
        static std::atomic<int> counter{0};
        path = std::filesystem::temp_directory_path() / "PSORoguelike-HubDefinitionFileTests" /
               ("run-" + std::to_string(counter++));
        std::filesystem::create_directories(path);
    }

    ~TempDirectory()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

} // namespace

TEST_CASE("LoadHubDefinition loads a valid document", "[HubDefinitionFile]")
{
    TempDirectory temp;
    std::filesystem::path file = temp.path / "hub.json";

    rapidjson::Document document;
    document.SetObject();
    document.AddMember("schema_version", psr::kHubDefinitionVersion, document.GetAllocator());
    rapidjson::Value piece_id;
    piece_id.SetString("hub_main");
    document.AddMember("piece_id_string", piece_id, document.GetAllocator());
    psr::WriteJsonFile(file, document);

    const psr::HubDefinition hub = psr::LoadHubDefinition(file);
    REQUIRE(hub.piece_id_string == "hub_main");
}

TEST_CASE("LoadHubDefinition throws on a missing file", "[HubDefinitionFile]")
{
    TempDirectory temp;
    REQUIRE_THROWS_AS(psr::LoadHubDefinition(temp.path / "missing.json"), psr::JsonFileError);
}

TEST_CASE("LoadHubDefinition throws on a schema_version mismatch", "[HubDefinitionFile]")
{
    TempDirectory temp;
    std::filesystem::path file = temp.path / "hub.json";

    rapidjson::Document document;
    document.SetObject();
    document.AddMember("schema_version", psr::kHubDefinitionVersion + 1, document.GetAllocator());
    psr::WriteJsonFile(file, document);

    REQUIRE_THROWS_AS(psr::LoadHubDefinition(file), psr::JsonFileError);
}

TEST_CASE("LoadHubDefinition throws when piece_id_string is missing", "[HubDefinitionFile]")
{
    TempDirectory temp;
    std::filesystem::path file = temp.path / "hub.json";

    rapidjson::Document document;
    document.SetObject();
    document.AddMember("schema_version", psr::kHubDefinitionVersion, document.GetAllocator());
    psr::WriteJsonFile(file, document);

    REQUIRE_THROWS_AS(psr::LoadHubDefinition(file), psr::JsonFileError);
}
