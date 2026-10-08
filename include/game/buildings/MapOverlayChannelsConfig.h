#pragma once

#include <string>
#include <unordered_map>

namespace ac
{

// Known map-overlay channels and their default draw layer (lower draws under higher).
// Loaded from map_overlay_channels.json; buildings may only name registered channels.
struct MapOverlayChannelsConfig_t
{
    // channel id → default layer
    std::unordered_map<std::string, int> layersByChannel;
};

} // namespace ac
