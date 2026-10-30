#pragma once

#include "game/PlayerInteraction.h"

#include <optional>
#include <string>

namespace ac
{

struct CaptureNoticeCopy_t
{
    std::string title;
    std::string body;
};

// The captor's or victim's battle report. Unset for any other audience.
std::optional<CaptureNoticeCopy_t> CaptureNoticeCopy(const CaptureNoticeInteraction_t& rNotice,
                                                    FactionId_t audienceId);

} // namespace ac
