#include "game/faction/base/BaseMoodEffects.h"

#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/population/pop-types/PopCompositionConfigParser.h"

#include <stdexcept>

namespace ac
{

namespace
{

const PopCompositionConfig_t& RequireConfig_(const BaseManager& rBase)
{
    const GameDataContext& rData = rBase.GetFaction().GetDataContext();
    if (!rData.popCompositionConfig)
    {
        throw std::runtime_error(
            "BaseMoodEffects: GameDataContext has no popCompositionConfig");
    }
    return *rData.popCompositionConfig;
}

template <typename AppendFn>
void AppendMood_(const BaseManager& rBase, AppendFn append, std::vector<ActiveEffect_t>& rOut)
{
    const PopCompositionConfig_t& rConfig = RequireConfig_(rBase);
    const PopulationManager& rPopulation = rBase.GetPopulation();

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
    return FindActiveRiotTier(RequireConfig_(rBase), rPopulation.GetConsecutiveRiotTurns());
}

} // namespace ac
