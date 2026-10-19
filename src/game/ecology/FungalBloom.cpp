#include "game/ecology/FungalBloom.h"

#include "game/ecology/EcologyRules.h"
#include "game/faction/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/faction/FactionConfig.h"
#include "game/units/UnitManager.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementIds.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/natives/EnsureNativeDesign.h"
#include "game/natives/NativeDesign.h"
#include "game/natives/NativeUnitRegistry.h"

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

int PlaceLifeforms_(Faction& rPlanet, const NativeUnitRegistry& rNatives,
                    const std::vector<Tile*>& rNewTiles, int lifeformCount, std::mt19937& rRng,
                    GameState& rGameState)
{
    std::vector<Tile*> habitable;
    for (Tile* pTile : rNewTiles)
    {
        if (!EligibleLifeforms(*pTile).empty())
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
        const std::vector<const NativeUnitConfig_t*> eligible = EligibleLifeforms(rTile);
        std::uniform_int_distribution<size_t> designDist(0, eligible.size() - 1);
        const NativeUnitConfig_t& rConfig = *eligible[designDist(rRng)];
        const NativeDesign* pDesign = EnsureNativeDesign(rPlanet, rNatives, rConfig.id);
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

std::vector<Tile*> SelectBloomTiles_(const Tile& rOrigin, WorldMap& rWorldMap, int tileCount,
                                    std::mt19937& rRng)
{
    BloomCandidates_t candidates = BloomCandidateTiles(rOrigin);
    std::vector<const Tile*> selected;
    if (candidates.pOrigin != nullptr)
    {
        selected.push_back(candidates.pOrigin);
    }
    std::shuffle(candidates.neighbours.begin(), candidates.neighbours.end(), rRng);
    const int neighborSlots = tileCount - static_cast<int>(selected.size());
    if (neighborSlots > 0)
    {
        const int available = static_cast<int>(candidates.neighbours.size());
        const int take = std::min(neighborSlots, available);
        selected.insert(selected.end(), candidates.neighbours.begin(),
                        candidates.neighbours.begin() + take);
    }

    std::vector<Tile*> tiles;
    tiles.reserve(selected.size());
    for (const Tile* pTile : selected)
    {
        Tile* pMutable = rWorldMap.GetTile(pTile->GetX(), pTile->GetY());
        if (pMutable == nullptr)
        {
            throw std::logic_error("ApplyFungalBloom: a bloom tile is off this world");
        }
        tiles.push_back(pMutable);
    }
    return tiles;
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
                                     std::mt19937& rRng, GameState& rGameState)
{
    FungalBloomResult_t result;
    if (tileCount <= 0)
    {
        return result;
    }
    const NativeUnitRegistry* pNatives = rGameState.GetGameData().nativeUnitRegistry.get();
    if (!pNatives)
    {
        throw std::logic_error("ApplyFungalBloom: no native unit registry is available");
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

    const int lifeformCount = RollLifeformCount_(*pNatives, rRng);
    if (lifeformCount <= 0)
    {
        return result;
    }

    result.lifeforms = SpawnLifeforms_(*pNatives, newly, lifeformCount, rRng, rGameState);
    return result;
}

} // namespace ac
