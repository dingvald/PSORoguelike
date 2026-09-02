#include "Combat/LevelingConfig.h"

#include "Engine/Persistence/JsonFile.h"

namespace psr {

namespace {

    constexpr int kLevelingConfigVersion = 1;

    int ReadInt(const rapidjson::Value& object, const char* key, int fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsInt())
            throw JsonFileError(std::string("leveling config: '") + key + "' must be an integer");
        return it->value.GetInt();
    }

    float ReadFloat(const rapidjson::Value& object, const char* key, float fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsNumber())
            throw JsonFileError(std::string("leveling config: '") + key + "' must be a number");
        return it->value.GetFloat();
    }

    StatsComponent ReadStatGrowth(const rapidjson::Value& object, const char* key)
    {
        StatsComponent growth;
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return growth;
        if (!it->value.IsObject())
            throw JsonFileError(std::string("leveling config: '") + key + "' must be an object");

        const rapidjson::Value& growth_object = it->value;
        growth.atp = ReadInt(growth_object, "atp", 0);
        growth.ata = ReadInt(growth_object, "ata", 0);
        growth.mst = ReadInt(growth_object, "mst", 0);
        growth.dfp = ReadInt(growth_object, "dfp", 0);
        growth.evp = ReadInt(growth_object, "evp", 0);
        growth.lck = ReadInt(growth_object, "lck", 0);
        return growth;
    }

} // namespace

LevelingConfig LoadLevelingConfig(const std::filesystem::path& path)
{
    const rapidjson::Document document = ReadJsonFile(path, kLevelingConfigVersion);
    if (!document.IsObject())
        throw JsonFileError("leveling config: must be an object");

    LevelingConfig config;
    config.exp_base = ReadInt(document, "exp_base", config.exp_base);
    config.exp_growth_exponent = ReadFloat(document, "exp_growth_exponent", config.exp_growth_exponent);
    config.stat_growth_per_level = ReadStatGrowth(document, "stat_growth_per_level");
    return config;
}

} // namespace psr
