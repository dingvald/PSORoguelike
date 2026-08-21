#include "Engine/Persistence/JsonDirectoryLoader.h"

#include "Engine/Persistence/JsonFile.h" // JsonFileError

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <fstream>
#include <string>

namespace {

// A fresh, empty subdirectory per TempDirectory instance, removed on
// destruction -- matches JsonFileTests.cpp's pattern.
struct TempDirectory
{
    std::filesystem::path path;

    TempDirectory()
    {
        static std::atomic<int> counter{ 0 };
        path = std::filesystem::temp_directory_path() / "PSORoguelike-JsonDirectoryLoaderTests" /
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

TEST_CASE("LoadJsonDirectory derives ids from nested relative paths", "[JsonDirectoryLoader]")
{
    TempDirectory temp;
    WriteText(temp.path / "terrain" / "floor.json", R"json({ "schema_version": 1, "value": 1 })json");
    WriteText(temp.path / "flora" / "grass.json", R"json({ "schema_version": 1, "value": 2 })json");
    WriteText(temp.path / "actors" / "hostile" / "goblin.json", R"json({ "schema_version": 1, "value": 3 })json");

    const std::vector<psr::JsonDirectoryEntry> entries = psr::LoadJsonDirectory(temp.path, 1);

    REQUIRE(entries.size() == 3);
    // Sorted by id, deterministic regardless of filesystem iteration order.
    CHECK(entries[0].id == "actors.hostile.goblin");
    CHECK(entries[1].id == "flora.grass");
    CHECK(entries[2].id == "terrain.floor");
    CHECK(entries[0].document["value"].GetInt() == 3);
}

TEST_CASE("LoadJsonDirectory only matches files ending with the given suffix", "[JsonDirectoryLoader]")
{
    TempDirectory temp;
    WriteText(temp.path / "world.map.json", R"json({ "schema_version": 1 })json");
    WriteText(temp.path / "world_map_select.json", R"json({ "schema_version": 1 })json");

    const std::vector<psr::JsonDirectoryEntry> entries = psr::LoadJsonDirectory(temp.path, 1, ".map.json");

    REQUIRE(entries.size() == 1);
    CHECK(entries[0].id == "world");
}

TEST_CASE("LoadJsonDirectory throws JsonFileError when the directory doesn't exist", "[JsonDirectoryLoader]")
{
    TempDirectory temp;
    REQUIRE_THROWS_AS(psr::LoadJsonDirectory(temp.path / "missing", 1), psr::JsonFileError);
}

TEST_CASE(
    "LoadJsonDirectory throws JsonFileError on a schema_version mismatch in any fragment", "[JsonDirectoryLoader]")
{
    TempDirectory temp;
    WriteText(temp.path / "ok.json", R"json({ "schema_version": 1 })json");
    WriteText(temp.path / "stale.json", R"json({ "schema_version": 2 })json");

    REQUIRE_THROWS_AS(psr::LoadJsonDirectory(temp.path, 1), psr::JsonFileError);
}

TEST_CASE("LoadJsonDirectory returns an empty list for a directory with no matching files", "[JsonDirectoryLoader]")
{
    TempDirectory temp;
    REQUIRE(psr::LoadJsonDirectory(temp.path, 1).empty());
}
