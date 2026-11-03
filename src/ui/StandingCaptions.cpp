#include "ui/StandingCaptions.h"

#include "game/diplomacy/DiplomacyConfig.h"
#include "lib/config/ConfigFields.h"
#include "lib/config/JsonConfigLoader.h"

#include <algorithm>
#include <iterator>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ac
{

StandingCaptions::StandingCaptions(std::vector<Band_t> moods, std::vector<Band_t> repute,
                                   int vendettaMoodFloor)
    : m_moods(std::move(moods))
    , m_repute(std::move(repute))
    , m_vendettaMoodFloor(vendettaMoodFloor)
{
}

const std::string& StandingCaptions::CaptionMood(int friction, DiplomaticStatus_t status) const
{
    if (status == DiplomaticStatus_t::Vendetta)
    {
        return Mood(std::max(friction, m_vendettaMoodFloor));
    }
    return Mood(friction);
}

StandingCaptions StandingCaptions::Load(const std::string& rConfigPath,
                                        const DiplomacyConfig_t& rDiplomacy)
{
    return JsonConfigLoader::LoadObjectFile<StandingCaptions>(
        rConfigPath, "standing caption",
        [&rConfigPath, &rDiplomacy](const nlohmann::json& rJson) {
            return Parse_(rJson, rConfigPath, rDiplomacy);
        });
}

StandingCaptions StandingCaptions::Parse_(const nlohmann::json& rJson,
                                          const std::string& rConfigPath,
                                          const DiplomacyConfig_t& rDiplomacy)
{
    const std::string context = "Standing captions '" + rConfigPath + "'";
    ConfigFields::RejectUnknownKeys(rJson, {"vendetta_mood_floor", "moods", "repute"}, context);
    std::vector<Band_t> moods =
        ParseBands_(rJson, "moods", "min_friction", rDiplomacy.frictionMax, context);
    std::vector<Band_t> repute =
        ParseBands_(rJson, "repute", "min_blemishes", rDiplomacy.blemishesMax, context);
    const int floor = ConfigFields::RequireIntAtLeast(rJson, "vendetta_mood_floor", 0, context);
    if (floor > rDiplomacy.frictionMax)
    {
        throw std::runtime_error(context + ": 'vendetta_mood_floor' is above the stored maximum "
                                 + std::to_string(rDiplomacy.frictionMax));
    }
    return StandingCaptions(std::move(moods), std::move(repute), floor);
}

std::vector<StandingCaptions::Band_t> StandingCaptions::ParseBands_(
    const nlohmann::json& rJson, const std::string& rKey, const std::string& rMinKey, int max,
    const std::string& rContext)
{
    const std::string context = rContext + " '" + rKey + "'";
    if (!rJson.contains(rKey) || !rJson.at(rKey).is_array() || rJson.at(rKey).empty())
    {
        throw std::runtime_error(context + ": must be a non-empty array");
    }
    std::vector<Band_t> bands;
    for (const nlohmann::json& rEntry : rJson.at(rKey))
    {
        bands.push_back(ParseBand_(rEntry, rMinKey, context));
    }
    RequireReachable_(bands, max, context);
    return bands;
}

StandingCaptions::Band_t StandingCaptions::ParseBand_(const nlohmann::json& rEntry,
                                                      const std::string& rMinKey,
                                                      const std::string& rContext)
{
    if (!rEntry.is_object())
    {
        throw std::runtime_error(rContext + ": each band must be an object");
    }
    ConfigFields::RejectUnknownKeys(rEntry, {"name", rMinKey}, rContext);
    if (!rEntry.contains("name") || !rEntry.at("name").is_string()
        || rEntry.at("name").get_ref<const std::string&>().empty())
    {
        throw std::runtime_error(rContext + ": each band needs a non-empty 'name'");
    }
    return Band_t{rEntry.at("name").get<std::string>(),
                  ConfigFields::RequireInt(rEntry, rMinKey, rContext)};
}

void StandingCaptions::RequireReachable_(const std::vector<Band_t>& rBands, int max,
                                         const std::string& rContext)
{
    if (rBands.front().min != 0)
    {
        throw std::runtime_error(rContext + ": the first band must start at 0");
    }
    for (std::size_t i = 1; i < rBands.size(); ++i)
    {
        if (rBands[i].min <= rBands[i - 1].min)
        {
            throw std::runtime_error(rContext + ": band '" + rBands[i].name
                                     + "' must start above the band before it");
        }
    }
    if (rBands.back().min > max)
    {
        throw std::runtime_error(rContext + ": band '" + rBands.back().name
                                 + "' starts above the stored maximum "
                                 + std::to_string(max));
    }
}

const std::string& StandingCaptions::NameFor_(const std::vector<Band_t>& rBands, int value)
{
    // The first band starts at 0, and stored counts never go below it.
    const auto itAbove = std::ranges::upper_bound(rBands, value, {}, &Band_t::min);
    return std::prev(itAbove)->name;
}

} // namespace ac
