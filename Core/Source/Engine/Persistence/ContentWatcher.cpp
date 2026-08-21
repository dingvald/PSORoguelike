#include "Engine/Persistence/ContentWatcher.h"

namespace psr {

namespace {

    bool HasSuffix(const std::string& text, const std::string& suffix)
    {
        return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

} // namespace

ContentWatcher::ContentWatcher(std::filesystem::path directory, std::string filename_suffix)
    : m_directory(std::move(directory)), m_filename_suffix(std::move(filename_suffix))
{
    Poll();
}

bool ContentWatcher::Poll()
{
    std::unordered_map<std::string, std::filesystem::file_time_type> current;

    if (std::filesystem::is_directory(m_directory))
    {
        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(m_directory))
        {
            if (!entry.is_regular_file())
                continue;
            if (!HasSuffix(entry.path().filename().string(), m_filename_suffix))
                continue;

            current.emplace(entry.path().string(), entry.last_write_time());
        }
    }

    bool changed = current.size() != m_snapshot.size();
    if (!changed)
    {
        for (const auto& [path, write_time] : current)
        {
            auto previous = m_snapshot.find(path);
            if (previous == m_snapshot.end() || previous->second != write_time)
            {
                changed = true;
                break;
            }
        }
    }

    m_snapshot = std::move(current);
    return changed;
}

} // namespace psr
