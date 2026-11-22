#pragma once

#include "game/PlayerInteraction.h"

#include <string>

namespace ac
{

struct SubmergenceNoticeCopy_t
{
    std::string title;
    std::string body;
};

SubmergenceNoticeCopy_t SubmergenceNoticeCopy(const SubmergenceNoticeInteraction_t& rNotice,
                                              FactionId_t audienceId);

} // namespace ac
