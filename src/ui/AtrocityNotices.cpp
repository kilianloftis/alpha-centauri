#include "ui/AtrocityNotices.h"

#include <stdexcept>
#include <string>
#include <string_view>

namespace ac
{

namespace
{

std::string_view SeverityWord_(AtrocitySeverityId_t id)
{
    switch (id)
    {
    case AtrocitySeverityId_t::Simple:
        return "simple";
    case AtrocitySeverityId_t::Major:
        return "major";
    }
    throw std::runtime_error("Atrocity severity has no label: "
                             + std::to_string(static_cast<int>(id)));
}

std::string ExcuseClause_(const AtrocityNoticeInteraction_t& rNotice)
{
    switch (rNotice.excuse)
    {
    case AtrocityExcuse_t::None:
        if (rNotice.sanctionUntilYear)
        {
            return " Commerce sanctions are in force until "
                   + std::to_string(*rNotice.sanctionUntilYear) + ".";
        }
        return {};
    case AtrocityExcuse_t::CharterRepealed:
        return " The Planetary Council no longer forbids it, so no sanctions follow.";
    case AtrocityExcuse_t::ProgenitorParty:
        return " Other faction leaders take no interest, so no sanctions follow.";
    }
    throw std::runtime_error("Atrocity excuse has no copy: "
                             + std::to_string(static_cast<int>(rNotice.excuse)));
}

} // namespace

AtrocityNoticeCopy_t AtrocityNoticeCopy(const AtrocityNoticeInteraction_t& rNotice)
{
    std::string against;
    if (rNotice.victimName)
    {
        against = " against " + *rNotice.victimName;
    }
    AtrocityNoticeCopy_t copy;
    copy.title = "Atrocity";
    copy.body = rNotice.perpetratorName + " has committed a "
                + std::string(SeverityWord_(rNotice.severity)) + " atrocity" + against + "."
                + ExcuseClause_(rNotice);
    return copy;
}

} // namespace ac
