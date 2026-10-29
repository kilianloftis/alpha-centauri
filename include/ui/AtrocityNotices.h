#pragma once

#include "game/PlayerInteraction.h"

#include <string>

namespace ac
{

struct AtrocityNoticeCopy_t
{
    std::string title;
    std::string body;
};

AtrocityNoticeCopy_t AtrocityNoticeCopy(const AtrocityNoticeInteraction_t& rNotice);

} // namespace ac
