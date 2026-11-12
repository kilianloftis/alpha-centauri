#include "game/production/ScrapConfigParser.h"

#include <optional>
#include <stdexcept>

namespace ac
{

namespace
{

std::optional<std::string> OptionalString_(const ConfigReader& rReader, const char* pKey)
{
    if (!rReader.Has(pKey))
    {
        return std::nullopt;
    }
    return rReader.NonEmptyString(pKey);
}

Formula_t RefundFormula_(const ConfigReader& rReader)
{
    return rReader.Formula("formula", {"minerals"});
}

std::optional<StatId_t> OptionalRefundType_(const ConfigReader& rReader)
{
    const std::optional<std::string> type = OptionalString_(rReader, "refund_type");
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
        rReader.FailKey("refund_type", "is not a known scrap refund type");
    }
    if (!IsScrapRefundStat(stat))
    {
        rReader.FailKey("refund_type", "is not a creditable scrap payout");
    }
    return stat;
}

} // namespace

ScrapKindConfig_t ScrapConfigParser::ParseKindConfig(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"formula", "refund_type", "refund_ceiling_percent"});

    ScrapKindConfig_t kind;
    kind.formula = RefundFormula_(rReader);
    const std::optional<StatId_t> refundType = OptionalRefundType_(rReader);
    if (!refundType)
    {
        rReader.FailKey("refund_type", "is required");
    }
    kind.refundType = *refundType;
    kind.refundCeilingPercent = rReader.IntAtLeast("refund_ceiling_percent", 0);
    return kind;
}

ScrapOverride_t ScrapConfigParser::ParseOverride(const ConfigReader& rReader)
{
    rReader.RejectUnknownKeys({"formula", "refund_type"});

    ScrapOverride_t override;
    if (rReader.Has("formula") && rReader.Json().at("formula").is_null())
    {
        if (rReader.Has("refund_type"))
        {
            rReader.Fail("a null formula denies scrap; omit refund_type");
        }
        override.formula = Formula_t{};
    }
    else if (rReader.Has("formula"))
    {
        override.formula = RefundFormula_(rReader);
    }
    override.refundType = OptionalRefundType_(rReader);
    if (!override.formula && !override.refundType)
    {
        rReader.Fail("must include formula or refund_type");
    }
    return override;
}

} // namespace ac
