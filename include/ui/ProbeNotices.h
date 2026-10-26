#pragma once

#include "game/PlayerInteraction.h"

#include <optional>
#include <string>

namespace ac
{

struct ProbeNoticeCopy_t
{
    std::string title;
    std::string body;
};

// Unset when this audience has no script for the outcome.
std::optional<ProbeNoticeCopy_t> ProbeNoticeCopy(const ProbeNoticeInteraction_t& rNotice,
                                                FactionId_t audienceId);

} // namespace ac
