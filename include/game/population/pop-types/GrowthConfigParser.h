#pragma once

#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"

#include <string>
#include <vector>

namespace ac
{

// Baseline AllOwnerBases StatModifiers for programmatic GrowthConfig_t (tests / defaults).
inline EffectConfig_t MakeGrowthBaselineStat(StatId_t stat, double amount,
                                             ModifierOp_t op = ModifierOp_t::Add)
{
    EffectConfig_t effect;
    effect.scope = EffectScope_t::AllOwnerBases;
    effect.persistence = EffectPersistence_t::Continuous;
    StatModifierEffect_t modifier;
    modifier.stat = stat;
    modifier.amount = amount;
    modifier.op = op;
    effect.effect = modifier;
    return effect;
}

inline std::vector<EffectConfig_t> MakeDefaultGrowthEffects()
{
    return {
        MakeGrowthBaselineStat(StatId_t::StartingSize, 1.0),
        MakeGrowthBaselineStat(StatId_t::MaxBaseSize, 7.0),
    };
}

struct GrowthConfig_t
{
    int nutrientsPerPop = 10;             // tank rows scale; see GrowthCalculator
    int nutrientIntakePerCitizen = 2;     // subtracted from production to form net surplus
    std::vector<EffectConfig_t> effects = MakeDefaultGrowthEffects();
};

class GrowthConfigParser
{
public:
    GrowthConfigParser() = default;
    ~GrowthConfigParser() = default;

    // Load pop_growth.json. Throws if the file cannot be opened or parsed.
    GrowthConfig_t ParseConfig(const std::string& configPath);
};

} // namespace ac
