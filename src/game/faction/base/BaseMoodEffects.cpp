#include "game/faction/base/BaseMoodEffects.h"

#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/population/pop-types/PopCompositionConfigParser.h"

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
    return FindActiveRiotTier(rPopulation.GetCompositionConfig(),
                              rPopulation.GetConsecutiveRiotTurns());
}

} // namespace ac
