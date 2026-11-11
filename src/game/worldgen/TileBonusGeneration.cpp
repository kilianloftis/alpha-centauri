#include "game/worldgen/TileBonusGeneration.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/OccupantCoexistence.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "lib/WeightedPick.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ac
{

namespace
{

bool CanPlaceBonus_(const Tile& rTile, const ImprovementConfig_t& rBonus)
{
    return !rTile.HasTerrainFeature(rBonus.id) && CanBuildImprovement(rTile, rBonus, {}, {});
}

bool TileAcceptsBonus_(const Tile& rTile, const std::vector<const ImprovementConfig_t*>& rBonuses)
{
    for (const ImprovementConfig_t* pBonus : rBonuses)
    {
        if (pBonus && CanPlaceBonus_(rTile, *pBonus))
        {
            return true;
        }
    }
    return false;
}

const ImprovementConfig_t* PickWeightedBonus_(
    const Tile& rTile,
    const std::vector<const ImprovementConfig_t*>& rBonuses,
    std::mt19937& rRng)
{
    std::vector<const ImprovementConfig_t*> eligible;
    eligible.reserve(rBonuses.size());

    for (const ImprovementConfig_t* pBonus : rBonuses)
    {
        if (pBonus && CanPlaceBonus_(rTile, *pBonus))
        {
            eligible.push_back(pBonus);
        }
    }

    const auto picked = PickWeightedIndex(
        eligible.size(), [&](std::size_t i) { return eligible[i]->frequency; }, rRng);
    if (!picked)
    {
        return nullptr;
    }
    return eligible[*picked];
}

// Optional terrain with a placement weight.
std::vector<const ImprovementConfig_t*> TerrainBonuses_(const ImprovementRegistry& rOccupants)
{
    std::vector<const ImprovementConfig_t*> bonuses;
    for (const ImprovementConfig_t& rConfig : rOccupants.GetAll())
    {
        if (rConfig.placement == OccupantPlacement_t::Terrain && rConfig.frequency > 0)
        {
            bonuses.push_back(&rConfig);
        }
    }
    return bonuses;
}

std::vector<Tile*> BonusCandidates_(WorldMap& rWorld,
                                    const std::vector<const ImprovementConfig_t*>& rBonuses)
{
    std::vector<Tile*> candidates;
    for (auto& pTile : rWorld.GetTiles())
    {
        if (pTile && TileAcceptsBonus_(*pTile, rBonuses))
        {
            candidates.push_back(pTile.get());
        }
    }
    return candidates;
}

} // namespace

int PlaceTileBonuses(WorldMap& rWorld,
                     const TileBonusDecorationConfig_t& rConfig,
                     const ImprovementRegistry& rOccupants,
                     std::mt19937& rRng)
{
    if (rConfig.fraction <= 0.0f)
    {
        return 0;
    }

    const std::vector<const ImprovementConfig_t*> bonuses = TerrainBonuses_(rOccupants);
    if (bonuses.empty())
    {
        return 0;
    }
    std::vector<Tile*> candidates = BonusCandidates_(rWorld, bonuses);
    if (candidates.empty())
    {
        return 0;
    }

    const float share = rConfig.fraction * static_cast<float>(candidates.size());
    const int target = static_cast<int>(std::lround(share));
    if (target <= 0)
    {
        return 0;
    }

    std::shuffle(candidates.begin(), candidates.end(), rRng);

    int placed = 0;
    for (Tile* pTile : candidates)
    {
        if (placed >= target)
        {
            break;
        }
        if (!pTile)
        {
            continue;
        }

        const ImprovementConfig_t* pBonus = PickWeightedBonus_(*pTile, bonuses, rRng);
        if (!pBonus)
        {
            continue;
        }

        pTile->AddTerrainFeature(*pBonus);
        ++placed;
    }

    return placed;
}

} // namespace ac
