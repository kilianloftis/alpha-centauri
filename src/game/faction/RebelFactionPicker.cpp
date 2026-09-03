#include "game/faction/RebelFactionPicker.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace ac
{

namespace
{

// Linear fade to zero at fadeRadius: adjacent is worth the most, fadeRadius away nothing.
int FadedBonus_(int distance, const RebelSelectionConfig_t& rConfig)
{
    return std::max(0, rConfig.fadeRadius - distance) * rConfig.distanceWeightPerTile;
}

int NearestBaseDistance_(const Faction& rFaction, const Tile& rFrom, int mapWidth)
{
    int best = std::numeric_limits<int>::max();
    for (const BaseManager& rBase : rFaction.Bases())
    {
        best = std::min(best, ChebyshevDistance(rFrom, rBase.GetTile(), mapWidth));
    }
    return best;
}

int HqDistance_(const Faction& rFaction, const Tile& rFrom, int mapWidth,
                const RebelSelectionConfig_t& rConfig)
{
    const BaseManager* pHq = rFaction.GetHeadquarters();
    if (!pHq)
    {
        return rConfig.missingHqDistance;
    }
    return ChebyshevDistance(rFrom, pHq->GetTile(), mapWidth);
}

int DistanceBonus_(const Faction& rCandidate, const BaseManager& rRebelling,
                   const RebelSelectionConfig_t& rConfig, int mapWidth)
{
    const Tile& rFrom = rRebelling.GetTile();
    switch (rConfig.distanceMode)
    {
        case RebelDistanceMode_t::None:
            return 0;
        case RebelDistanceMode_t::NearestBase:
            // Candidates are pre-filtered to faction with at least one base, so the scan
            // below always sees one.
            return FadedBonus_(NearestBaseDistance_(rCandidate, rFrom, mapWidth), rConfig);
        case RebelDistanceMode_t::HqDistance:
            return FadedBonus_(HqDistance_(rCandidate, rFrom, mapWidth, rConfig), rConfig);
        case RebelDistanceMode_t::NearbyBases:
        {
            int total = 0;
            for (const BaseManager& rBase : rCandidate.Bases())
            {
                total += FadedBonus_(ChebyshevDistance(rFrom, rBase.GetTile(), mapWidth),
                                     rConfig);
            }
            return total;
        }
    }
    return 0;
}

int RebelJoinWeight_(const Faction& rFaction, const RebelSelectionConfig_t& rConfig)
{
    return static_cast<int>(std::lround(
        ResolveFactionStat(rFaction.GetActiveEffects(), StatId_t::RebelJoinWeight,
                           static_cast<double>(rConfig.baseJoinWeight))));
}

struct Candidate_t
{
    Faction* pFaction = nullptr;
    int weight = 0;
};

} // namespace

void PickRebelFactionAndTransfer(BaseManager& rBase, GameState& rGameState,
                                 const RebelSelectionConfig_t& rConfig, std::mt19937& rRng)
{
    Faction& rOwner = rBase.GetFaction();
    const FactionId_t ownerId = rOwner.GetFactionId();
    const int mapWidth = rGameState.GetWorldMap().GetWidth();

    std::vector<Candidate_t> candidates;
    for (Faction& rFaction : rGameState.Factions())
    {
        if (rFaction.GetFactionId() == ownerId)
        {
            continue;
        }
        // Alive = still holds at least one base (factions are never removed from GameState).
        if (rFaction.GetBaseCount() == 0)
        {
            continue;
        }

        const int weight = RebelJoinWeight_(rFaction, rConfig)
            + DistanceBonus_(rFaction, rBase, rConfig, mapWidth);
        if (weight <= 0)
        {
            continue;
        }
        candidates.push_back(Candidate_t{&rFaction, weight});
    }

    if (candidates.empty())
    {
        std::cerr << "[Rebel] no candidate factions for base " << rBase.GetBaseId() << '\n';
        return;
    }

    int totalWeight = 0;
    for (const Candidate_t& rCand : candidates)
    {
        totalWeight += rCand.weight;
    }

    std::uniform_int_distribution<int> dist(1, totalWeight);
    int roll = dist(rRng);
    Faction* pChosen = candidates.front().pFaction;
    for (const Candidate_t& rCand : candidates)
    {
        roll -= rCand.weight;
        if (roll <= 0)
        {
            pChosen = rCand.pFaction;
            break;
        }
    }

    const BaseId_t baseId = rBase.GetBaseId();
    // Same order conquest and mind control use: the assimilation window is stamped while the
    // base still belongs to the loser, then TransferBaseTo moves it (and resets escalation).
    rBase.GetPopulation().NotifyCaptured(ownerId, pChosen->GetFactionId());
    rOwner.TransferBaseTo(baseId, *pChosen);
}

} // namespace ac
