#include "game/faction/base/production/ScrapConfigParser.h"

#include <algorithm>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace ac
{

namespace
{

[[noreturn]] void Fail_(const std::string& rPath, const std::string& rMessage)
{
    throw std::runtime_error(rPath + " " + rMessage);
}

void RejectUnknownKeys_(const nlohmann::json& rValue, const std::string& rPath,
                        std::initializer_list<std::string_view> knownKeys)
{
    for (const auto& [rKey, rUnused] : rValue.items())
    {
        if (std::find(knownKeys.begin(), knownKeys.end(), rKey) == knownKeys.end())
        {
            Fail_(rPath, "unknown key '" + rKey + "'");
        }
    }
}

std::optional<std::string> OptionalString_(const nlohmann::json& rValue, const std::string& rPath,
                                           const char* pKey)
{
    if (!rValue.contains(pKey))
    {
        return std::nullopt;
    }
    if (!rValue.at(pKey).is_string() || rValue.at(pKey).get<std::string>().empty())
    {
        Fail_(rPath, std::string(pKey) + " must be a non-empty string");
    }
    return rValue.at(pKey).get<std::string>();
}

std::optional<StatId_t> OptionalRefundType_(const nlohmann::json& rValue, const std::string& rPath)
{
    const std::optional<std::string> type = OptionalString_(rValue, rPath, "refund_type");
    if (!type)
    {
        return std::nullopt;
    }
    StatId_t stat = StatId_t::EnergyCredits;
    try
    {
        stat = ParseStatId(*type);
    }
    catch (const std::runtime_error&)
    {
        Fail_(rPath, "refund_type is not a known scrap refund type");
    }
    if (!IsScrapRefundStat(stat))
    {
        Fail_(rPath, "refund_type is not a creditable scrap payout");
    }
    return stat;
}

} // namespace

ScrapKindConfig_t ScrapConfigParser::ParseKindConfig(const nlohmann::json& rValue,
                                                     const std::string& rPath)
{
    if (!rValue.is_object())
    {
        Fail_(rPath, "must be an object with formula, refund_type, and refund_ceiling_percent");
    }
    RejectUnknownKeys_(rValue, rPath, {"formula", "refund_type", "refund_ceiling_percent"});

    ScrapKindConfig_t kind;
    const std::optional<std::string> formula = OptionalString_(rValue, rPath, "formula");
    if (!formula)
    {
        Fail_(rPath, "formula must be a non-empty string");
    }
    kind.formula = *formula;

    const std::optional<StatId_t> refundType = OptionalRefundType_(rValue, rPath);
    if (!refundType)
    {
        Fail_(rPath, "refund_type must be a non-empty string");
    }
    kind.refundType = *refundType;

    if (!rValue.contains("refund_ceiling_percent")
        || !rValue.at("refund_ceiling_percent").is_number_integer())
    {
        Fail_(rPath, "refund_ceiling_percent must be an integer");
    }
    kind.refundCeilingPercent = rValue.at("refund_ceiling_percent").get<int>();
    if (kind.refundCeilingPercent < 0)
    {
        Fail_(rPath, "refund_ceiling_percent must be >= 0, got "
                         + std::to_string(kind.refundCeilingPercent));
    }
    return kind;
}

ScrapOverride_t ScrapConfigParser::ParseOverride(const nlohmann::json& rValue,
                                                 const std::string& rPath)
{
    if (!rValue.is_object())
    {
        Fail_(rPath, "must be an object with formula and/or refund_type");
    }
    RejectUnknownKeys_(rValue, rPath, {"formula", "refund_type"});

    ScrapOverride_t override;
    override.formula = OptionalString_(rValue, rPath, "formula");
    override.refundType = OptionalRefundType_(rValue, rPath);
    if (!override.formula && !override.refundType)
    {
        Fail_(rPath, "must include formula or refund_type");
    }
    return override;
}

} // namespace ac
