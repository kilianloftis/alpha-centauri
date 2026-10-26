#include "game/base/BaseMoodEffects.h"

#include "game/base/BaseManager.h"
#include "game/population/PopulationManager.h"
#include "game/population/PopCompositionConfigParser.h"

#include <algorithm>

namespace ac
{

namespace
{

template <typename AppendFn>
void AppendMood_(const BaseManager& rBase, AppendFn append, std::vector<ActiveEffect_t>& rOut)
{
    const PopulationManager& rPopulation = rBase.GetPopulation();
    const PopCompositionConfig_t& rConfig = rPopulation.GetCompositionConfig();

    if (rPopulation.IsInGoldenAge())
    {
        append(rConfig.goldenAgeEffects, &rBase, "golden_age", rOut);
    }
    if (const RiotTier_t* pTier = ActiveRiotTierFor(rBase))
    {
        append(pTier->effects, &rBase, "riot_tier", rOut);
    }
}

} // namespace

void AppendBaseMoodBaseLaneEffects(const BaseManager& rBase, std::vector<ActiveEffect_t>& rOut)
{
    AppendMood_(rBase, AppendBaseLaneEffects, rOut);
}

void AppendBaseMoodFactionLaneEffects(const BaseManager& rBase, std::vector<ActiveEffect_t>& rOut)
{
    AppendMood_(rBase, AppendFactionLaneEffects, rOut);
}

const RiotTier_t* ActiveRiotTierFor(const BaseManager& rBase)
{
    const PopulationManager& rPopulation = rBase.GetPopulation();
    if (!rPopulation.IsRioting())
    {
        return nullptr;
    }
    // Incite latches before the first Mood commit, and SMAC's flag already blocks that turn's
    // production (terranx-map "Probe: Incite Drone Riots").
    const int turns = std::max(1, rPopulation.GetConsecutiveRiotTurns());
    return FindActiveRiotTier(rPopulation.GetCompositionConfig(), turns);
}

} // namespace ac
