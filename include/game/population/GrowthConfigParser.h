#pragma once

#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"

#include <string>
#include <vector>

namespace ac
{

struct GrowthConfig_t
{
    int nutrientsPerPop{};          // tank rows scale; see ComputeNutrientsRequired
    int nutrientIntakePerCitizen{}; // subtracted from production to form net surplus
    std::vector<EffectConfig_t> effects;
};

class GrowthConfigParser
{
public:
    // Load pop_growth.json. Throws if the file cannot be opened or parsed.
    static GrowthConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac
