#include "game/worldgen/SupplyPodGeneration.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementIds.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/MapUtils.h"
#include "game/map/OccupantCoexistence.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/supply/SupplyPodIndex.h"
#include "game/units/UnitDomain.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_set>
#include <vector>

namespace ac
{

namespace
{

bool TileMatchesDomain_(const Tile& rTile, UnitDomain_t domain)
{
    const bool bWater = rTile.IsWater();
    return domain == UnitDomain_t::Sea ? bWater : !bWater;
}

std::vector<Tile*> EligibleTiles_(WorldMap& rWorld, const ImprovementConfig_t& rPod,
                                  UnitDomain_t domain)
{
    std::vector<Tile*> candidates;
    for (const std::unique_ptr<Tile>& pTile : rWorld.GetTiles())
    {
        if (!pTile || !TileMatchesDomain_(*pTile, domain))
        {
            continue;
        }
        if (CanBuildImprovement(*pTile, rPod, {}, {}))
        {
            candidates.push_back(pTile.get());
        }
    }
    return candidates;
}

// Candidates are already eligible and unique; shuffle and stamp the first `target`.
int PlaceOnShuffled_(std::vector<Tile*>& rCandidates, const ImprovementConfig_t& rPod,
                     const std::string& rEventGroupId, SupplyPodIndex& rIndex, int target,
                     std::mt19937& rRng)
{
    if (target <= 0 || rCandidates.empty())
    {
        return 0;
    }
    std::shuffle(rCandidates.begin(), rCandidates.end(), rRng);
    const int placeCount = std::min(target, static_cast<int>(rCandidates.size()));
    for (int i = 0; i < placeCount; ++i)
    {
        Tile& rTile = *rCandidates[static_cast<std::size_t>(i)];
        rTile.AddTerrainFeature(rPod);
        rIndex.Stamp(rTile, rEventGroupId);
    }
    return placeCount;
}

int PlaceGlobalForEventGroup_(WorldMap& rWorld, const SupplyPodEventGroupConfig_t& rEventGroup,
                              const ImprovementConfig_t& rPod, SupplyPodIndex& rIndex,
                              std::mt19937& rRng)
{
    const SupplyPodPlacementConfig_t& rPlacement = rEventGroup.placement;
    if (rPlacement.globalFraction <= 0.0)
    {
        return 0;
    }
    std::vector<Tile*> candidates = EligibleTiles_(rWorld, rPod, rPlacement.domain);
    const double share = rPlacement.globalFraction * static_cast<double>(candidates.size());
    const int target = static_cast<int>(std::lround(share));
    return PlaceOnShuffled_(candidates, rPod, rEventGroup.id, rIndex, target, rRng);
}

int PlaceWreckageForEventGroup_(WorldMap& rWorld, const SupplyPodEventGroupConfig_t& rEventGroup,
                                const ImprovementConfig_t& rPod, SupplyPodIndex& rIndex,
                                std::mt19937& rRng)
{
    const SupplyPodPlacementConfig_t& rPlacement = rEventGroup.placement;
    if (rPlacement.wreckageExtra <= 0 || rPlacement.wreckageRadius < 0)
    {
        return 0;
    }
    std::vector<Tile*> nearWreckage;
    std::unordered_set<Tile*> seen;
    for (const std::unique_ptr<Tile>& pTile : rWorld.GetTiles())
    {
        if (!pTile || !pTile->HasTerrainFeature("UnityWreckage"))
        {
            continue;
        }
        ForEachTileInChebyshevRadius(*pTile, rWorld, rPlacement.wreckageRadius,
                                     [&](Tile* pT, int /*distance*/)
                                     {
                                         if (!pT || !TileMatchesDomain_(*pT, rPlacement.domain)
                                             || !CanBuildImprovement(*pT, rPod, {}, {})
                                             || !seen.insert(pT).second)
                                         {
                                             return;
                                         }
                                         nearWreckage.push_back(pT);
                                     });
    }
    return PlaceOnShuffled_(nearWreckage, rPod, rEventGroup.id, rIndex, rPlacement.wreckageExtra,
                            rRng);
}

int PlaceStartForEventGroup_(WorldMap& rWorld, const Tile& rStart,
                             const SupplyPodEventGroupConfig_t& rEventGroup,
                             const ImprovementConfig_t& rPod, SupplyPodIndex& rIndex,
                             std::mt19937& rRng)
{
    const SupplyPodPlacementConfig_t& rPlacement = rEventGroup.placement;
    if (rPlacement.startCount <= 0 || rPlacement.startRadius < 0)
    {
        return 0;
    }
    std::vector<Tile*> candidates;
    ForEachNeighborInChebyshevRadius(rStart, rWorld, rPlacement.startRadius,
                                     [&](Tile* pTile, int /*distance*/)
                                     {
                                         if (pTile && TileMatchesDomain_(*pTile, rPlacement.domain)
                                             && CanBuildImprovement(*pTile, rPod, {}, {}))
                                         {
                                             candidates.push_back(pTile);
                                         }
                                     });
    return PlaceOnShuffled_(candidates, rPod, rEventGroup.id, rIndex, rPlacement.startCount, rRng);
}

} // namespace

int PlaceGlobalSupplyPods(WorldMap& rWorld, const SupplyPodConfig_t& rConfig,
                          const ImprovementRegistry& rImprovementRegistry, SupplyPodIndex& rIndex,
                          std::mt19937& rRng)
{
    const ImprovementConfig_t& rPod =
        rImprovementRegistry.Get(std::string(ImprovementIds::k_SupplyPod));
    int placed = 0;
    for (const SupplyPodEventGroupConfig_t& rEventGroup : rConfig.eventGroups)
    {
        placed += PlaceGlobalForEventGroup_(rWorld, rEventGroup, rPod, rIndex, rRng);
    }
    return placed;
}

int PlaceWreckageSupplyPods(WorldMap& rWorld, const SupplyPodConfig_t& rConfig,
                            const ImprovementRegistry& rImprovementRegistry,
                            SupplyPodIndex& rIndex, std::mt19937& rRng)
{
    const ImprovementConfig_t& rPod =
        rImprovementRegistry.Get(std::string(ImprovementIds::k_SupplyPod));
    int placed = 0;
    for (const SupplyPodEventGroupConfig_t& rEventGroup : rConfig.eventGroups)
    {
        placed += PlaceWreckageForEventGroup_(rWorld, rEventGroup, rPod, rIndex, rRng);
    }
    return placed;
}

int PlaceStartSupplyPods(WorldMap& rWorld, const Tile& rStart, const SupplyPodConfig_t& rConfig,
                         const ImprovementRegistry& rImprovementRegistry, SupplyPodIndex& rIndex,
                         std::mt19937& rRng)
{
    const ImprovementConfig_t& rPod =
        rImprovementRegistry.Get(std::string(ImprovementIds::k_SupplyPod));
    int placed = 0;
    for (const SupplyPodEventGroupConfig_t& rEventGroup : rConfig.eventGroups)
    {
        placed += PlaceStartForEventGroup_(rWorld, rStart, rEventGroup, rPod, rIndex, rRng);
    }
    return placed;
}

} // namespace ac
