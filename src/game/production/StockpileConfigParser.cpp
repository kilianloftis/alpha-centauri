#include "game/production/StockpileConfigParser.h"
#include "lib/config/JsonConfigLoader.h"
#include "game/effects/EffectConfigParser.h"
#include <algorithm>
#include <variant>

namespace ac
{

namespace
{

bool HasMineralsConverted_(const std::vector<EffectConfig_t>& rEffects)
{
    return std::any_of(rEffects.begin(), rEffects.end(), [](const EffectConfig_t& rEffect) {
        const auto* pMod = std::get_if<StatModifierEffect_t>(&rEffect.effect);
        return pMod && pMod->HasAmountSource(StatModifierEffect_t::AmountSource_t::MineralsConverted);
    });
}

StockpileRounding_t ParseRounding_(const ConfigReader& rReader)
{
    if (!rReader.Has("rounding"))
    {
        rReader.FailKey("rounding",
                        "is required (\"down\", \"up\", or \"nearest\") — a fractional "
                        "conversion rate does not say what a leftover fraction is worth, and that "
                        "is a balance rule for config to state, not for the engine to assume");
    }
    const std::string value = rReader.String("rounding");
    if (value == "down") { return StockpileRounding_t::Down; }
    if (value == "up") { return StockpileRounding_t::Up; }
    if (value == "nearest") { return StockpileRounding_t::Nearest; }
    rReader.FailKey("rounding", "has unknown value '" + value
                                    + "' (expected \"down\", \"up\", or \"nearest\")");
}

// A stockpile yields only from the minerals it converts, so a plain Add without an amount
// source is rejected; percentage and multiplier ops scale the converted amount.
void RejectMineralIndependentYield_(const std::vector<EffectConfig_t>& rEffects,
                                    const ConfigReader& rReader)
{
    for (const EffectConfig_t& rEffect : rEffects)
    {
        const auto* pMod = std::get_if<StatModifierEffect_t>(&rEffect.effect);
        if (!pMod || pMod->op != ModifierOp_t::Add)
        {
            continue;
        }
        if (!pMod->HasAmountSource(StatModifierEffect_t::AmountSource_t::MineralsConverted))
        {
            rReader.Fail(
                "a StatModifier with op Add must carry amount_source MineralsConverted — a "
                "flat Add would pay out even on turns with no minerals to convert. Use "
                "AddPercent or MultiplyGeometric to scale the converted yield");
        }
    }
}

// A stockpile that converts nothing would be an unselectable dead end.
void RequireConvertsMinerals_(const std::vector<EffectConfig_t>& rEffects,
                              const ConfigReader& rReader)
{
    if (!HasMineralsConverted_(rEffects))
    {
        rReader.Fail("requires at least one StatModifier with amount_source MineralsConverted "
                     "(a stockpile that converts nothing would be an unselectable dead end)");
    }
}

StockpileConfig_t ParseStockpile_(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({
        "id", "name", "required_tech", "fallback_priority", "rounding", "effects",
    });
    StockpileConfig_t config;
    config.id = rReader.NonEmptyString("id");
    config.name = rReader.String("name");
    config.requiredTech = rReader.StringOr("required_tech", "");
    config.fallbackPriority = rReader.IntOr("fallback_priority", config.fallbackPriority);
    config.rounding = ParseRounding_(rReader);
    config.effects = EffectConfigParser::ParseEffects(rReader.Json(),
                                                      EffectSourceKind_t::Stockpile, config.id);
    RequireConvertsMinerals_(config.effects, rReader);
    RejectMineralIndependentYield_(config.effects, rReader);
    return config;
}

} // namespace

std::vector<StockpileConfig_t> StockpileConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadPath<StockpileConfig_t>(configPath, "stockpile",
                                                         ParseStockpile_);
}

} // namespace ac
