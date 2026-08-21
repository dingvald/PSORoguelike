#include "Engine/Persistence/JsonDirectoryLoader.h"

#include "Engine/Persistence/JsonFile.h"

#include <algorithm>

namespace psr {

namespace {

    bool HasSuffix(const std::string& text, const std::string& suffix)
    {
        return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    // The path relative to directory, '/'-joined regardless of platform, with
    // filename_suffix stripped -- e.g. "terrain/floor.json" -> "terrain/floor".
    std::string RelativeStem(const std::filesystem::path& path, const std::filesystem::path& directory,
                             const std::string& filename_suffix)
    {
        std::string relative = std::filesystem::relative(path, directory).generic_string();
        return relative.substr(0, relative.size() - filename_suffix.size());
    }

} // namespace

std::vector<JsonDirectoryEntry> LoadJsonDirectory(const std::filesystem::path& directory, int expected_schema_version,
                                                  const std::string& filename_suffix)
{
    if (!std::filesystem::is_directory(directory))
        throw JsonFileError("JsonDirectoryLoader: no such directory " + directory.string());

    std::vector<JsonDirectoryEntry> entries;
    for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(directory))
    {
        if (!entry.is_regular_file())
            continue;
        if (!HasSuffix(entry.path().filename().string(), filename_suffix))
            continue;

        JsonDirectoryEntry item;
        item.path = entry.path();
        item.document = ReadJsonFile(item.path, expected_schema_version);

        std::string stem = RelativeStem(item.path, directory, filename_suffix);
        std::replace(stem.begin(), stem.end(), '/', '.');
        item.id = std::move(stem);

        entries.push_back(std::move(item));
    }

    std::sort(entries.begin(), entries.end(),
              [](const JsonDirectoryEntry& a, const JsonDirectoryEntry& b) { return a.id < b.id; });

    return entries;
}

} // namespace psr
