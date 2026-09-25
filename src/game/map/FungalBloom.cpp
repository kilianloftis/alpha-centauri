#include "game/map/FungalBloom.h"

#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/faction/UnitManager.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementIds.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/BaseConquestRules.h"
#include "game/units/EnsureNativeDesign.h"
#include "game/units/NativeDesign.h"
#include "game/units/NativeUnitRegistry.h"
#include "game/units/UnitDomain.h"

#include <algorithm>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{
namespace
{

bool DomainCanHold_(UnitDomain_t domain, const Tile& rTile)
{
    switch (domain)
    {
        case UnitDomain_t::Land:
            return !rTile.IsWater();
        case UnitDomain_t::Sea:
            return rTile.IsWater();
        case UnitDomain_t::Air:
        case UnitDomain_t::Orbital:
            return true;
    }
    return false;
}

Faction* FindNativeLifeFaction_(GameState& rGameState)
{
    for (Faction& rFaction : rGameState.Factions())
    {
        if (IsNativeLifeFaction(rFaction.GetDefinition().identity.species))
        {
            return &rFaction;
        }
    }
    return nullptr;
}

std::vector<const NativeUnitConfig_t*> EligibleNatives_(const NativeUnitRegistry& rRegistry,
                                                       const Tile& rTile)
{
    std::vector<const NativeUnitConfig_t*> eligible;
    for (const NativeUnitConfig_t& rConfig : rRegistry.GetAll())
    {
        const NativeDesign design(rConfig);
        if (!design.IsNativeLife() || !DomainCanHold_(design.GetDomain(), rTile))
        {
            continue;
        }
        eligible.push_back(&rConfig);
    }
    return eligible;
}

int PlaceLifeforms_(Faction& rPlanet, const std::vector<Tile*>& rNewTiles, int lifeformCount,
                    std::mt19937& rRng, GameState& rGameState)
{
    const NativeUnitRegistry* pRegistry = rPlanet.GetDataContext().nativeUnitRegistry.get();
    if (!pRegistry)
    {
        throw std::logic_error("ApplyFungalBloom: no native unit registry is available");
    }

    std::vector<Tile*> habitable;
    for (Tile* pTile : rNewTiles)
    {
        if (!EligibleNatives_(*pRegistry, *pTile).empty())
        {
            habitable.push_back(pTile);
        }
    }
    if (habitable.empty())
    {
        return 0;
    }

    int spawned = 0;
    std::uniform_int_distribution<size_t> tileDist(0, habitable.size() - 1);
    for (int n = 0; n < lifeformCount; ++n)
    {
        Tile& rTile = *habitable[tileDist(rRng)];
        const std::vector<const NativeUnitConfig_t*> eligible = EligibleNatives_(*pRegistry, rTile);
        std::uniform_int_distribution<size_t> designDist(0, eligible.size() - 1);
        const NativeUnitConfig_t& rConfig = *eligible[designDist(rRng)];
        const NativeDesign* pDesign = EnsureNativeDesign(rPlanet, rPlanet.GetDataContext(),
                                                         rConfig.id);
        if (!pDesign)
        {
            throw std::logic_error("ApplyFungalBloom: native design '" + rConfig.id
                                   + "' could not be registered");
        }
        rPlanet.GetUnitManager().CreateUnit(
            rGameState.AllocateUnitId(), *pDesign, rGameState.GetWorldMap().GetUnitPositions(),
            rTile, /*pHomeBase=*/nullptr, std::optional<BaseManager*>{nullptr});
        ++spawned;
    }
    return spawned;
}

int SpawnLifeforms_(const std::vector<Tile*>& rNewTiles, int lifeformCount, std::mt19937& rRng,
                    GameState& rGameState)
{
    Faction* pPlanet = FindNativeLifeFaction_(rGameState);
    if (!pPlanet)
    {
        throw std::logic_error("ApplyFungalBloom: no native-life faction in the session");
    }
    return PlaceLifeforms_(*pPlanet, rNewTiles, lifeformCount, rRng, rGameState);
}

std::vector<Tile*> SelectBloomTiles_(Tile& rOrigin, WorldMap& rWorldMap, int tileCount,
                                    std::mt19937& rRng)
{
    std::vector<Tile*> selected;
    if (!rOrigin.HasImprovement(ImprovementIds::k_Base))
    {
        selected.push_back(&rOrigin);
    }

    std::vector<Tile*> neighbors;
    ForEachTileInChebyshevRadius(rOrigin, rWorldMap, /*radius=*/1, /*includeOrigin=*/false,
        [&](Tile* pTile, int distance)
        {
            if (distance != 1 || !pTile)
            {
                return;
            }
            if (pTile->HasImprovement(ImprovementIds::k_Base)
                || pTile->HasImprovement(ImprovementIds::k_Fungus))
            {
                return;
            }
            neighbors.push_back(pTile);
        });

    std::shuffle(neighbors.begin(), neighbors.end(), rRng);
    const int neighborSlots = tileCount - static_cast<int>(selected.size());
    if (neighborSlots > 0)
    {
        const int take = std::min(neighborSlots, static_cast<int>(neighbors.size()));
        selected.insert(selected.end(), neighbors.begin(), neighbors.begin() + take);
    }
    return selected;
}

std::vector<Tile*> FungusTiles_(const std::vector<Tile*>& rSelected,
                               const ImprovementConfig_t& rFungus)
{
    std::vector<Tile*> newly;
    for (Tile* pTile : rSelected)
    {
        if (!pTile || pTile->HasImprovement(ImprovementIds::k_Fungus))
        {
            continue;
        }
        pTile->AddImprovement(rFungus);
        newly.push_back(pTile);
    }
    return newly;
}

int RollLifeformCount_(const NativeUnitRegistry& rNatives, std::mt19937& rRng)
{
    const NativeLifeConfig_t& rLife = rNatives.FungalBloomLifeforms();
    if (rLife.fungalBloomNativeLifeformsMin < 0
        || rLife.fungalBloomNativeLifeformsMax < rLife.fungalBloomNativeLifeformsMin)
    {
        throw std::logic_error("ApplyFungalBloom: native lifeform range is invalid");
    }
    std::uniform_int_distribution<int> countDist(rLife.fungalBloomNativeLifeformsMin,
                                                 rLife.fungalBloomNativeLifeformsMax);
    return countDist(rRng);
}

} // namespace

FungalBloomResult_t ApplyFungalBloom(Tile& rOrigin, WorldMap& rWorldMap, int tileCount,
                                     std::mt19937& rRng, GameState& rGameState,
                                     const NativeUnitRegistry& rNatives)
{
    FungalBloomResult_t result;
    if (tileCount <= 0)
    {
        return result;
    }

    const ImprovementConfig_t& rFungus =
        rGameState.GetTileEffects().GetImprovements().Get(std::string(ImprovementIds::k_Fungus));
    const std::vector<Tile*> newly = FungusTiles_(
        SelectBloomTiles_(rOrigin, rWorldMap, tileCount, rRng), rFungus);
    result.tilesFungused = static_cast<int>(newly.size());
    if (newly.empty())
    {
        return result;
    }

    const int lifeformCount = RollLifeformCount_(rNatives, rRng);
    if (lifeformCount <= 0)
    {
        return result;
    }

    result.lifeforms = SpawnLifeforms_(newly, lifeformCount, rRng, rGameState);
    return result;
}

} // namespace ac
