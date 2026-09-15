#include "Progression/GrowthCurve.h"

#include "Engine/Persistence/JsonFile.h"

#include <cmath>
#include <random>
#include <string>

namespace psr {

namespace {

    constexpr float kGrowthVariance = 0.25f; // +/-25%, uniform, mean == the authored rate

    int EvaluateStat(const StatCurve& curve, int level)
    {
        const float n = static_cast<float>(level - 2);
        const float value = curve.base + curve.rate * std::pow(n, curve.exponent);
        return static_cast<int>(std::lround(value));
    }

    int RoundedRate(const StatCurve& curve)
    {
        return static_cast<int>(std::lround(curve.rate));
    }

    int Jitter(int deterministic_gain, std::mt19937& rng)
    {
        std::uniform_real_distribution<float> multiplier(1.0f - kGrowthVariance, 1.0f + kGrowthVariance);
        return static_cast<int>(std::lround(static_cast<float>(deterministic_gain) * multiplier(rng)));
    }

    float ReadFloat(const rapidjson::Value& object, const char* key, float fallback)
    {
        auto it = object.FindMember(key);
        if (it == object.MemberEnd())
            return fallback;
        if (!it->value.IsNumber())
            throw JsonFileError(std::string("growth curve: '") + key + "' must be a number");
        return static_cast<float>(it->value.GetDouble());
    }

    StatCurve ReadStatCurve(const rapidjson::Value& curves_object, const char* key)
    {
        auto it = curves_object.FindMember(key);
        if (it == curves_object.MemberEnd() || !it->value.IsObject())
            throw JsonFileError(std::string("growth curve: '") + key + "' must be an object");

        StatCurve curve;
        curve.base = ReadFloat(it->value, "base", curve.base);
        curve.rate = ReadFloat(it->value, "rate", curve.rate);
        curve.exponent = ReadFloat(it->value, "exponent", curve.exponent);
        return curve;
    }

} // namespace

GrowthCurveLevel GrowthCurve::Evaluate(int level) const
{
    GrowthCurveLevel result;
    result.level = level;
    result.xp_to_next = EvaluateStat(xp_to_next, level);
    result.max_hp = EvaluateStat(max_hp, level);
    result.max_tp = EvaluateStat(max_tp, level);
    result.stats.atp = EvaluateStat(atp, level);
    result.stats.ata = EvaluateStat(ata, level);
    result.stats.mst = EvaluateStat(mst, level);
    result.stats.dfp = EvaluateStat(dfp, level);
    result.stats.evp = EvaluateStat(evp, level);
    result.stats.lck = EvaluateStat(lck, level);
    return result;
}

GrowthCurveLevel GrowthCurve::EvaluateGain(int level) const
{
    GrowthCurveLevel gain = Evaluate(level);

    if (level <= 2)
    {
        gain.max_hp = RoundedRate(max_hp);
        gain.max_tp = RoundedRate(max_tp);
        gain.stats.atp = RoundedRate(atp);
        gain.stats.ata = RoundedRate(ata);
        gain.stats.mst = RoundedRate(mst);
        gain.stats.dfp = RoundedRate(dfp);
        gain.stats.evp = RoundedRate(evp);
        gain.stats.lck = RoundedRate(lck);
        return gain;
    }

    const GrowthCurveLevel previous = Evaluate(level - 1);
    gain.max_hp -= previous.max_hp;
    gain.max_tp -= previous.max_tp;
    gain.stats.atp -= previous.stats.atp;
    gain.stats.ata -= previous.stats.ata;
    gain.stats.mst -= previous.stats.mst;
    gain.stats.dfp -= previous.stats.dfp;
    gain.stats.evp -= previous.stats.evp;
    gain.stats.lck -= previous.stats.lck;
    return gain;
}

GrowthCurveLevel GrowthCurve::EvaluateRandomGain(int level, std::mt19937& rng) const
{
    GrowthCurveLevel gain = EvaluateGain(level);
    gain.max_hp = Jitter(gain.max_hp, rng);
    gain.max_tp = Jitter(gain.max_tp, rng);
    gain.stats.atp = Jitter(gain.stats.atp, rng);
    gain.stats.ata = Jitter(gain.stats.ata, rng);
    gain.stats.mst = Jitter(gain.stats.mst, rng);
    gain.stats.dfp = Jitter(gain.stats.dfp, rng);
    gain.stats.evp = Jitter(gain.stats.evp, rng);
    gain.stats.lck = Jitter(gain.stats.lck, rng);
    return gain;
}

GrowthCurve ParseGrowthCurve(const rapidjson::Value& curves_object)
{
    if (!curves_object.IsObject())
        throw JsonFileError("growth curve: 'curves' must be an object");

    GrowthCurve curve;
    curve.xp_to_next = ReadStatCurve(curves_object, "xp_to_next");
    curve.max_hp = ReadStatCurve(curves_object, "max_hp");
    curve.max_tp = ReadStatCurve(curves_object, "max_tp");
    curve.atp = ReadStatCurve(curves_object, "atp");
    curve.ata = ReadStatCurve(curves_object, "ata");
    curve.mst = ReadStatCurve(curves_object, "mst");
    curve.dfp = ReadStatCurve(curves_object, "dfp");
    curve.evp = ReadStatCurve(curves_object, "evp");
    curve.lck = ReadStatCurve(curves_object, "lck");
    return curve;
}

} // namespace psr
