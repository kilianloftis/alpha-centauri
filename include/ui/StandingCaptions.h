#pragma once

#include "game/diplomacy/DiplomaticStatus.h"

#include "lib/config/ConfigReader.h"

#include <string>
#include <vector>

namespace ac
{

struct DiplomacyConfig_t;

// Mood and repute names for stored friction and blemish counts, from
// config/ui/standing_captions.json.
class StandingCaptions
{
public:
    static StandingCaptions Load(const std::string& rConfigPath,
                                 const DiplomacyConfig_t& rDiplomacy);

    const std::string& Mood(int friction) const { return NameFor_(m_moods, friction); }
    const std::string& Repute(int blemishes) const { return NameFor_(m_repute, blemishes); }
    // The talk caption's mood: SMAC's diplomacy_caption reads friction below the floor as the
    // floor at Vendetta without changing it (terranx-map.md).
    const std::string& CaptionMood(int friction, DiplomaticStatus_t status) const;

private:
    // The name for counts from min up to the next band's min.
    struct Band_t
    {
        std::string name;
        int min = 0;
    };

    StandingCaptions(std::vector<Band_t> moods, std::vector<Band_t> repute,
                     int vendettaMoodFloor);

    static StandingCaptions Parse_(const ConfigReader& rReader,
                                   const DiplomacyConfig_t& rDiplomacy);
    static std::vector<Band_t> ParseBands_(const ConfigReader& rReader, const std::string& rKey,
                                           const std::string& rMinKey, int max);
    static Band_t ParseBand_(const ConfigReader& rEntry, const std::string& rMinKey);
    // Bands rise from 0 to at most the stored maximum, so every one can be shown.
    static void RequireReachable_(const std::vector<Band_t>& rBands, int max,
                                  const std::string& rContext);
    static const std::string& NameFor_(const std::vector<Band_t>& rBands, int value);

    std::vector<Band_t> m_moods;
    std::vector<Band_t> m_repute;
    int m_vendettaMoodFloor = 0;
};

} // namespace ac
