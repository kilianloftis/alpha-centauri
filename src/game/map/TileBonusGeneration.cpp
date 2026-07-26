#include "game/map/TileBonusGeneration.h"

#include "game/map/ImprovementConfigParser.h"
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

// True if an existing feature on the tile lists rBonusId in its excludes list.
bool TileExcludesBonus_(const Tile& rTile, const std::string& rBonusId)
{
    for (const ImprovementConfig_t* pFeature : rTile.GetImprovements())
    {
        if (!pFeature)
        {
            continue;
        }
        for (const std::string& rExcluded : pFeature->excludes)
        {
            if (rExcluded == rBonusId)
            {
                return true;
            }
        }
    }
    for (const ImprovementConfig_t* pFeature : rTile.GetTerrainFeatures())
    {
        if (!pFeature)
        {
            continue;
        }
        for (const std::string& rExcluded : pFeature->excludes)
        {
            if (rExcluded == rBonusId)
            {
                return true;
            }
        }
    }
    return false;
}

bool CanPlaceBonus_(const Tile& rTile, const ImprovementConfig_t& rBonus)
{
    if (!rTile.IsLand() || rTile.HasImprovement(rBonus.id))
    {
        return false;
    }
    if (!CanBuildImprovement(rTile, rBonus))
    {
        return false;
    }
    return !TileExcludesBonus_(rTile, rBonus.id);
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
                     const ImprovementRegistry& rImprovements,
                     std::mt19937& rRng)
{
    if (rConfig.landFraction <= 0.0f)
    {
        return 0;
    }

    std::vector<const ImprovementConfig_t*> bonuses;
    for (const ImprovementConfig_t& rConfigEntry : rImprovements.GetAll())
    {
        if (rConfigEntry.frequency > 0)
        {
            bonuses.push_back(&rConfigEntry);
        }
    }
    if (bonuses.empty())
    {
        return 0;
    }

    std::vector<Tile*> landTiles;
    for (auto& pTile : rWorld.GetTiles())
    {
        if (pTile && pTile->IsLand())
        {
            landTiles.push_back(pTile.get());
        }
    }
    if (landTiles.empty())
    {
        return 0;
    }

    const int target = static_cast<int>(std::lround(
        rConfig.landFraction * static_cast<float>(landTiles.size())));
    if (target <= 0)
    {
        return 0;
    }

    std::shuffle(landTiles.begin(), landTiles.end(), rRng);

    int placed = 0;
    for (Tile* pTile : landTiles)
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

        pTile->AddImprovement(*pBonus);
        ++placed;
    }

    return placed;
}

} // namespace ac
