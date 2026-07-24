#include "game/units/TerraformRules.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/TileEffectsContext.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/base/BaseTypes.h"
#include "game/map/MapUtils.h"
#include "game/map/RiverGeneration.h"
#include "game/map/TerritoryMap.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/units/UnitDomain.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ac
{

namespace
{

bool HasRequiredTech_(const Faction& rFaction, const ImprovementConfig_t& rConfig)
{
    if (rConfig.requiredTech.empty())
    {
        return true;
    }
    return rFaction.GetResearch().HasDiscoveredTech(rConfig.requiredTech);
}

bool IsSeaTile_(const Tile& rTile)
{
    return rTile.GetElevation() < 0;
}

bool DomainAllows_(const Unit& rUnit, const ImprovementConfig_t& rConfig, const Tile& rTile)
{
    const bool bSea = IsSeaTile_(rTile);
    const UnitDomain_t domain = rUnit.GetDomain();

    if (rConfig.id == "KelpFarm" || rConfig.id == "MiningPlatform" || rConfig.id == "TidalHarness")
    {
        return bSea && domain == UnitDomain_t::Sea;
    }

    if (rConfig.terraformResult == TerraformResult_t::RaiseLand
        || rConfig.terraformResult == TerraformResult_t::LowerLand)
    {
        return domain == UnitDomain_t::Sea || domain == UnitDomain_t::Land;
    }

    if (bSea)
    {
        return domain == UnitDomain_t::Sea;
    }
    return domain == UnitDomain_t::Land;
}

bool CanApplyMutation_(const Tile& rTile, const ImprovementConfig_t& rConfig, UnitDomain_t domain)
{
    switch (rConfig.terraformResult)
    {
        case TerraformResult_t::Place:
            return CanBuildImprovement(rTile, rConfig) && !rTile.HasImprovement(rConfig.id);
        case TerraformResult_t::LevelTerrain:
            return rTile.GetRockiness() == Rockiness_t::Rocky
                || rTile.GetRockiness() == Rockiness_t::Rolling;
        case TerraformResult_t::PlantFungus:
            return !rTile.GetHasFungus();
        case TerraformResult_t::RemoveFungus:
            return rTile.GetHasFungus();
        case TerraformResult_t::Aquifer:
            return !rTile.GetHasAquifer() && !IsSeaTile_(rTile);
        case TerraformResult_t::RaiseLand:
            if (domain == UnitDomain_t::Sea)
            {
                return rTile.GetElevation() <= -1000;
            }
            return rTile.GetElevation() < 3500;
        case TerraformResult_t::LowerLand:
            if (domain == UnitDomain_t::Land)
            {
                return rTile.GetElevation() >= 1000;
            }
            return true;
    }
    return false;
}

} // namespace

int QuoteRaiseLowerEnergyCost(const Tile& rTile, FactionId_t factionId, const WorldMap& rWorldMap)
{
    const int elevBand = std::abs(rTile.GetElevation()) / 1000 + 1;
    int nearest = std::numeric_limits<int>::max();
    const int mapWidth = rWorldMap.GetWidth();

    for (const auto& pTile : rWorldMap.GetTiles())
    {
        if (!pTile || !pTile->HasImprovement("Base"))
        {
            continue;
        }
        if (rWorldMap.GetTerritory().GetOwner(*pTile) == factionId)
        {
            nearest = std::min(nearest, ChebyshevDistance(rTile, *pTile, mapWidth));
        }
    }
    if (nearest == std::numeric_limits<int>::max())
    {
        nearest = std::max(rWorldMap.GetWidth(), rWorldMap.GetHeight());
    }
    return elevBand * 8 + nearest * 2;
}

int TerraformEnergyCost(const Unit& rUnit, const ImprovementConfig_t& rConfig,
                        const GameState& rGameState)
{
    if (rConfig.terraformResult == TerraformResult_t::RaiseLand
        || rConfig.terraformResult == TerraformResult_t::LowerLand)
    {
        return QuoteRaiseLowerEnergyCost(rUnit.GetTile(), rUnit.GetFaction().GetFactionId(),
                                         rGameState.GetWorldMap());
    }
    return rConfig.energyCost;
}

bool CanStartTerraform(const Unit& rUnit, const ImprovementConfig_t& rConfig,
                       const GameState& rGameState)
{
    if (!rUnit.GetFlag(RuleFlagId_t::Terraform))
    {
        return false;
    }
    if (rConfig.turnsRequired <= 0)
    {
        return false;
    }
    if (!HasRequiredTech_(rUnit.GetFaction(), rConfig))
    {
        return false;
    }

    const Tile& rTile = rUnit.GetTile();
    if (!DomainAllows_(rUnit, rConfig, rTile))
    {
        return false;
    }
    if (!CanApplyMutation_(rTile, rConfig, rUnit.GetDomain()))
    {
        return false;
    }

    const int cost = TerraformEnergyCost(rUnit, rConfig, rGameState);
    return rUnit.GetFaction().GetEconomy().GetEnergy() >= cost;
}

bool ApplyTerraformResult(Tile& rTile, const ImprovementConfig_t& rConfig,
                          TileEffectsContext& rTileEffects, WorldMap& rWorldMap,
                          const Unit& rFormer)
{
    switch (rConfig.terraformResult)
    {
        case TerraformResult_t::Place:
            if (!CanBuildImprovement(rTile, rConfig) || rTile.HasImprovement(rConfig.id))
            {
                return false;
            }
            rTileEffects.AddImprovementWithEffects(rTile, rConfig.id);
            return true;

        case TerraformResult_t::LevelTerrain:
            if (rTile.GetRockiness() == Rockiness_t::Rocky)
            {
                rTile.SetRockiness(Rockiness_t::Rolling);
                return true;
            }
            if (rTile.GetRockiness() == Rockiness_t::Rolling)
            {
                rTile.SetRockiness(Rockiness_t::Flat);
                return true;
            }
            return false;

        case TerraformResult_t::PlantFungus:
            if (rTile.GetHasFungus())
            {
                return false;
            }
            rTile.SetHasFungus(true);
            return true;

        case TerraformResult_t::RemoveFungus:
            if (!rTile.GetHasFungus())
            {
                return false;
            }
            rTile.SetHasFungus(false);
            return true;

        case TerraformResult_t::Aquifer:
            if (rTile.GetHasAquifer() || IsSeaTile_(rTile))
            {
                return false;
            }
            rTile.SetHasAquifer(true);
            RecomputeRivers(rWorldMap);
            return true;

        case TerraformResult_t::RaiseLand:
        {
            const int newElev = std::min(3500, rTile.GetElevation() + 1000);
            rTile.SetElevation(newElev);
            ForEachTileInChebyshevRadius(rTile, rWorldMap, 1, false,
                [&](Tile* pNeighbor, int /*distance*/)
                {
                    if (!pNeighbor)
                    {
                        return;
                    }
                    if (pNeighbor->GetElevation() < newElev)
                    {
                        pNeighbor->SetElevation(
                            std::min(newElev, pNeighbor->GetElevation() + 1000));
                    }
                });
            RecomputeRivers(rWorldMap);
            (void)rFormer;
            return true;
        }

        case TerraformResult_t::LowerLand:
        {
            if (rFormer.GetDomain() == UnitDomain_t::Land && rTile.GetElevation() < 1000)
            {
                return false;
            }
            rTile.SetElevation(rTile.GetElevation() - 1000);
            RecomputeRivers(rWorldMap);
            return true;
        }
    }
    return false;
}

} // namespace ac
