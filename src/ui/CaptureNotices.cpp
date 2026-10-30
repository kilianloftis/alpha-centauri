#include "ui/CaptureNotices.h"

#include <stdexcept>
#include <string>

namespace ac
{

namespace
{

constexpr const char* k_BattleReportTitle = "Battle Report";

// "the Alien Artifact", or "3 Alien Artifact units".
std::string TakenUnits_(const CaptureNoticeInteraction_t& rNotice)
{
    if (rNotice.count == 1)
    {
        return "the " + rNotice.unitName;
    }
    return std::to_string(rNotice.count) + " " + rNotice.unitName + " units";
}

// "our Alien Artifact", or "3 of our Alien Artifact units".
std::string LostUnits_(const CaptureNoticeInteraction_t& rNotice)
{
    if (rNotice.count == 1)
    {
        return "our " + rNotice.unitName;
    }
    return std::to_string(rNotice.count) + " of our " + rNotice.unitName + " units";
}

std::string CaptorBody_(const CaptureNoticeInteraction_t& rNotice)
{
    switch (rNotice.kind)
    {
    case CaptureNoticeKind_t::Boarded:
        return "Our Marine Detachment boarded and captured the " + rNotice.victimAdjective + " "
               + rNotice.unitName + "!";
    case CaptureNoticeKind_t::TakenOnDefeat:
        return "We have captured " + TakenUnits_(rNotice) + "!";
    }
    throw std::logic_error("CaptureNoticeCopy: unknown CaptureNoticeKind_t");
}

// The script picks the captor's verb by its noun's number; nouns are plural, so "have".
std::string VictimBody_(const CaptureNoticeInteraction_t& rNotice)
{
    switch (rNotice.kind)
    {
    case CaptureNoticeKind_t::Boarded:
        return rNotice.captorAdjective + " Marine Detachment boarded and captured our "
               + rNotice.unitName + "!";
    case CaptureNoticeKind_t::TakenOnDefeat:
        if (rNotice.bVictimInVendetta)
        {
            return "The enemy has captured " + LostUnits_(rNotice) + "!";
        }
        return "The " + rNotice.captorNoun + " have appropriated " + LostUnits_(rNotice) + "!";
    }
    throw std::logic_error("CaptureNoticeCopy: unknown CaptureNoticeKind_t");
}

} // namespace

std::optional<CaptureNoticeCopy_t> CaptureNoticeCopy(const CaptureNoticeInteraction_t& rNotice,
                                                    FactionId_t audienceId)
{
    if (audienceId == rNotice.captorId)
    {
        return CaptureNoticeCopy_t{k_BattleReportTitle, CaptorBody_(rNotice)};
    }
    if (audienceId == rNotice.victimId)
    {
        return CaptureNoticeCopy_t{k_BattleReportTitle, VictimBody_(rNotice)};
    }
    return std::nullopt;
}

} // namespace ac
