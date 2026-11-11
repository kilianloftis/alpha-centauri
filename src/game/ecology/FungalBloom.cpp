#include "game/ecology/FungalBloom.h"

#include "game/CurrentGameState.h"
#include "game/ecology/EcologyRules.h"
#include "game/faction/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/faction/FactionConfig.h"
#include "game/units/UnitManager.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementIds.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/natives/NativeLifeConfig.h"
#include "game/natives/NativeUnitRegistry.h"
#include "game/units/Military.h"
#include "lib/Rational.h"

#include <algorithm>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{
namespace
{

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

std::vector<Tile*> HabitableTiles_(const std::vector<Tile*>& rTiles,
                                   const NativeUnitRegistry& rNatives)
{
    std::vector<Tile*> habitable;
    for (Tile* pTile : rTiles)
    {
        if (!EligibleLifeforms(*pTile, rNatives).empty())
        {
            habitable.push_back(pTile);
        }
    }
    return habitable;
}

// One lifeform drawn from those the tile can host.
void SpawnRandomLifeform_(Faction& rPlanet, const NativeUnitRegistry& rNatives, Tile& rTile,
                          std::mt19937& rRng, GameState& rGameState)
{
    const std::vector<const NativeUnitConfig_t*> eligible = EligibleLifeforms(rTile, rNatives);
    std::uniform_int_distribution<size_t> designDist(0, eligible.size() - 1);
    const NativeUnitConfig_t& rConfig = *eligible[designDist(rRng)];
    const IDesign* pDesign = rPlanet.GetMilitary().GetDesign(rConfig.id);
    if (!pDesign)
    {
        throw std::logic_error("ApplyFungalBloom: native design '" + rConfig.id
                               + "' is not registered on the native-life faction");
    }
    rPlanet.GetUnitManager().CreateUnit(
        rGameState.AllocateUnitId(), *pDesign, rGameState.GetWorldMap().GetUnitPositions(),
        rTile, UnitOrigin::Gift(nullptr));
}

int PlaceLifeforms_(Faction& rPlanet, const NativeUnitRegistry& rNatives,
                    const std::vector<Tile*>& rNewTiles, int lifeformCount, std::mt19937& rRng,
                    GameState& rGameState)
{
    const std::vector<Tile*> habitable = HabitableTiles_(rNewTiles, rNatives);
    if (habitable.empty())
    {
        return 0;
    }
    std::uniform_int_distribution<size_t> tileDist(0, habitable.size() - 1);
    for (int n = 0; n < lifeformCount; ++n)
    {
        Tile& rTile = *habitable[tileDist(rRng)];
        SpawnRandomLifeform_(rPlanet, rNatives, rTile, rRng, rGameState);
    }
    return lifeformCount;
}

int SpawnLifeforms_(const NativeUnitRegistry& rNatives, const std::vector<Tile*>& rNewTiles,
                    int lifeformCount, std::mt19937& rRng, GameState& rGameState)
{
    Faction* pPlanet = FindNativeLifeFaction_(rGameState);
    if (!pPlanet)
    {
        throw std::logic_error("ApplyFungalBloom: no native-life faction in the session");
    }
    return PlaceLifeforms_(*pPlanet, rNatives, rNewTiles, lifeformCount, rRng, rGameState);
}

std::vector<Tile*> FungusTiles_(const std::vector<Tile*>& rSelected,
                               const ImprovementConfig_t& rFungus)
{
    std::vector<Tile*> newly;
    for (Tile* pTile : rSelected)
    {
        if (pTile->HasTerrainFeature(ImprovementIds::k_Fungus))
        {
            continue;
        }
        pTile->AddTerrainFeature(rFungus);
        newly.push_back(pTile);
    }
    return newly;
}

int RollLifeformCount_(const NativeLifeConfig_t& rLife, std::mt19937& rRng)
{
    if (rLife.fungalBloomNativeLifeformsMin < 0
        || rLife.fungalBloomNativeLifeformsMax < rLife.fungalBloomNativeLifeformsMin)
    {
        throw std::logic_error("ApplyFungalBloom: native lifeform range is invalid");
    }
    std::uniform_int_distribution<int> countDist(rLife.fungalBloomNativeLifeformsMin,
                                                 rLife.fungalBloomNativeLifeformsMax);
    return countDist(rRng);
}

FungalBloomResult_t ApplySelectedBloom_(WorldMap& rWorldMap, std::vector<Tile*> selected,
                                        std::mt19937& rRng, GameState& rGameState)
{
    RequireCurrentGameState(rGameState);
    RequireCurrentWorldMap(rWorldMap);
    FungalBloomResult_t result;
    if (selected.empty())
    {
        return result;
    }
    const NativeUnitRegistry* pNatives = rGameState.GetGameData().nativeUnitRegistry.get();
    if (!pNatives)
    {
        throw std::logic_error("ApplyFungalBloom: no native unit registry is available");
    }
    const NativeLifeConfig_t* pLife = rGameState.GetGameData().nativeLifeConfig.get();
    if (!pLife)
    {
        throw std::logic_error("ApplyFungalBloom: no native life config is available");
    }

    const ImprovementConfig_t& rFungus =
        rGameState.GetTileEffects().GetImprovements().Get(std::string(ImprovementIds::k_Fungus));
    const std::vector<Tile*> newly = FungusTiles_(selected, rFungus);
    result.tilesFungused = static_cast<int>(newly.size());
    if (newly.empty())
    {
        return result;
    }

    const int lifeformCount = RollLifeformCount_(*pLife, rRng);
    if (lifeformCount <= 0)
    {
        return result;
    }

    result.lifeforms = SpawnLifeforms_(*pNatives, newly, lifeformCount, rRng, rGameState);
    return result;
}

} // namespace

FungalBloomResult_t ApplyFungalBloom(Tile& rOrigin, WorldMap& rWorldMap, int tileCount,
                                     std::mt19937& rRng, GameState& rGameState)
{
    std::vector<Tile*> selected =
        SelectTilesByRingQuota(rOrigin, rWorldMap, /*radius=*/1, tileCount, IsBloomCandidate, rRng);
    return ApplySelectedBloom_(rWorldMap, std::move(selected), rRng, rGameState);
}

FungalBloomResult_t ApplyFungalBloom(Tile& rOrigin, WorldMap& rWorldMap,
                                     std::span<const Rational_t> rChances, std::mt19937& rRng,
                                     GameState& rGameState)
{
    std::vector<Tile*> selected =
        SelectTilesByRingChance(rOrigin, rWorldMap, rChances, IsBloomCandidate, rRng);
    return ApplySelectedBloom_(rWorldMap, std::move(selected), rRng, rGameState);
}

} // namespace ac
