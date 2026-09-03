#include "Progression/GrowthCurveFile.h"

#include "Engine/Persistence/JsonFile.h"

#include <algorithm>
#include <string>

namespace psr {

namespace {

    int ReadInt(const rapidjson::Value& object, const char* key, int fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsInt())
            throw JsonFileError(std::string("growth curve file: '") + key + "' must be an integer");
        return it->value.GetInt();
    }

    GrowthCurveLevel ReadGrowthCurveLevel(const rapidjson::Value& entry)
    {
        if (!entry.IsObject())
            throw JsonFileError("growth curve file: each level entry must be an object");

        GrowthCurveLevel level;
        level.level = ReadInt(entry, "level", level.level);
        level.xp_to_next = ReadInt(entry, "xp_to_next", level.xp_to_next);
        level.max_hp = ReadInt(entry, "max_hp", level.max_hp);
        level.max_tp = ReadInt(entry, "max_tp", level.max_tp);
        level.stats.atp = ReadInt(entry, "atp", level.stats.atp);
        level.stats.ata = ReadInt(entry, "ata", level.stats.ata);
        level.stats.mst = ReadInt(entry, "mst", level.stats.mst);
        level.stats.dfp = ReadInt(entry, "dfp", level.stats.dfp);
        level.stats.evp = ReadInt(entry, "evp", level.stats.evp);
        level.stats.lck = ReadInt(entry, "lck", level.stats.lck);

        if (level.level < 2)
            throw JsonFileError("growth curve file: each level entry's 'level' must be 2 or greater");

        return level;
    }

} // namespace

GrowthCurve LoadGrowthCurve(const std::filesystem::path& path)
{
    const rapidjson::Document document = ReadJsonFile(path, kGrowthCurveVersion);
    if (!document.IsObject())
        throw JsonFileError("growth curve file: must be an object");

    auto levels_member = document.FindMember("levels");
    if (levels_member == document.MemberEnd() || !levels_member->value.IsArray())
        throw JsonFileError("growth curve file: 'levels' must be an array");

    GrowthCurve curve;
    for (const rapidjson::Value& entry : levels_member->value.GetArray())
        curve.levels.push_back(ReadGrowthCurveLevel(entry));

    std::sort(curve.levels.begin(), curve.levels.end(),
              [](const GrowthCurveLevel& a, const GrowthCurveLevel& b) { return a.level < b.level; });

    return curve;
}

} // namespace psr
