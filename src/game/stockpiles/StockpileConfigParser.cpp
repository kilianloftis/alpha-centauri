#include "game/stockpiles/StockpileConfigParser.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include "game/effects/EffectConfigParser.h"
#include <algorithm>
#include <stdexcept>
#include <variant>

namespace ac
{

namespace
{

const std::vector<std::string>& KnownStockpileKeys_()
{
    static const std::vector<std::string> keys = {
        "id", "name", "required_tech", "fallback_priority", "rounding", "effects",
    };
    return keys;
}

bool HasMineralsConverted_(const std::vector<EffectConfig_t>& rEffects)
{
    return std::any_of(rEffects.begin(), rEffects.end(), [](const EffectConfig_t& rEffect) {
        const auto* pMod = std::get_if<StatModifierEffect_t>(&rEffect.effect);
        return pMod && pMod->amountSource == StatModifierEffect_t::AmountSource_t::MineralsConverted;
    });
}

StockpileRounding_t ParseRounding_(const nlohmann::json& rJson, const StockpileId_t& rId)
{
    if (!rJson.contains("rounding") || rJson.at("rounding").is_null())
    {
        throw std::runtime_error(
            "Stockpile '" + rId
            + "': 'rounding' is required (\"down\", \"up\", or \"nearest\") — a fractional "
              "conversion rate does not say what a leftover fraction is worth, and that is a "
              "balance rule for config to state, not for the engine to assume");
    }
    const std::string value = rJson.at("rounding").get<std::string>();
    if (value == "down") { return StockpileRounding_t::Down; }
    if (value == "up") { return StockpileRounding_t::Up; }
    if (value == "nearest") { return StockpileRounding_t::Nearest; }
    throw std::runtime_error("Stockpile '" + rId + "': unknown 'rounding' value '" + value
                             + "' (expected \"down\", \"up\", or \"nearest\")");
}

// A stockpile's yield is by definition a function of the minerals it consumes. A plain Add
// with no amount_source would pay out every turn regardless — including turns with nothing
// left to convert — so it is rejected rather than silently becoming a per-turn stipend.
// Percentage / multiplier ops are fine: they scale the converted amount.
void RejectMineralIndependentYield_(const std::vector<EffectConfig_t>& rEffects,
                                    const StockpileId_t& rId)
{
    for (const EffectConfig_t& rEffect : rEffects)
    {
        const auto* pMod = std::get_if<StatModifierEffect_t>(&rEffect.effect);
        if (!pMod || pMod->op != ModifierOp_t::Add)
        {
            continue;
        }
        if (pMod->amountSource != StatModifierEffect_t::AmountSource_t::MineralsConverted)
        {
            throw std::runtime_error(
                "Stockpile '" + rId
                + "': a StatModifier with op Add must carry amount_source MineralsConverted — a "
                  "flat Add would pay out even on turns with no minerals to convert. Use "
                  "AddPercent or MultiplyGeometric to scale the converted yield");
        }
    }
}

} // namespace

std::vector<StockpileConfig_t> StockpileConfigParser::ParseConfig(const std::string& configPath)
{
    return JsonConfigLoader::LoadPath<StockpileConfig_t>(
        configPath, "stockpile",
        [this](const nlohmann::json& rJson) { return ParseStockpileConfig_(rJson); });
}

StockpileConfig_t StockpileConfigParser::ParseStockpileConfig_(const nlohmann::json& stockpileJson)
{
    StockpileConfig_t config;
    config.id = ConfigFields::ParseId(stockpileJson);
    config.name = ConfigFields::ParseName(stockpileJson, config.id);

    for (const auto& [rKey, rUnused] : stockpileJson.items())
    {
        const std::vector<std::string>& rKnown = KnownStockpileKeys_();
        if (std::find(rKnown.begin(), rKnown.end(), rKey) == rKnown.end())
        {
            throw std::runtime_error("Stockpile '" + config.id + "': unknown key '" + rKey + "'");
        }
    }

    config.requiredTech = ConfigFields::ParseRequiredTech(stockpileJson);

    if (stockpileJson.contains("fallback_priority") && !stockpileJson.at("fallback_priority").is_null())
    {
        const nlohmann::json& rPriority = stockpileJson.at("fallback_priority");
        if (!rPriority.is_number_integer())
        {
            throw std::runtime_error("Stockpile '" + config.id
                                     + "': 'fallback_priority' must be an integer");
        }
        config.fallbackPriority = rPriority.get<int>();
    }

    config.rounding = ParseRounding_(stockpileJson, config.id);

    config.effects =
        EffectConfigParser::ParseEffects(stockpileJson, EffectSourceKind_t::Stockpile, config.id);
    if (!HasMineralsConverted_(config.effects))
    {
        throw std::runtime_error(
            "Stockpile '" + config.id
            + "': requires at least one StatModifier with amount_source MineralsConverted "
              "(a stockpile that converts nothing would be an unselectable dead end)");
    }
    RejectMineralIndependentYield_(config.effects, config.id);

    return config;
}

} // namespace ac
