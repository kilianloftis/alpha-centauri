#include "game/base/BaseConquestCalculator.h"

#include "game/base/BaseConquestConfig.h"

namespace ac
{

BaseConquestCalculator::BaseConquestCalculator(const BaseConquestConfig_t& rConfig)
    : m_rConfig(rConfig)
{}

int BaseConquestCalculator::HeadquartersEscapeScore(const EscapeScoreInputs_t& rInputs) const
{
    const HeadquartersEscapeConfig_t& rConfig = m_rConfig.headquartersEscape;
    const int spread = rConfig.scoreSeed + rInputs.sameRegionDistance
        + rConfig.otherRegionDistanceFactor * rInputs.otherRegionDistance;
    const int weight = rInputs.regionBases + rConfig.populationFactor * rInputs.population;
    return spread * rConfig.scoreScale / weight;
}

} // namespace ac
