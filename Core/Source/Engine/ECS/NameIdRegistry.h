#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace psr {

// A hashed-string id (FieldKind::NameId, see ComponentSchema.h) is a one-way
// hash: entt::hashed_string never retains the original text once only its
// .value() is kept, so nothing downstream of that point can turn the id back
// into a human-readable label. This registry closes that gap by capturing
// the source string the moment something *does* hash it (see
// JsonEntityLoader.cpp's NameId branch) into a small global id -> label map,
// so any later consumer (e.g. an item-detail UI) can look the label back up.
// First registration for a given id wins -- authors always spell the same id
// the same way, so a later differing registration would only happen for an
// accidental hash collision, and keeping the first-seen label is as good a
// tie-break as any.
class NameIdRegistry
{
public:
    static void Register(std::uint32_t hash, std::string_view label)
    {
        std::lock_guard<std::mutex> lock(Mutex());
        Map().try_emplace(hash, label);
    }

    static std::optional<std::string> Find(std::uint32_t hash)
    {
        std::lock_guard<std::mutex> lock(Mutex());
        auto it = Map().find(hash);
        if (it == Map().end())
            return std::nullopt;
        return it->second;
    }

private:
    static std::unordered_map<std::uint32_t, std::string>& Map()
    {
        static std::unordered_map<std::uint32_t, std::string> map;
        return map;
    }

    static std::mutex& Mutex()
    {
        static std::mutex mutex;
        return mutex;
    }
};

} // namespace psr
