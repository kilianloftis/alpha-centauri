#pragma once

namespace ac
{

struct BaseConquestConfig_t;

// What BaseConquestRules gathers to score one base as a Headquarters' escape destination.
struct EscapeScoreInputs_t
{
    // Distances to the owner's other bases in this base's region, and outside it.
    int sameRegionDistance = 0;
    int otherRegionDistance = 0;
    // The owner's bases in this base's region, this one included.
    int regionBases = 0;
    int population = 0;
};

// base_conquest.json's configured formulas.
class BaseConquestCalculator
{
public:
    explicit BaseConquestCalculator(const BaseConquestConfig_t& rConfig);

    // capture_base's ESCAPE score; the lowest wins.
    int HeadquartersEscapeScore(const EscapeScoreInputs_t& rInputs) const;

private:
    const BaseConquestConfig_t& m_rConfig;
};

} // namespace ac
