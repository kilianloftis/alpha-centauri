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
#include "game/effects/BonusEffect.h"
#include "game/effects/EffectEnums.h"

#include <algorithm>
#include <variant>

namespace ac
{

namespace
{

// A base sees a Chebyshev radius-2 square (own tile plus two rings).
constexpr int k_BaseVisionRadius = 2;

// Sight radius from ThisTile Vision effects on an improvement. Prefer the effect's own
// radius (Sensor declares "radius": 2 on the Vision entry); fall back to resolved amount
// so amount-only configs still work like unit Vision.
int SightRadiusFromImprovement_(const ImprovementConfig_t& rConfig)
{
    int sight = 0;
    for (const EffectConfig_t& rEffect : rConfig.effects)
    {
        if (rEffect.scope != EffectScope_t::ThisTile)
        {
            continue;
        }
        if (rEffect.persistence == EffectPersistence_t::Instantaneous)
        {
            continue;
        }
        const StatModifierEffect_t* pMod = std::get_if<StatModifierEffect_t>(&rEffect.effect);
        if (!pMod || pMod->stat != StatId_t::Vision)
        {
            continue;
        }

        int range = rEffect.radius;
        if (range <= 0)
        {
            ActiveEffect_t active;
            active.config = &rEffect;
            active.sourceId = rConfig.id;
            range = static_cast<int>(
                ResolveStatModifiers(std::vector<ActiveEffect_t>{active}, SeedFor(StatId_t::Vision))
                    .total);
        }
        sight = std::max(sight, range);
    }
    return sight;
}

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
        return;
    }

    ClearAll();

    for (const Unit& rUnit : rFaction.GetUnitManager().Units())
    {
        RevealAround_(rUnit.GetTile(), rUnit.GetVision(), rWorldMap, rExplored);
    }

    for (const BaseManager& rBase : rFaction.Bases())
    {
        const Tile* pBaseTile = rWorldMap.GetTile(rBase.GetX(), rBase.GetY());
        if (pBaseTile)
        {
            RevealAround_(*pBaseTile, k_BaseVisionRadius, rWorldMap, rExplored);
        }
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
                const int sight = SightRadiusFromImprovement_(*pImprovement);
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
