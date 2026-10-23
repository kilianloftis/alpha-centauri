#include "game/production/StockpileConfigParser.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"
#include "game/effects/EffectConfigParser.h"
#include <algorithm>
#include <optional>
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
        return pMod && pMod->HasAmountSource(StatModifierEffect_t::AmountSource_t::MineralsConverted);
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

// A stockpile yields only from the minerals it converts, so a plain Add without an amount
// source is rejected; percentage and multiplier ops scale the converted amount.
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
        if (!pMod->HasAmountSource(StatModifierEffect_t::AmountSource_t::MineralsConverted))
        {
            throw std::runtime_error(
                "Stockpile '" + rId
                + "': a StatModifier with op Add must carry amount_source MineralsConverted — a "
                  "flat Add would pay out even on turns with no minerals to convert. Use "
                  "AddPercent or MultiplyGeometric to scale the converted yield");
        }
    }
}

void RejectUnknownStockpileKeys_(const nlohmann::json& rJson, const std::string& rId)
{
    const std::vector<std::string>& rKnown = KnownStockpileKeys_();
    for (const auto& [rKey, rUnused] : rJson.items())
    {
        if (std::find(rKnown.begin(), rKnown.end(), rKey) == rKnown.end())
        {
            throw std::runtime_error("Stockpile '" + rId + "': unknown key '" + rKey + "'");
        }
    }
}

std::optional<int> ParseFallbackPriority_(const nlohmann::json& rJson, const std::string& rId)
{
    if (!rJson.contains("fallback_priority") || rJson.at("fallback_priority").is_null())
    {
        return std::nullopt;
    }
    const nlohmann::json& rPriority = rJson.at("fallback_priority");
    if (!rPriority.is_number_integer())
    {
        throw std::runtime_error("Stockpile '" + rId + "': 'fallback_priority' must be an integer");
    }
    return rPriority.get<int>();
}

// A stockpile that converts nothing would be an unselectable dead end.
void RequireConvertsMinerals_(const std::vector<EffectConfig_t>& rEffects, const std::string& rId)
{
    if (!HasMineralsConverted_(rEffects))
    {
        throw std::runtime_error(
            "Stockpile '" + rId
            + "': requires at least one StatModifier with amount_source MineralsConverted "
              "(a stockpile that converts nothing would be an unselectable dead end)");
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
    config.name = ConfigFields::ParseName(stockpileJson, config.id, "name");

    RejectUnknownStockpileKeys_(stockpileJson, config.id);
    config.requiredTech = ConfigFields::ParseRequiredTech(stockpileJson);
    if (const std::optional<int> priority = ParseFallbackPriority_(stockpileJson, config.id))
    {
        config.fallbackPriority = *priority;
    }
    config.rounding = ParseRounding_(stockpileJson, config.id);
    config.effects =
        EffectConfigParser::ParseEffects(stockpileJson, EffectSourceKind_t::Stockpile, config.id);
    RequireConvertsMinerals_(config.effects, config.id);
    RejectMineralIndependentYield_(config.effects, config.id);
    return config;
}

} // namespace ac
