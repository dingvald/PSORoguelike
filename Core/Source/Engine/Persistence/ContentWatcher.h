#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

namespace psr {

// Polling directory-mtime watcher, sibling to LoadJsonDirectory (mirrors its
// recursive scan + filename_suffix filter, so a caller pairs one
// ContentWatcher with the same directory it LoadJsonDirectory()s). Detects
// changes by re-scanning and diffing last_write_time against a snapshot --
// no OS file-watch API, no background thread; cheap enough to call once per
// frame/tick from a dev-build main loop.
class ContentWatcher
{
public:
    // Snapshots directory's current state immediately, so the first Poll()
    // only reports files changed after construction. directory need not
    // exist yet -- treated as an empty snapshot, so Poll() reports true once
    // it's created and populated later (a fresh dev checkout may not have
    // the content directory yet).
    explicit ContentWatcher(std::filesystem::path directory, std::string filename_suffix = ".json");

    // Re-scans directory and compares against the last snapshot. Returns
    // true (and updates the snapshot) iff any matching file was added,
    // removed, or has a newer last_write_time than last observed.
    bool Poll();

private:
    std::filesystem::path m_directory;
    std::string m_filename_suffix;
    std::unordered_map<std::string, std::filesystem::file_time_type> m_snapshot;
};

} // namespace psr
