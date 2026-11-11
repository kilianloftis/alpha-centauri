#include "game/display/MapOverlayChannelsConfigParser.h"
#include "lib/config/JsonConfigLoader.h"

namespace ac
{

namespace
{

MapOverlayChannelsConfig_t ParseChannels_(const ConfigReader& rReader)
{
    MapOverlayChannelsConfig_t config;
    for (const auto& [rChannelId, rUnused] : rReader.Json().items())
    {
        if (rChannelId.empty())
        {
            rReader.Fail("channel id must not be empty");
        }
        const ConfigReader channel = rReader.Object(rChannelId);
        channel.RejectUnknownKeys({"layer"});
        config.layersByChannel.emplace(rChannelId, channel.Int("layer"));
    }
    return config;
}

} // namespace

MapOverlayChannelsConfig_t MapOverlayChannelsConfigParser::ParseConfig(
    const std::string& rConfigPath)
{
    return JsonConfigLoader::LoadObjectFile<MapOverlayChannelsConfig_t>(
        rConfigPath, "map overlay channels", ParseChannels_);
}

} // namespace ac
