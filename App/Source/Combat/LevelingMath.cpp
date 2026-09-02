#include "Combat/LevelingMath.h"

#include <cmath>

namespace psr {

int ExpRequiredForLevel(int level, const LevelingConfig& config)
{
    return static_cast<int>(
        std::lround(static_cast<float>(config.exp_base) * std::pow(static_cast<float>(level), config.exp_growth_exponent)));
}

} // namespace psr
