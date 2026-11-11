#include "game/population/GrowthConfigParser.h"
#include "game/effects/EffectConfigParser.h"
#include "lib/config/JsonConfigLoader.h"
#include <string>
#include <variant>

namespace ac
{

namespace
{

// A positive Add on starting_size or max_base_size. Returns the stat it sets.
StatId_t ValidateBaselineEffect_(const EffectConfig_t& rEffect, const ConfigReader& rReader)
{
    const auto* pMod = std::get_if<StatModifierEffect_t>(&rEffect.effect);
    if (!pMod)
    {
        rReader.Fail("only StatModifier effects are allowed");
    }
    if (pMod->stat != StatId_t::StartingSize && pMod->stat != StatId_t::MaxBaseSize)
    {
        rReader.Fail("stat is not allowed here (use starting_size or max_base_size)");
    }
    if (pMod->op != ModifierOp_t::Add)
    {
        rReader.Fail("population baselines must use op Add");
    }
    if (pMod->amount <= 0.0)
    {
        rReader.Fail("population baselines must be > 0, got " + std::to_string(pMod->amount));
    }
    return pMod->stat;
}

// Exactly the two population baselines, so nothing joins the faction pool from a file whose
// name does not say so.
void ValidateGrowthEffects_(const std::vector<EffectConfig_t>& rEffects,
                            const ConfigReader& rReader)
{
    bool bHasStartingSize = false;
    bool bHasMaxBaseSize = false;
    for (const EffectConfig_t& rEffect : rEffects)
    {
        const StatId_t stat = ValidateBaselineEffect_(rEffect, rReader);
        bHasStartingSize = bHasStartingSize || stat == StatId_t::StartingSize;
        bHasMaxBaseSize = bHasMaxBaseSize || stat == StatId_t::MaxBaseSize;
    }
    // Absent, StartingSize 0 throws at the first founding and MaxBaseSize 0 stops all growth, so
    // both are required at load.
    if (!bHasStartingSize)
    {
        rReader.Fail("'effects' must contain a starting_size StatModifier");
    }
    if (!bHasMaxBaseSize)
    {
        rReader.Fail("'effects' must contain a max_base_size StatModifier");
    }
}

GrowthConfig_t ParseGrowth_(const ConfigReader& rReader)
{
    GrowthConfig_t config;
    // Required: a nutrients_per_pop of 0 would make every base grow every turn.
    config.nutrientsPerPop = rReader.IntAtLeast("nutrients_per_pop", 1);
    config.nutrientIntakePerCitizen = rReader.IntAtLeast("nutrient_intake_per_citizen", 1);
    rReader.Array("effects");
    // Assigned, not appended: the struct's programmatic defaults exist for tests, and
    // must not survive into a config that declared its own baselines.
    config.effects =
        EffectConfigParser::ParseEffects(rReader.Json(), EffectSourceKind_t::Growth, "pop_growth");
    ValidateGrowthEffects_(config.effects, rReader);
    return config;
}

} // namespace

GrowthConfig_t GrowthConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadObjectFile<GrowthConfig_t>(configPath, "population growth",
                                                            ParseGrowth_);
}

} // namespace ac
