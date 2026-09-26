#include "game/map/TileBonusGeneration.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/OccupantCoexistence.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace ac
{

namespace
{

bool CanPlaceBonus_(const Tile& rTile, const ImprovementConfig_t& rBonus)
{
    return !rTile.HasTerrainFeature(rBonus.id) && CanBuildImprovement(rTile, rBonus);
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
    int totalWeight = 0;
    std::vector<const ImprovementConfig_t*> eligible;
    eligible.reserve(rBonuses.size());

    for (const ImprovementConfig_t* pBonus : rBonuses)
    {
        if (pBonus && CanPlaceBonus_(rTile, *pBonus))
        {
            eligible.push_back(pBonus);
            totalWeight += pBonus->frequency;
        }
    }
    if (eligible.empty() || totalWeight <= 0)
    {
        return nullptr;
    }

    std::uniform_int_distribution<int> pick(1, totalWeight);
    int roll = pick(rRng);
    for (const ImprovementConfig_t* pBonus : eligible)
    {
        roll -= pBonus->frequency;
        if (roll <= 0)
        {
            return pBonus;
        }
    }
    return eligible.back();
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

    std::vector<const ImprovementConfig_t*> bonuses;
    for (const ImprovementConfig_t& rConfigEntry : rOccupants.GetAll())
    {
        if (rConfigEntry.placement == OccupantPlacement_t::Terrain && rConfigEntry.frequency > 0)
        {
            bonuses.push_back(&rConfigEntry);
        }
    }
    if (bonuses.empty())
    {
        return 0;
    }

    std::vector<Tile*> candidates;
    for (auto& pTile : rWorld.GetTiles())
    {
        if (pTile && TileAcceptsBonus_(*pTile, bonuses))
        {
            candidates.push_back(pTile.get());
        }
    }
    if (candidates.empty())
    {
        return 0;
    }

    const int target = static_cast<int>(std::lround(
        rConfig.fraction * static_cast<float>(candidates.size())));
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
