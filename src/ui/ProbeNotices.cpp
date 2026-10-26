#include "ui/ProbeNotices.h"

namespace ac
{

namespace
{

constexpr const char* k_DirectorTitle = "Operations Director";

std::optional<std::string> InciteBody_(ProbeActionStatus_t status, const std::string& rBaseName)
{
    switch (status)
    {
    case ProbeActionStatus_t::Riot:
        return "Drone riots incited at " + rBaseName + "!";
    case ProbeActionStatus_t::IncreasedActivity:
        return "Drone activity increased at " + rBaseName + "!";
    case ProbeActionStatus_t::Infiltrated:
    case ProbeActionStatus_t::NoTech:
    case ProbeActionStatus_t::ProductionWiped:
    case ProbeActionStatus_t::ResearchWiped:
    case ProbeActionStatus_t::BaseCaptured:
    case ProbeActionStatus_t::UnitSubverted:
    case ProbeActionStatus_t::NoTarget:
        // TODO: SMAC scripts for these outcomes (DETECTINFILTRATE, GOTENERGY, …).
        return std::nullopt;
    }
    return std::nullopt;
}

} // namespace

std::optional<ProbeNoticeCopy_t> ProbeNoticeCopy(const ProbeNoticeInteraction_t& rNotice)
{
    const auto* pStatus = std::get_if<ProbeActionStatus_t>(&rNotice.result.detail);
    if (!pStatus)
    {
        return std::nullopt;
    }
    const std::optional<std::string> body = InciteBody_(*pStatus, rNotice.subjectName);
    if (!body.has_value())
    {
        return std::nullopt;
    }
    return ProbeNoticeCopy_t{k_DirectorTitle, *body};
}

} // namespace ac
