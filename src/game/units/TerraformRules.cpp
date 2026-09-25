#include "game/units/TerraformRules.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/TileEffectsContext.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/base/BaseTypes.h"
#include "game/map/ElevationChange.h"
#include "game/map/ImprovementIds.h"
#include "game/map/ImprovementRegistry.h"
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
#include <magic_enum.hpp>
#include <stdexcept>

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
    const bool bSea = rTile.IsWater();
    const UnitDomain_t domain = rUnit.GetDomain();

    if (rConfig.domain == ImprovementDomain_t::Sea)
    {
        return bSea && domain == UnitDomain_t::Sea;
    }
    if (rConfig.domain == ImprovementDomain_t::Land)
    {
        return !bSea && domain == UnitDomain_t::Land;
    }

    // Order-only projects have no occupancy domain. Raise and lower are either former;
    // anything else still has to match the tile the former is standing on.
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

bool ExcludesId_(const ImprovementConfig_t& rConfig, std::string_view id)
{
    return std::any_of(rConfig.excludes.begin(), rConfig.excludes.end(),
                       [&](const std::string& rExcluded) { return rExcluded == id; });
}

const ImprovementConfig_t* FindFeatureConfig_(const Tile& rTile, std::string_view featureId)
{
    for (const ImprovementConfig_t* pFeature : rTile.GetTerrainFeatures())
    {
        if (pFeature && pFeature->id == featureId)
        {
            return pFeature;
        }
    }
    for (const ImprovementConfig_t* pFeature : rTile.GetImprovements())
    {
        if (pFeature && pFeature->id == featureId)
        {
            return pFeature;
        }
    }
    return nullptr;
}

bool FeatureBlocksPlacement_(const Tile& rTile, const ImprovementConfig_t& rCandidate,
                             std::string_view featureId)
{
    if (featureId == rCandidate.id || !rTile.HasFeature(featureId))
    {
        return false;
    }
    if (ExcludesId_(rCandidate, featureId))
    {
        return true;
    }
    const ImprovementConfig_t* pFeature = FindFeatureConfig_(rTile, featureId);
    return pFeature && ExcludesId_(*pFeature, rCandidate.id);
}

// Depth bands come from elevation. A place order does not raise or lower, so these still
// refuse the start. Rockiness, moisture, rivers, aquifers, and improvements are cleared
// when the order finishes.
bool SurfaceBlocksPlacement_(const Tile& rTile, const ImprovementConfig_t& rCandidate)
{
    if (rCandidate.domain == ImprovementDomain_t::Land && !rTile.IsLand())
    {
        return true;
    }
    if (rCandidate.domain == ImprovementDomain_t::Sea && !rTile.IsWater())
    {
        return true;
    }
    return FeatureBlocksPlacement_(rTile, rCandidate, magic_enum::enum_name(TerrainFeature_t::Water))
        || FeatureBlocksPlacement_(rTile, rCandidate,
                                   magic_enum::enum_name(TerrainFeature_t::Ocean))
        || FeatureBlocksPlacement_(rTile, rCandidate,
                                   magic_enum::enum_name(TerrainFeature_t::OceanShelf));
}

void ClearBlockersForPlacement_(Tile& rTile, const ImprovementConfig_t& rPlaced,
                                TileEffectsContext& rTileEffects, WorldMap& rWorldMap)
{
    const std::vector<std::string> displaced = ImprovementsDisplacedBy(rTile, rPlaced);
    for (const std::string& rId : displaced)
    {
        rTileEffects.RemoveImprovementWithEffects(rTile, rId);
    }

    while (FeatureBlocksPlacement_(rTile, rPlaced, magic_enum::enum_name(rTile.GetRockiness())))
    {
        if (rTile.GetRockiness() == Rockiness_t::Flat)
        {
            break;
        }
        rTile.SetRockiness(rTile.GetRockiness() == Rockiness_t::Rocky ? Rockiness_t::Rolling
                                                                       : Rockiness_t::Flat);
    }

    while (FeatureBlocksPlacement_(rTile, rPlaced, magic_enum::enum_name(rTile.GetMoisture())))
    {
        if (rTile.GetMoisture() == Moisture_t::Arid)
        {
            break;
        }
        const Moisture_t next =
            rTile.GetMoisture() == Moisture_t::Wet ? Moisture_t::Moist : Moisture_t::Arid;
        rTile.SetBaseMoisture(next);
        rTile.SetMoisture(next);
    }

    if (FeatureBlocksPlacement_(rTile, rPlaced, magic_enum::enum_name(TerrainFeature_t::Aquifer)))
    {
        rTile.SetHasAquifer(false);
        RecomputeRivers(rWorldMap);
    }
    if (FeatureBlocksPlacement_(rTile, rPlaced, magic_enum::enum_name(TerrainFeature_t::River)))
    {
        rTile.SetHasRiver(false);
    }
}

