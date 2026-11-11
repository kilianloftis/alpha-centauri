#include "game/natives/PlanetFaunaMutations.h"

#include "game/DifficultyConfig.h"
#include "game/GameDataContext.h"
#include "game/GameRulesConfig.h"
#include "game/GameSettings.h"
#include "game/GameState.h"
#include "game/base/BaseManager.h"
#include "game/faction/Faction.h"
#include "game/faction/FactionConfig.h"
#include "game/map/ImprovementIds.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/movement/TransportRules.h"
#include "game/natives/NativeLifeConfig.h"
#include "game/natives/NativeUnitRegistry.h"
#include "game/natives/PlanetFaunaCalculator.h"
#include "game/natives/PlanetFaunaRules.h"
#include "game/units/IDesign.h"
#include "game/units/Military.h"
#include "game/units/Unit.h"
#include "game/units/UnitManager.h"
#include "game/units/UnitOrigin.h"
#include "lib/RandomRoll.h"
#include "lib/WeightedPick.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace ac
{

namespace
{

Faction* FindPlanet_(GameState& rGameState)
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

const IDesign* RequireDesign_(Faction& rPlanet, const std::string& rId)
{
    const IDesign* pDesign = rPlanet.GetMilitary().GetDesign(rId);
    if (!pDesign)
    {
        throw std::logic_error("RunPlanetFauna: design '" + rId
                               + "' is not registered on the native-life faction");
    }
    return pDesign;
}

Unit& SpawnOnTile_(Faction& rPlanet, const IDesign& rDesign, Tile& rTile, GameState& rGameState)
{
    return rPlanet.GetUnitManager().CreateUnit(rGameState.AllocateUnitId(), rDesign,
                                               rGameState.GetWorldMap().GetUnitPositions(), rTile,
                                               UnitOrigin::Gift(nullptr));
}

const NativeUnitConfig_t* WeightedPick_(const std::vector<const NativeUnitConfig_t*>& rCandidates,
                                        std::mt19937& rRng)
{
    const auto picked = PickWeightedIndex(
        rCandidates.size(),
        [&](std::size_t i) { return rCandidates[i]->planet->faunaWeight; }, rRng);
    if (!picked)
    {
        return nullptr;
    }
    return rCandidates[*picked];
}

const NativeUnitConfig_t* PickByRole_(NativeFaunaRole_t role, const Tile* pDomainTile,
                                     std::mt19937& rRng)
{
    return WeightedPick_(FaunaCandidatesForRole(role, pDomainTile), rRng);
}

Tile* PickRandomTile_(WorldMap& rMap, std::mt19937& rRng)
{
    const auto tiles = rMap.GetTiles();
    if (tiles.empty())
    {
        return nullptr;
    }
    return tiles[PickUniformIndex(tiles.size(), rRng)].get();
}

Tile* FindBoatSpawnTile_(WorldMap& rMap, std::mt19937& rRng)
{
    for (int attempt = 0; attempt < 1000; ++attempt)
    {
        Tile* pTile = PickRandomTile_(rMap, rRng);
        if (!pTile)
        {
            return nullptr;
        }
        if (!pTile->IsWater())
        {
            continue;
        }
        if (pTile->HasFeature(ImprovementIds::k_Fungus)
            || rMap.GetUnitsOnTile(*pTile).empty())
        {
            return pTile;
        }
    }
    return nullptr;
}

int NearestBaseDistance_(GameState& rGameState, const Tile& rTile)
{
    const BaseManager* pNearest = rGameState.FindNearestBase(rTile);
    if (!pNearest)
    {
        return std::numeric_limits<int>::max();
    }
    return TabletopDiagonalDistance(rTile, pNearest->GetTile(), rGameState.GetWorldMap().GetWidth());
}

bool HasForeignUnit_(const Tile& rTile, const WorldMap& rMap, FactionId_t planetId)
{
    for (Unit* pUnit : rMap.GetUnitsOnTile(rTile))
    {
        if (pUnit && pUnit->GetFaction().GetFactionId() != planetId)
        {
            return true;
        }
    }
    return false;
}

void SpawnEscortIfAny_(Faction& rPlanet, Tile& rTile, GameState& rGameState, std::mt19937& rRng)
{
    const NativeUnitConfig_t* pEscort = PickByRole_(NativeFaunaRole_t::BoatEscort, &rTile, rRng);
    if (!pEscort)
    {
        return;
    }
    SpawnOnTile_(rPlanet, *RequireDesign_(rPlanet, pEscort->id), rTile, rGameState);
}

void RunBoatSpawn_(Faction& rPlanet, const PlanetFaunaCalculator& rCalc, int faunaForms,
                   int faunaDiff, int turn, bool bEarlyLimit, GameState& rGameState,
                   std::mt19937& rRng)
{
    const int period = rCalc.BoatPeriod(faunaForms);
    if (period <= 0 || turn % period != 0)
    {
        return;
    }
    Tile* pTile = FindBoatSpawnTile_(rGameState.GetWorldMap(), rRng);
    if (!pTile)
    {
        return;
    }

    const NativeUnitConfig_t* pHull = PickByRole_(NativeFaunaRole_t::BoatHull, pTile, rRng);
    if (!pHull)
    {
        return;
    }
    Unit& rHull = SpawnOnTile_(rPlanet, *RequireDesign_(rPlanet, pHull->id), *pTile, rGameState);

    const double cargoScale = ResolvePlanetFaunaCargoScale(rPlanet);
    int cargo =
        PlanetFaunaBoatCargo(turn, rCalc, faunaForms, faunaDiff, cargoScale, bEarlyLimit);
    cargo = std::min(cargo, FreeCargoSlots(rHull));
    while (cargo > 0)
    {
        // No domain filter: land cargo boards the hull; Enter would deny them on water.
        const NativeUnitConfig_t* pPassenger =
            PickByRole_(NativeFaunaRole_t::BoatCargo, nullptr, rRng);
        if (!pPassenger)
        {
            break;
        }
        Unit& rPassenger =
            SpawnOnTile_(rPlanet, *RequireDesign_(rPlanet, pPassenger->id), *pTile, rGameState);
        rPassenger.Transport().EmbarkInto(rHull);
        --cargo;
    }
    SpawnEscortIfAny_(rPlanet, *pTile, rGameState, rRng);
}

struct AmbushResult_t
{
    Tile* pNearBaseFungus = nullptr;
    const BaseManager* pNearBase = nullptr;
};

AmbushResult_t RunAmbushSpawns_(Faction& rPlanet, const NativeLifeConfig_t& rLife,
                                const PlanetFaunaCalculator& rCalc, int faunaForms,
                                GameState& rGameState, std::mt19937& rRng)
{
    AmbushResult_t result;
    WorldMap& rMap = rGameState.GetWorldMap();
    const FactionId_t planetId = rPlanet.GetFactionId();
    const int attempts = rCalc.AmbushAttempts(faunaForms);

    for (int iter = 0; iter < attempts; ++iter)
    {
        Tile* pTile = PickRandomTile_(rMap, rRng);
        if (!pTile || pTile->IsWater() || !pTile->HasFeature(ImprovementIds::k_Fungus))
        {
            continue;
        }
        if (pTile->HasImprovement(ImprovementIds::k_Base))
        {
            continue;
        }

        const int baseDist = NearestBaseDistance_(rGameState, *pTile);
        if (HasForeignUnit_(*pTile, rMap, planetId)
            && (baseDist > 2 || baseDist == std::numeric_limits<int>::max()))
        {
            bool bSpawned = false;
            ForEachNeighborInChebyshevRadius(
                *pTile, rMap, 2, [&](Tile* pNeighbor, int)
                {
                    if (bSpawned || !pNeighbor || pNeighbor->IsWater())
                    {
                        return;
                    }
                    if (!rMap.GetUnitsOnTile(*pNeighbor).empty())
                    {
                        return;
                    }
                    const NativeUnitConfig_t* pPick =
                        PickByRole_(NativeFaunaRole_t::Ambush, pNeighbor, rRng);
                    if (!pPick)
                    {
                        return;
                    }
                    SpawnOnTile_(rPlanet, *RequireDesign_(rPlanet, pPick->id), *pNeighbor,
                                 rGameState);
                    bSpawned = true;
                });
            continue;
        }

        if (baseDist >= rLife.ambushNearBaseMinDist && baseDist <= rLife.ambushNearBaseMaxDist
            && !HasForeignUnit_(*pTile, rMap, planetId))
        {
            const NativeUnitConfig_t* pPick = PickByRole_(NativeFaunaRole_t::Ambush, pTile, rRng);
            if (!pPick)
            {
                continue;
            }
            SpawnOnTile_(rPlanet, *RequireDesign_(rPlanet, pPick->id), *pTile, rGameState);
            result.pNearBaseFungus = pTile;
            result.pNearBase = rGameState.FindNearestBase(*pTile);
        }
    }
    return result;
}

void RunSwarmSpawn_(Faction& rPlanet, const PlanetFaunaCalculator& rCalc, int faunaForms,
                    bool bEarlyLimit, bool bSwarmAllowed, int turn, const AmbushResult_t& rAmbush,
                    GameState& rGameState, std::mt19937& rRng)
{
    if (!bSwarmAllowed || !rAmbush.pNearBaseFungus || !rAmbush.pNearBase)
    {
        return;
    }
    if (!PlanetFaunaSwarmYearReached(turn, rCalc, faunaForms))
    {
        return;
    }

    Tile* pSpawn = nullptr;
    ForEachNeighborInChebyshevRadius(
        *rAmbush.pNearBaseFungus, rGameState.GetWorldMap(), 2,
        [&](Tile* pNeighbor, int)
        {
            if (pSpawn || !pNeighbor)
            {
                return;
            }
            if (!pNeighbor->HasFeature(ImprovementIds::k_Fungus))
            {
                return;
            }
            if (!rGameState.GetWorldMap().GetUnitsOnTile(*pNeighbor).empty())
            {
                return;
            }
            if (!RollPercent(50, rRng))
            {
                return;
            }
            pSpawn = pNeighbor;
        });
    if (!pSpawn)
    {
        pSpawn = rAmbush.pNearBaseFungus;
    }

    const std::vector<const NativeUnitConfig_t*> leadCandidates =
        FaunaCandidatesForRole(NativeFaunaRole_t::SwarmLead, pSpawn);
    int count = std::clamp(faunaForms + 1, 1, 10);
    if (bEarlyLimit)
    {
        count = leadCandidates.empty() ? 1 : 2;
    }
    for (int i = 0; i < count; ++i)
    {
        if (i == 0 && !leadCandidates.empty())
        {
            const NativeUnitConfig_t* pLead = WeightedPick_(leadCandidates, rRng);
            if (pLead)
            {
                SpawnOnTile_(rPlanet, *RequireDesign_(rPlanet, pLead->id), *pSpawn, rGameState);
                continue;
            }
        }
        const NativeUnitConfig_t* pPick = PickByRole_(NativeFaunaRole_t::Swarm, pSpawn, rRng);
        if (!pPick)
        {
            break;
        }
        SpawnOnTile_(rPlanet, *RequireDesign_(rPlanet, pPick->id), *pSpawn, rGameState);
    }
}

} // namespace

void RunPlanetFauna(GameState& rGameState, std::mt19937& rRng)
{
    const GameDataContext& rData = rGameState.GetGameData();
    if (!rData.nativeUnitRegistry)
    {
        throw std::logic_error("RunPlanetFauna: no native unit registry is available");
    }
    if (!rData.nativeLifeConfig)
    {
        throw std::logic_error("RunPlanetFauna: no native life config is available");
    }
    if (!rData.planetFaunaCalculator)
    {
        throw std::logic_error("RunPlanetFauna: no planet fauna calculator is available");
    }
    Faction* pPlanet = FindPlanet_(rGameState);
    if (!pPlanet)
    {
        return;
    }
    if (PlanetFaunaSuppressed())
    {
        return;
    }

    const NativeLifeConfig_t& rLife = *rData.nativeLifeConfig;
    const PlanetFaunaCalculator& rCalc = *rData.planetFaunaCalculator;
    const GameRulesConfig_t& rRules = rGameState.GetSettings().GetGameRules();
    const NativeLifeLevel_t& rNativeLevel = rLife.RequireForSession(rRules.nativeLifeLevelId);
    const DifficultyLevel_t& rDiffLevel =
        rData.difficultyConfig->RequireForSession(rRules.difficultyId);

    const int faunaForms = rNativeLevel.faunaForms;
    const int faunaDiff = rDiffLevel.rules.planetFaunaDiff;
    const int turn = rGameState.GetTurnNumber();
    const bool bEarlyLimit = PlanetFaunaEarlyLimit(turn, rCalc, faunaForms, faunaDiff);

    RunBoatSpawn_(*pPlanet, rCalc, faunaForms, faunaDiff, turn, bEarlyLimit, rGameState, rRng);
    const AmbushResult_t ambush =
        RunAmbushSpawns_(*pPlanet, rLife, rCalc, faunaForms, rGameState, rRng);
    RunSwarmSpawn_(*pPlanet, rCalc, faunaForms, bEarlyLimit,
                   rDiffLevel.rules.planetFaunaSwarmEvent, turn, ambush, rGameState, rRng);
}

} // namespace ac
