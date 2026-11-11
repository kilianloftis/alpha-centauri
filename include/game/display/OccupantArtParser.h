#pragma once

#include "game/display/OccupantArt.h"

#include "lib/config/ConfigReader.h"

#include <optional>

namespace ac
{

// The "art" object of one terrain or improvement entry; nullopt when the entry has none.
std::optional<OccupantArt_t> ParseOccupantArt(const ConfigReader& rEntry);

} // namespace ac
