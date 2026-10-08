#include "game/buildings/MapOverlayChannelsConfigParser.h"
#include "lib/config/JsonConfigLoader.h"

#include <nlohmann/json.hpp>
#include <stdexcept>

namespace ac
{

MapOverlayChannelsConfig_t MapOverlayChannelsConfigParser::ParseConfig(
    const std::string& rConfigPath) const
{
    return JsonConfigLoader::LoadObjectFile<MapOverlayChannelsConfig_t>(
        rConfigPath, "map overlay channel",
        [](const nlohmann::json& rJson) {
            MapOverlayChannelsConfig_t config;
            for (const auto& [rChannelId, rEntry] : rJson.items())
            {
                if (rChannelId.empty())
                {
                    throw std::runtime_error("map_overlay_channels: channel id must not be empty");
                }
                if (!rEntry.is_object())
                {
                    throw std::runtime_error("map_overlay_channels '" + rChannelId
                                             + "': value must be an object");
                }
                if (!rEntry.contains("layer") || !rEntry.at("layer").is_number_integer())
                {
                    throw std::runtime_error("map_overlay_channels '" + rChannelId
                                             + "': 'layer' must be an integer");
                }
                for (const auto& [rKey, rUnused] : rEntry.items())
                {
                    if (rKey != "layer")
                    {
                        throw std::runtime_error("map_overlay_channels '" + rChannelId
                                                 + "': unknown key '" + rKey + "'");
                    }
                }
                config.layersByChannel.emplace(rChannelId, rEntry.at("layer").get<int>());
            }
            return config;
        });
}

} // namespace ac
