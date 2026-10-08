#pragma once

#include "game/map/OccupantArt.h"

#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace ac
{

// The "art" object of one terrain or improvement entry; nullopt when the entry has none.
// rId names the entry in error messages.
std::optional<OccupantArt_t> ParseOccupantArt(const nlohmann::json& rEntryJson,
                                              const std::string& rId);

} // namespace ac
