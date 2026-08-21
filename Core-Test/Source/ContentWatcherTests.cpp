#include "Engine/Persistence/ContentWatcher.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <fstream>
#include <string>
#include <thread>

namespace {

// A fresh, empty subdirectory per TempDirectory instance, removed on
// destruction -- matches JsonDirectoryLoaderTests.cpp's pattern.
struct TempDirectory
{
    std::filesystem::path path;

    TempDirectory()
    {
        static std::atomic<int> counter{ 0 };
        path = std::filesystem::temp_directory_path() / "PSORoguelike-ContentWatcherTests" /
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

// Directory-entry mtime resolution can be coarser than a tight test loop's
// wall-clock delta on some filesystems -- sleeping past it keeps the
// "modified file has a newer last_write_time" cases deterministic.
void SleepPastMtimeResolution()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

} // namespace

TEST_CASE("ContentWatcher Poll returns false when nothing has changed", "[ContentWatcher]")
{
    TempDirectory temp;
    WriteText(temp.path / "terrain.json", R"json({ "value": 1 })json");

    psr::ContentWatcher watcher(temp.path);

    REQUIRE_FALSE(watcher.Poll());
}

TEST_CASE("ContentWatcher Poll detects a modified file's contents", "[ContentWatcher]")
{
    TempDirectory temp;
    WriteText(temp.path / "terrain.json", R"json({ "value": 1 })json");

    psr::ContentWatcher watcher(temp.path);

    SleepPastMtimeResolution();
    WriteText(temp.path / "terrain.json", R"json({ "value": 2 })json");

    REQUIRE(watcher.Poll());
    REQUIRE_FALSE(watcher.Poll());
}

TEST_CASE("ContentWatcher Poll detects a newly added matching file", "[ContentWatcher]")
{
    TempDirectory temp;
    psr::ContentWatcher watcher(temp.path);

    WriteText(temp.path / "terrain.json", R"json({ "value": 1 })json");

    REQUIRE(watcher.Poll());
}

TEST_CASE("ContentWatcher Poll ignores files that don't match filename_suffix", "[ContentWatcher]")
{
    TempDirectory temp;
    psr::ContentWatcher watcher(temp.path);

    WriteText(temp.path / "notes.txt", "not content");

    REQUIRE_FALSE(watcher.Poll());
}

TEST_CASE("ContentWatcher Poll detects a removed file", "[ContentWatcher]")
{
    TempDirectory temp;
    WriteText(temp.path / "terrain.json", R"json({ "value": 1 })json");

    psr::ContentWatcher watcher(temp.path);

    std::filesystem::remove(temp.path / "terrain.json");

    REQUIRE(watcher.Poll());
}

TEST_CASE("ContentWatcher tolerates a directory that doesn't exist yet", "[ContentWatcher]")
{
    TempDirectory temp;
    std::filesystem::path missing = temp.path / "not-created-yet";

    psr::ContentWatcher watcher(missing);
    REQUIRE_FALSE(watcher.Poll());

    std::filesystem::create_directories(missing);
    WriteText(missing / "terrain.json", R"json({ "value": 1 })json");

    REQUIRE(watcher.Poll());
}

TEST_CASE("ContentWatcher scans nested subdirectories recursively", "[ContentWatcher]")
{
    TempDirectory temp;
    WriteText(temp.path / "actors" / "hostile" / "goblin.json", R"json({ "value": 1 })json");

    psr::ContentWatcher watcher(temp.path);
    REQUIRE_FALSE(watcher.Poll());

    SleepPastMtimeResolution();
    WriteText(temp.path / "actors" / "hostile" / "goblin.json", R"json({ "value": 2 })json");

    REQUIRE(watcher.Poll());
}