const ImprovementConfig_t* FeatureIntroducedBy_(const ImprovementConfig_t& rOrder,
                                                const ImprovementRegistry& rImprovements)
{
    if (rOrder.terraformResult != TerraformResult_t::Place)
    {
        return nullptr;
    }
    if (rOrder.placesImprovementId.empty())
    {
        return &rOrder;
    }
    return &rImprovements.Get(rOrder.placesImprovementId);
}

bool CanApplyMutation_(const Tile& rTile, const ImprovementConfig_t& rConfig,
                       const ImprovementConfig_t* pPlaced, UnitDomain_t domain,
                       const ElevationRulesConfig_t& rRules)
{
    switch (rConfig.terraformResult)
    {
        case TerraformResult_t::Place:
            return pPlaced != nullptr
                && !rTile.HasImprovement(pPlaced->id)
                && !SurfaceBlocksPlacement_(rTile, *pPlaced);
        case TerraformResult_t::LevelTerrain:
            return rTile.GetRockiness() == Rockiness_t::Rocky
                || rTile.GetRockiness() == Rockiness_t::Rolling;
        case TerraformResult_t::RemoveFungus:
            return rTile.HasImprovement(ImprovementIds::k_Fungus);
        case TerraformResult_t::Aquifer:
            return !rTile.GetHasAquifer() && !IsSeaTile_(rTile);
        case TerraformResult_t::RaiseLand:
            if (domain == UnitDomain_t::Sea)
            {
                return rTile.GetElevation() <= -rRules.referenceLevelMeters;
            }
            return rTile.GetElevation() < rRules.maxElevationMeters;
        case TerraformResult_t::LowerLand:
            if (domain == UnitDomain_t::Land)
            {
                return rTile.GetElevation() >= rRules.referenceLevelMeters;
            }
            // TODO: SMAC's floor for sea-former lowering is unknown; Planet's own floor is the
            // only limit we can state.
            return rTile.GetElevation() - rRules.referenceLevelMeters >= rRules.minElevationMeters;
    }
    return false;
}

} // namespace

std::vector<std::string> ImprovementsDestroyedByTerraform(const Tile& rTile,
                                                         const ImprovementConfig_t& rOrder,
                                                         const ImprovementRegistry& rImprovements)
{
    const ImprovementConfig_t* pIntroduced = FeatureIntroducedBy_(rOrder, rImprovements);
    if (!pIntroduced)
    {
        return {};
    }
    return ImprovementsDisplacedBy(rTile, *pIntroduced);
}

