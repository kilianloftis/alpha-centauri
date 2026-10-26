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

// Unset when this outcome has no copy yet.
std::optional<ProbeNoticeCopy_t> ProbeNoticeCopy(const ProbeNoticeInteraction_t& rNotice);

} // namespace ac
