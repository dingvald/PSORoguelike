#include "Engine/Persistence/JsonFile.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>

namespace {

// A fresh, empty subdirectory per TempDirectory instance, removed on
// destruction -- real file I/O, matching RegistryTests.cpp's peers.
struct TempDirectory
{
    std::filesystem::path path;

    TempDirectory()
    {
        static std::atomic<int> counter{ 0 };
        path = std::filesystem::temp_directory_path() / "PSORoguelike-JsonFileTests" / ("run-" + std::to_string(counter++));
        std::filesystem::create_directories(path);
    }

    ~TempDirectory()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

} // namespace

TEST_CASE("JsonFile round-trips a document through write then read", "[JsonFile]")
{
    TempDirectory temp;
    std::filesystem::path file = temp.path / "nested" / "data.json";

    rapidjson::Document document;
    document.SetObject();
    document.AddMember("schema_version", 1, document.GetAllocator());
    document.AddMember("value", 42, document.GetAllocator());

    psr::WriteJsonFile(file, document);
    REQUIRE(std::filesystem::exists(file));

    rapidjson::Document loaded = psr::ReadJsonFile(file, /*expected_schema_version=*/1);
    REQUIRE(loaded["value"].GetInt() == 42);
}

TEST_CASE("JsonFile::ReadJsonFile throws on a missing file", "[JsonFile]")
{
    TempDirectory temp;
    REQUIRE_THROWS_AS(psr::ReadJsonFile(temp.path / "missing.json", 1), psr::JsonFileError);
}

TEST_CASE("JsonFile::ReadJsonFile throws on malformed JSON", "[JsonFile]")
{
    TempDirectory temp;
    std::filesystem::path file = temp.path / "malformed.json";
    {
        std::ofstream stream(file);
        stream << "{ not valid json";
    }

    REQUIRE_THROWS_AS(psr::ReadJsonFile(file, 1), psr::JsonFileError);
}

TEST_CASE("JsonFile::ReadJsonFile throws on a schema_version mismatch", "[JsonFile]")
{
    TempDirectory temp;
    std::filesystem::path file = temp.path / "versioned.json";

    rapidjson::Document document;
    document.SetObject();
    document.AddMember("schema_version", 1, document.GetAllocator());
    psr::WriteJsonFile(file, document);

    REQUIRE_THROWS_AS(psr::ReadJsonFile(file, /*expected_schema_version=*/2), psr::JsonFileError);
}