int QuoteRaiseLowerEnergyCost(const Tile& rTile, FactionId_t factionId, const WorldMap& rWorldMap,
                              const ElevationRulesConfig_t& rRules)
{
    if (rRules.referenceLevelMeters <= 0)
    {
        throw std::logic_error("QuoteRaiseLowerEnergyCost: reference_level_meters must be > 0");
    }
    const int elevBand = std::abs(rTile.GetElevation()) / rRules.referenceLevelMeters + 1;
    int nearest = std::numeric_limits<int>::max();
    const int mapWidth = rWorldMap.GetWidth();

    for (const auto& pTile : rWorldMap.GetTiles())
    {
        if (!pTile || !pTile->HasImprovement(ImprovementIds::k_Base))
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
                        const GameState& rGameState, const ElevationRulesConfig_t& rRules)
{
    if (rConfig.terraformResult == TerraformResult_t::RaiseLand
        || rConfig.terraformResult == TerraformResult_t::LowerLand)
    {
        return QuoteRaiseLowerEnergyCost(rUnit.GetTile(), rUnit.GetFaction().GetFactionId(),
                                         rGameState.GetWorldMap(), rRules);
    }
    return rConfig.energyCost;
}

bool CanStartTerraform(const Unit& rUnit, const ImprovementConfig_t& rConfig,
                       const GameState& rGameState, const ElevationRulesConfig_t& rRules)
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
    const ImprovementConfig_t* pPlaced =
        FeatureIntroducedBy_(rConfig, rGameState.GetTileEffects().GetImprovements());
    if (!CanApplyMutation_(rTile, rConfig, pPlaced, rUnit.GetDomain(), rRules))
    {
        return false;
    }

    const int cost = TerraformEnergyCost(rUnit, rConfig, rGameState, rRules);
    return rUnit.GetFaction().GetEconomy().CanAfford(cost);
}

bool ApplyTerraformResult(Tile& rTile, const ImprovementConfig_t& rConfig,
                          TileEffectsContext& rTileEffects, WorldMap& rWorldMap,
                          const Unit& rFormer, std::mt19937& rRng,
                          const ElevationRulesConfig_t& rRules, IUnitOrderWorld* pWorld)
{
    switch (rConfig.terraformResult)
    {
        case TerraformResult_t::Place:
        {
            const ImprovementConfig_t* pPlaced =
                FeatureIntroducedBy_(rConfig, rTileEffects.GetImprovements());
            if (!pPlaced || SurfaceBlocksPlacement_(rTile, *pPlaced))
            {
                return false;
            }
            if (rTile.HasImprovement(pPlaced->id))
            {
                return true;
            }
            ClearBlockersForPlacement_(rTile, *pPlaced, rTileEffects, rWorldMap);
            if (SurfaceBlocksPlacement_(rTile, *pPlaced)
                || RemainingFeaturesBlockPlacement(rTile, *pPlaced, {}))
            {
                return false;
            }
            rTileEffects.AddImprovementWithEffects(rTile, pPlaced->id);
            return rTile.HasImprovement(pPlaced->id);
        }

        case TerraformResult_t::LevelTerrain:
            if (rTile.GetRockiness() == Rockiness_t::Rocky)
            {
                rTile.SetRockiness(Rockiness_t::Rolling);
            }
            else if (rTile.GetRockiness() == Rockiness_t::Rolling)
            {
                rTile.SetRockiness(Rockiness_t::Flat);
            }
            else
            {
                return false;
            }
            return true;

        case TerraformResult_t::RemoveFungus:
            if (!rTile.HasImprovement(ImprovementIds::k_Fungus))
            {
                return false;
            }
            rTileEffects.RemoveImprovementWithEffects(rTile, std::string(ImprovementIds::k_Fungus));
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
            const int roll = RollLevelMeters(rRng, rRules);
            return ApplyElevationDelta(rTile, rWorldMap, roll, rRules, rRules.minElevationMeters,
                                       rRules.maxElevationMeters, pWorld ? &rTileEffects : nullptr,
                                       pWorld);
        }

        case TerraformResult_t::LowerLand:
        {
            // A roll deeper than the floor lowers to the floor rather than failing. Refusing
            // here would keep the energy CanStartTerraform already charged and the turns the
            // order already spent, and whether it happens is a die roll the player never saw.
            const int roll = RollLevelMeters(rRng, rRules);
            const int floor = rFormer.GetDomain() == UnitDomain_t::Land
                                  ? rRules.oceanLevelMeters
                                  : rRules.minElevationMeters;
            return ApplyElevationDelta(rTile, rWorldMap, -roll, rRules, floor,
                                       rRules.maxElevationMeters, pWorld ? &rTileEffects : nullptr,
                                       pWorld);
        }
    }
    return false;
}

} // namespace ac
