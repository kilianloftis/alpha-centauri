#pragma once

#include "game/PlayerInteraction.h"
#include "ui/text/TextPack.h"

#include <optional>
#include <string>

namespace ac
{

struct SupplyPodNoticeCopy_t
{
    std::string title;
    std::string body;
    std::optional<std::string> imagePath;
};

// Formats the notice's TextPack script entry with substitution slots.
SupplyPodNoticeCopy_t SupplyPodNoticeCopy(const SupplyPodNoticeInteraction_t& rNotice,
                                          const TextPack& rText);

} // namespace ac
