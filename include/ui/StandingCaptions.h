#pragma once

#include <nlohmann/json_fwd.hpp>

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

private:
    // The name for counts from min up to the next band's min.
    struct Band_t
    {
        std::string name;
        int min = 0;
    };

    StandingCaptions(std::vector<Band_t> moods, std::vector<Band_t> repute);

    static StandingCaptions Parse_(const nlohmann::json& rJson, const std::string& rConfigPath,
                                   const DiplomacyConfig_t& rDiplomacy);
    static std::vector<Band_t> ParseBands_(const nlohmann::json& rJson, const std::string& rKey,
                                           const std::string& rMinKey, int max,
                                           const std::string& rContext);
    static Band_t ParseBand_(const nlohmann::json& rEntry, const std::string& rMinKey,
                             const std::string& rContext);
    // Bands rise from 0 to at most the stored maximum, so every one can be shown.
    static void RequireReachable_(const std::vector<Band_t>& rBands, int max,
                                  const std::string& rContext);
    static const std::string& NameFor_(const std::vector<Band_t>& rBands, int value);

    std::vector<Band_t> m_moods;
    std::vector<Band_t> m_repute;
};

} // namespace ac
