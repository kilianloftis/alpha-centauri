#include "game/faction/FactionVisibleMap.h"

#include "game/Faction.h"
#include "game/faction/FactionExploredMap.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/MapUtils.h"
#include "game/map/TerritoryMap.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"

#include <stdexcept>

namespace ac
{

namespace
{

// A base sees a Chebyshev radius-2 square (own tile plus two rings).
constexpr int k_BaseVisionRadius = 2;

} // namespace

void FactionVisibleMap::RevealAround_(const Tile& rOrigin, int radius, const WorldMap& rWorldMap,
                                      FactionExploredMap& rExplored)
{
    if (radius < 0)
    {
        return;
    }

    ForEachTileInChebyshevRadius(rOrigin, rWorldMap, radius, /*includeOrigin=*/true,
        [this, &rExplored](const Tile* pTile, int /*distance*/)
        {
            Mark_(*pTile);
            rExplored.Mark(*pTile);
        });
}

void FactionVisibleMap::RebuildFromSources(const Faction& rFaction, const WorldMap& rWorldMap,
                                           FactionExploredMap& rExplored)
{
    if (!IsSized())
    {
        throw std::runtime_error("FactionVisibleMap::RebuildFromSources: map is unsized");
    }

    ClearAll();

    for (const Unit& rUnit : rFaction.GetUnitManager().Units())
    {
        RevealAround_(rUnit.GetTile(), rUnit.GetStat(StatId_t::Vision), rWorldMap, rExplored);
    }

    for (const BaseManager& rBase : rFaction.Bases())
    {
        RevealAround_(rBase.GetTile(), k_BaseVisionRadius, rWorldMap, rExplored);
    }

    // Vision ThisTile effects on improvements (e.g. Sensor): territory-owned improvements
    // only grant sight to the territory owner.
    const FactionId_t factionId = rFaction.GetFactionId();
    const TerritoryMap& rTerritory = rWorldMap.GetTerritory();
    for (int y = 0; y < rWorldMap.GetHeight(); ++y)
    {
        for (int x = 0; x < rWorldMap.GetWidth(); ++x)
        {
            const Tile* pTile = rWorldMap.GetTile(x, y);
            if (!pTile)
            {
                continue;
            }
            for (const ImprovementConfig_t* pImprovement : pTile->GetImprovements())
            {
                if (!pImprovement)
                {
                    continue;
                }
                const int sight = pImprovement->visionRadius;
                if (sight <= 0)
                {
                    continue;
                }
                if (pImprovement->ownedByTerritory
                    && rTerritory.GetOwner(*pTile) != factionId)
                {
                    continue;
                }
                RevealAround_(*pTile, sight, rWorldMap, rExplored);
            }
        }
    }
}

} // namespace ac
