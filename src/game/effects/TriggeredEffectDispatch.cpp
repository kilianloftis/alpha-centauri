#include "game/effects/TriggeredEffectDispatch.h"

#include "game/CurrentGameState.h"
#include "game/faction/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/atrocities/AtrocityRules.h"
#include "game/buildings/BuildingConfig.h"
#include "game/effects/ActiveEffect.h"
#include "game/diplomacy/InfiltrationRules.h"
#include "game/effects/TileEffectsContext.h"
#include "game/effects/TriggeredEffectRules.h"
#include "game/diplomacy/DiplomaticPermissionRules.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/movement/TransportRules.h"
#include "game/economy/EconomyManager.h"
#include "game/base/RebelMutations.h"
#include "game/research/ResearchManager.h"
#include "game/research/ResearchRules.h"
#include "game/units/UnitManager.h"
#include "game/base/BaseManager.h"
#include "game/buildings/BuildingMutations.h"
#include "game/buildings/BuildingManager.h"
#include "game/production/ProductionConfigParser.h"
#include "game/production/ProductionManager.h"
#include "game/terraform/ElevationChange.h"
#include "game/terraform/TerraformMutations.h"
#include "game/terraform/TerraformRules.h"
#include "game/combat/ExplosionMutations.h"
#include "game/combat/ExplosionRules.h"
#include "game/ecology/FungalBloom.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/population/PopCompositionConfigParser.h"
#include "game/population/PopulationManager.h"
#include "game/units/AdHocDesign.h"
#include "game/natives/NativeDesign.h"
#include "game/natives/NativeUnitRegistry.h"
#include "game/movement/MovementRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitOrigin.h"
#include "game/units/UnitPlacementRules.h"
#include "game/units/Military.h"
#include "game/map/OccupantCoexistence.h"
#include "game/map/ImprovementIds.h"
#include "game/map/ImprovementRegistry.h"
#include "game/units/UnitDomain.h"
#include "game/visibility/FactionExploredMap.h"
#include "game/production/ProductionRules.h"
#include "game/production/IConstructable.h"
#include "game/faction/FactionConfig.h"
#include "lib/LuaRuntime.h"
#include "lib/RandomRoll.h"
#include "game/ecology/EcologyLedger.h"
#include "game/probes/MindControlLedger.h"

#include <algorithm>
#include <cmath>
#include <magic_enum.hpp>
#include <random>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ac
{

TriggeredEffectContext_t::TriggeredEffectContext_t(GameState& rGameStateIn, Faction& rFaction)
    : rGameState(rGameStateIn)
    , factions{&rFaction}
    , pFaction(&rFaction)
{
}

TriggeredEffectContext_t::TriggeredEffectContext_t(GameState& rGameStateIn, BaseManager& rBase)
    : rGameState(rGameStateIn)
    , factions{&rBase.GetFaction()}
    , pBase(&rBase)
    , pFaction(&rBase.GetFaction())
    , pTile(&rBase.GetTile())
{
}

std::mt19937& TriggeredEffectContext_t::Rng() const
{
    return pRng ? *pRng : rGameState.GetRng();
}

namespace
{

// Absent when the context has no such subject: the entry is skipped, not re-fired forever.
std::set<std::string>* OnceSubjectFor_(const OncePer_t& rOncePer, TriggeredEffectContext_t& rCtx,
                                       Faction& rFaction)
{
    switch (rOncePer.scope)
    {
        case OnceScope_t::Unit:
            return rCtx.pUnit ? &rCtx.pUnit->ConsumedTriggerKeys() : nullptr;
        case OnceScope_t::Base:
            return rCtx.pBase ? &rCtx.pBase->ConsumedTriggerKeys() : nullptr;
        case OnceScope_t::Faction:
            return &rFaction.ConsumedTriggerKeys();
        case OnceScope_t::World:
            return &rCtx.rGameState.ConsumedTriggerKeys();
    }
    return nullptr;
}

// The configured tech, or a random one the faction could research next.
std::optional<TechId> PickTechToGrant_(TriggeredEffectContext_t& rCtx, const Faction& rFaction,
                                       const GrantTechEffect_t& rGrant)
{
    if (rGrant.techId.has_value())
    {
        return rGrant.techId;
    }
    const std::vector<const TechConfig_t*> available = AvailableTechs(rFaction.GetResearch());
    if (available.empty())
    {
        return std::nullopt;
    }
    const std::size_t picked = PickUniformIndex(available.size(), rCtx.Rng());
    return available[picked]->id;
}

bool AddTech_(TriggeredEffectContext_t& rCtx, Faction& rFaction, const GrantTechEffect_t& rGrant,
              std::vector<TriggeredEffectResult_t>& rOut)
{
    const std::optional<TechId> picked = PickTechToGrant_(rCtx, rFaction, rGrant);
    if (!picked)
    {
        return false;
    }
    const TechId& techId = *picked;
    // Granting what the faction already knows is an ordinary outcome — two facilities can
    // grant the same tech — not the programmer error AddDiscoveredTech throws on.
    if (rFaction.GetResearch().HasDiscoveredTech(techId))
    {
        return false;
    }
    rFaction.GetResearch().AddDiscoveredTech(techId);
    rOut.push_back(TechGranted_t{techId});
    return true;
}

bool AddBuilding_(TriggeredEffectContext_t& rCtx, const AddBuildingEffect_t& rAdd,
                  std::vector<TriggeredEffectResult_t>& rOut)
{
    if (!rCtx.pBase)
    {
        return false;
    }
    // A grant whose target the base already holds is an ordinary outcome, not an error.
    if (!rCtx.pBase->GetBuildingManager().CanAddBuilding(rAdd.buildingId))
    {
        return false;
    }
    rCtx.pBase->GetBuildingManager().AddBuilding(rAdd.buildingId);
    rOut.push_back(BuildingAdded_t{rAdd.buildingId});
    return true;
}

// The subject is the perpetrator and actionTarget the victim. A victimless act is still an
// atrocity.
bool CommitAtrocity_(TriggeredEffectContext_t& rCtx, const CommitAtrocityEffect_t& rConfig,
                     Faction& rPerpetrator, std::vector<TriggeredEffectResult_t>& rOut)
{
    // An arm that worked out its own victim (Explosion) outranks the trigger site's target.
    const std::optional<FactionId_t> victimId =
        rCtx.derivedVictim ? rCtx.derivedVictim : rCtx.actionTarget;
    Faction* pVictim = victimId ? rCtx.rGameState.FindFaction(*victimId) : nullptr;
    rOut.push_back(CommitAtrocity(rCtx.rGameState, rPerpetrator, pVictim, rConfig.severity));
    return true;
}

bool SetInfiltration_(TriggeredEffectContext_t& rCtx, const TriggeredEffectConfig_t& rConfig,
                      Faction& rBeneficiary, std::vector<TriggeredEffectResult_t>& rOut)
{
    const FactionId_t beneficiaryId = rBeneficiary.GetFactionId();
    DiplomacyLedger& rDiplomacy = rCtx.rGameState.GetDiplomacyLedger();
    InfiltrationSet_t set;
    for (const Faction& rFaction : rCtx.rGameState.Factions())
    {
        const FactionId_t candidateId = rFaction.GetFactionId();
        if (FactionFilterCoversTarget(rConfig.factionFilter, beneficiaryId, candidateId,
                                      rCtx.actionTarget))
        {
            rDiplomacy.SetInfiltration(beneficiaryId, candidateId, true);
            set.targets.push_back(candidateId);
        }
    }
    const bool bDidAnything = !set.targets.empty();
    rOut.push_back(std::move(set));
    return bDidAnything;
}

bool Rebel_(TriggeredEffectContext_t& rCtx, std::vector<TriggeredEffectResult_t>& rOut)
{
    if (!rCtx.pBase)
    {
        return false;
    }
    const BaseId_t baseId = rCtx.pBase->GetBaseId();
    const std::optional<FactionId_t> newOwner =
        PickRebelFactionAndTransfer(*rCtx.pBase, rCtx.rGameState, rCtx.Rng());
    if (!newOwner)
    {
        return false;
    }
    rOut.push_back(BaseRebelled_t{baseId, *newOwner});
    return true;
}

// A full map grants fewer units rather than throwing: the caller is told the real count.
int SpawnGrantedUnits_(GameState& rGameState, Faction& rFaction, const UnitDesign& rDesign,
                       BaseManager& rAnchor, int count)
{
    WorldMap& rMap = rGameState.GetWorldMap();
    const std::vector<const Tile*> tiles =
        PlacementTiles(rAnchor.GetTile(), rDesign, rFaction.GetFactionId(), count);
    int spawned = 0;
    for (const Tile* pTile : tiles)
    {
        if (spawned >= count)
        {
            break;
        }
        // Homed at the anchor so it pays support, but explicitly produced nowhere: a gift is
        // not a build, so it receives no on_unit_produced train bonuses and no prototype latch.
        rFaction.GetUnitManager().CreateUnit(rGameState.AllocateUnitId(), rDesign,
                                             rMap.GetUnitPositions(), *pTile,
                                             UnitOrigin::Gift(&rAnchor));
        ++spawned;
    }
    return spawned;
}

const IDesign* ResolveGrantUnitDesign_(TriggeredEffectContext_t& rCtx, Faction& rFaction,
                                       const GrantUnitEffect_t& rGrant)
{
    if (rGrant.designId)
    {
        if (const IDesign* pExisting = rFaction.GetMilitary().GetDesign(*rGrant.designId))
        {
            return pExisting;
        }
        const NativeUnitRegistry* pNatives =
            rCtx.rGameState.GetGameData().nativeUnitRegistry.get();
        if (!pNatives)
        {
            return nullptr;
        }
        const NativeUnitConfig_t* pConfig = pNatives->Find(*rGrant.designId);
        if (!pConfig)
        {
            // TODO(supply-pods case 9): author Unity Rover/Chopper/Foil + Battle Ogre Mk1–3
            // and a single conditional picker — docs/thinker/terranx-map.md "Case 9 — UnityVehicle".
            return nullptr;
        }
        auto pDesign = std::make_unique<NativeDesign>(*pConfig);
        if (!rFaction.GetMilitary().AddDesign(std::move(pDesign)))
        {
            return rFaction.GetMilitary().GetDesign(*rGrant.designId);
        }
        return rFaction.GetMilitary().GetDesign(*rGrant.designId);
    }
    return EnsureAdHocDesign(rFaction, *rCtx.rGameState.GetGameData().unitComponentRegistry,
                             rGrant.componentIds, "granted");
}

bool DomainMatchesTile_(UnitDomain_t domain, const Tile& rTile)
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

int SpawnOnTile_(GameState& rGameState, Faction& rFaction, const IDesign& rDesign,
                 BaseManager* pAnchor, Tile& rTile, int count)
{
    WorldMap& rMap = rGameState.GetWorldMap();
    int spawned = 0;
    for (int i = 0; i < count; ++i)
    {
        rFaction.GetUnitManager().CreateUnit(rGameState.AllocateUnitId(), rDesign,
                                             rMap.GetUnitPositions(), rTile,
                                             UnitOrigin::Gift(pAnchor));
        ++spawned;
    }
    return spawned;
}

bool GrantUnit_(TriggeredEffectContext_t& rCtx, const GrantUnitEffect_t& rGrant,
                Faction& rFaction, std::vector<TriggeredEffectResult_t>& rOut)
{
    if (rGrant.count <= 0)
    {
        rOut.push_back(UnitsGranted_t{0, {}});
        return false;
    }

    const IDesign* pDesign = ResolveGrantUnitDesign_(rCtx, rFaction, rGrant);
    if (!pDesign)
    {
        rOut.push_back(UnitsGranted_t{0, {}});
        return false;
    }

    const std::optional<BaseId_t> anchorId = GrantAnchorBase(rFaction, rCtx.pBase, rCtx.pTile);
    BaseManager* pAnchor = anchorId ? rFaction.FindBase(*anchorId) : nullptr;

    int spawned = 0;
    if (rGrant.designId)
    {
        // Named designs (natives / future Unity stock) land on the context tile.
        if (!rCtx.pTile || !DomainMatchesTile_(pDesign->GetDomain(), *rCtx.pTile))
        {
            rOut.push_back(UnitsGranted_t{0, {}});
            return false;
        }
        spawned = SpawnOnTile_(rCtx.rGameState, rFaction, *pDesign, pAnchor, *rCtx.pTile,
                               rGrant.count);
    }
    else
    {
        // Component grants keep the anchor-ring placement used by base conquest / council.
        if (!pAnchor)
        {
            rOut.push_back(UnitsGranted_t{0, {}});
            return false;
        }
        const auto* pUnitDesign = dynamic_cast<const UnitDesign*>(pDesign);
        if (!pUnitDesign)
        {
            rOut.push_back(UnitsGranted_t{0, {}});
            return false;
        }
        spawned = SpawnGrantedUnits_(rCtx.rGameState, rFaction, *pUnitDesign, *pAnchor,
                                     rGrant.count);
    }
    rOut.push_back(UnitsGranted_t{spawned, pDesign->GetId()});
    return spawned > 0;
}

int ResolveGrantEnergyAmount_(const GrantEnergyEffect_t& rGrant, TriggeredEffectContext_t& rCtx)
{
    if (rGrant.amount)
    {
        return *rGrant.amount;
    }
    LuaRuntime& rLua = *rCtx.rGameState.GetGameData().luaRuntime;
    const std::unordered_map<std::string, double> vars = {
        {"turn", static_cast<double>(rCtx.rGameState.GetTurnNumber())},
    };
    return rLua.EvalInt(*rGrant.formula, vars);
}

bool TryPlaceOccupant_(GameState& rGameState, Tile& rTile, const std::string& rImprovementId)
{
    const ImprovementRegistry& rRegistry = rGameState.GetTileEffects().GetImprovements();
    const ImprovementConfig_t* pConfig = rRegistry.Find(rImprovementId);
    if (!pConfig)
    {
        return false;
    }
    if (rTile.HasTerrainFeature(pConfig->id))
    {
        return false;
    }
    if (!CanBuildImprovement(rTile, *pConfig, {}, {}))
    {
        return false;
    }
    rGameState.GetTileEffects().AddOccupantWithEffects(rTile, pConfig->id);
    return true;
}

bool AddOccupant_(TriggeredEffectContext_t& rCtx, const std::string& rImprovementId,
                  std::vector<TriggeredEffectResult_t>& rOut)
{
    if (!rCtx.pTile)
    {
        return false;
    }
    if (!TryPlaceOccupant_(rCtx.rGameState, *rCtx.pTile, rImprovementId))
    {
        return false;
    }
    rOut.push_back(OccupantAdded_t{rImprovementId});
    return true;
}

bool RevealMap_(TriggeredEffectContext_t& rCtx, Faction& rFaction,
                const RevealMapEffect_t& rReveal, std::vector<TriggeredEffectResult_t>& rOut)
{
    if (!rCtx.pTile)
    {
        return false;
    }
    FactionExploredMap& rExplored = rFaction.GetExploredMap();
    WorldMap& rWorldMap = rCtx.rGameState.GetWorldMap();
    ForEachTileInChebyshevRadius(*rCtx.pTile, rWorldMap, rReveal.radius,
                                 [&rExplored](Tile* pTile, int)
                                 {
                                     rExplored.Mark(*pTile);
                                 });
    rOut.push_back(MapRevealed_t{rReveal.radius});
    return true;
}

bool TeleportUnit_(TriggeredEffectContext_t& rCtx, const TeleportUnitEffect_t& rTeleport,
                   std::vector<TriggeredEffectResult_t>& rOut)
{
    if (!rCtx.pUnit || !rCtx.pTile)
    {
        return false;
    }
    WorldMap& rWorldMap = rCtx.rGameState.GetWorldMap();
    const int mapWidth = rWorldMap.GetWidth();
    const bool bSea = rCtx.pTile->IsWater();
    std::vector<Tile*> candidates;
    for (const std::unique_ptr<Tile>& pTile : rWorldMap.GetTiles())
    {
        Tile& rCandidate = *pTile;
        if (rCandidate.IsWater() != bSea)
        {
            continue;
        }
        const int distance = ChebyshevDistance(*rCtx.pTile, rCandidate, mapWidth);
        if (distance < rTeleport.minDistance || distance > rTeleport.maxDistance)
        {
            continue;
        }
        candidates.push_back(&rCandidate);
    }
    if (candidates.empty())
    {
        return false;
    }
    Tile& rDest = *candidates[PickUniformIndex(candidates.size(), rCtx.Rng())];
    rWorldMap.GetUnitPositions().MoveUnit(*rCtx.pUnit, rDest);
    rCtx.pTile = &rDest;
    rOut.push_back(UnitTeleported_t{rDest.GetCoord()});
    return true;
}

bool FinishProduction_(TriggeredEffectContext_t& rCtx, Faction& rFaction,
                       std::vector<TriggeredEffectResult_t>& rOut)
{
    if (!rCtx.pTile)
    {
        return false;
    }
    BaseManager* pBase = rFaction.FindNearestBase(*rCtx.pTile);
    if (!pBase)
    {
        return false;
    }
    const IConstructable* pItem = pBase->GetProduction().GetCurrentProduction();
    if (!pItem)
    {
        return false;
    }
    const int cost = ItemMineralCost(*pBase, *pItem);
    pBase->GetProduction().SetMineralStockpile(cost);
    ProductionFinished_t finished;
    finished.baseName = pBase->GetName();
    finished.itemName = pItem->GetName();
    finished.cameraTile = pBase->GetTile().GetCoord();
    rOut.push_back(std::move(finished));
    return true;
}

bool CloneUnit_(TriggeredEffectContext_t& rCtx, Faction& rFaction,
                std::vector<TriggeredEffectResult_t>& rOut)
{
    if (!rCtx.pUnit || !rCtx.pTile)
    {
        return false;
    }
    const int hp = rCtx.pUnit->GetCurrentHp();
    Unit& rClone = rFaction.GetUnitManager().CreateUnit(
        rCtx.rGameState.AllocateUnitId(), rCtx.pUnit->GetDesign(),
        rCtx.rGameState.GetWorldMap().GetUnitPositions(), *rCtx.pTile, UnitOrigin::Gift(nullptr));
    rClone.SetCurrentHp(hp);
    rOut.push_back(UnitsGranted_t{1, rClone.GetDesign().GetId()});
    return true;
}

bool GrantCommlink_(TriggeredEffectContext_t& rCtx, Faction& rFaction,
                    std::vector<TriggeredEffectResult_t>& rOut)
{
    const FactionId_t visitorId = rFaction.GetFactionId();
    DiplomacyLedger& rLedger = rCtx.rGameState.GetDiplomacyLedger();
    std::vector<Faction*> candidates;
    for (Faction& rOther : rCtx.rGameState.Factions())
    {
        if (rOther.GetFactionId() == visitorId)
        {
            continue;
        }
        if (IsNativeLifeFaction(rOther.GetDefinition().identity.species))
        {
            continue;
        }
        if (rLedger.AreKnown(visitorId, rOther.GetFactionId()))
        {
            continue;
        }
        candidates.push_back(&rOther);
    }
    if (candidates.empty())
    {
        return false;
    }
    Faction& rContact = *candidates[PickUniformIndex(candidates.size(), rCtx.Rng())];
    rLedger.SetKnown(visitorId, rContact.GetFactionId(), true);
    const FactionConfig_t& rDef = rContact.GetDefinition();
    rOut.push_back(CommlinkGranted_t{rDef.leader.title, rDef.leader.name, rDef.identity.name});
    return true;
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

bool TileAcceptsNativeSpawn_(const Faction& rPlanet, const IDesign& rDesign, const Tile& rTile,
                             const WorldMap& rWorldMap)
{
    if (!CanEnterTileTerrain(rDesign.GetDomain(), rTile))
    {
        return false;
    }
    for (const Unit* pUnit : rWorldMap.GetUnitsOnTile(rTile))
    {
        if (pUnit && !MayShareTiles(rPlanet, pUnit->GetFaction().GetFactionId()))
        {
            return false;
        }
    }
    return true;
}

std::vector<Tile*> CollectAdjacentNativeSpawnTiles_(const Faction& rPlanet,
                                                    const IDesign& rDesign, Tile& rOrigin,
                                                    WorldMap& rWorldMap)
{
    std::vector<Tile*> candidates;
    ForEachNeighborInChebyshevRadius(
        rOrigin, rWorldMap, 1,
        [&](Tile* pNeighbor, int)
        {
            if (pNeighbor && TileAcceptsNativeSpawn_(rPlanet, rDesign, *pNeighbor, rWorldMap))
            {
                candidates.push_back(pNeighbor);
            }
        });
    return candidates;
}

Unit& SpawnNativeUnit_(Faction& rPlanet, const IDesign& rDesign, Tile& rTile,
                       GameState& rGameState)
{
    return rPlanet.GetUnitManager().CreateUnit(rGameState.AllocateUnitId(), rDesign,
                                               rGameState.GetWorldMap().GetUnitPositions(),
                                               rTile, UnitOrigin::Gift(nullptr));
}

void LoadSpawnedTransportCargo_(Faction& rPlanet, Unit& rHull, Tile& rTile,
                                const std::vector<std::string>& rCargoIds,
                                GameState& rGameState, std::mt19937& rRng)
{
    if (rCargoIds.empty())
    {
        return;
    }
    while (FreeCargoSlots(rHull) > 0)
    {
        const IDesign* pCargo = rPlanet.GetMilitary().GetDesign(
            rCargoIds[PickUniformIndex(rCargoIds.size(), rRng)]);
        if (!pCargo)
        {
            break;
        }
        Unit& rPassenger = SpawnNativeUnit_(rPlanet, *pCargo, rTile, rGameState);
        rPassenger.Transport().EmbarkInto(rHull);
    }
}

bool SpawnNative_(TriggeredEffectContext_t& rCtx, const SpawnNativeEffect_t& rSpawn,
                  std::vector<TriggeredEffectResult_t>& rOut)
{
    if (!rCtx.pTile)
    {
        return false;
    }
    Faction* pPlanet = FindNativeLifeFaction_(rCtx.rGameState);
    if (!pPlanet)
    {
        return false;
    }
    const IDesign* pDesign = pPlanet->GetMilitary().GetDesign(rSpawn.nativeId);
    if (!pDesign)
    {
        return false;
    }

    WorldMap& rWorldMap = rCtx.rGameState.GetWorldMap();
    const std::vector<Tile*> candidates =
        CollectAdjacentNativeSpawnTiles_(*pPlanet, *pDesign, *rCtx.pTile, rWorldMap);
    if (candidates.empty())
    {
        return false;
    }
    Tile& rSpawnTile = *candidates[PickUniformIndex(candidates.size(), rCtx.Rng())];
    Unit& rSpawned =
        SpawnNativeUnit_(*pPlanet, *pDesign, rSpawnTile, rCtx.rGameState);
    LoadSpawnedTransportCargo_(*pPlanet, rSpawned, rSpawnTile, rSpawn.cargoIds,
                               rCtx.rGameState, rCtx.Rng());
    rOut.push_back(NativeSpawned_t{rSpawn.nativeId});
    return true;
}

void PlaceOnTile_(GameState& rGameState, Tile& rTile, const std::string& rPrimaryId,
                  bool bAlsoRoad)
{
    TryPlaceOccupant_(rGameState, rTile, rPrimaryId);
    if (bAlsoRoad)
    {
        TryPlaceOccupant_(rGameState, rTile, std::string(ImprovementIds::k_Road));
    }
}

void PlaceCluster_(GameState& rGameState, Tile& rCenter, WorldMap& rWorldMap,
                   const std::string& rPrimaryId, bool bAlsoRoad,
                   std::span<const Rational_t> rChances, std::mt19937& rRng)
{
    const std::vector<Tile*> tiles = SelectTilesByRingChance(
        rCenter, rWorldMap, rChances, [](Tile&, int) { return true; }, rRng);
    for (Tile* pTile : tiles)
    {
        PlaceOnTile_(rGameState, *pTile, rPrimaryId, bAlsoRoad);
    }
}

bool TerraformCluster_(TriggeredEffectContext_t& rCtx, const TerraformClusterEffect_t& rCluster,
                       std::vector<TriggeredEffectResult_t>& rOut)
{
    if (!rCtx.pTile)
    {
        return false;
    }
    WorldMap& rWorldMap = rCtx.rGameState.GetWorldMap();
    const std::string& rChosenId =
        rCluster.improvementIds[PickUniformIndex(rCluster.improvementIds.size(), rCtx.Rng())];
    const bool bAlsoRoad = rChosenId == "Mine";
    PlaceCluster_(rCtx.rGameState, *rCtx.pTile, rWorldMap, rChosenId, bAlsoRoad, rCluster.chance,
                  rCtx.Rng());
    rOut.push_back(TerraformClusterApplied_t{rChosenId});
    return true;
}

bool AddRandomOccupant_(TriggeredEffectContext_t& rCtx, const AddRandomOccupantEffect_t& rAdd,
                        std::vector<TriggeredEffectResult_t>& rOut)
{
    if (!rCtx.pTile)
    {
        return false;
    }
    const ImprovementRegistry& rRegistry = rCtx.rGameState.GetTileEffects().GetImprovements();
    std::vector<const ImprovementConfig_t*> candidates;
    for (const std::string& rId : rAdd.improvementIds)
    {
        const ImprovementConfig_t* pBonus = rRegistry.Find(rId);
        if (!pBonus)
        {
            continue;
        }
        if (rCtx.pTile->HasTerrainFeature(pBonus->id))
        {
            continue;
        }
        if (!CanBuildImprovement(*rCtx.pTile, *pBonus, {}, {}))
        {
            continue;
        }
        candidates.push_back(pBonus);
    }
    if (candidates.empty())
    {
        return false;
    }
    const ImprovementConfig_t& rBonus =
        *candidates[PickUniformIndex(candidates.size(), rCtx.Rng())];
    rCtx.rGameState.GetTileEffects().AddOccupantWithEffects(*rCtx.pTile, rBonus.id);
    rOut.push_back(OccupantAdded_t{rBonus.id});
    return true;
}

bool Earthquake_(TriggeredEffectContext_t& rCtx, const EarthquakeEffect_t& rConfig,
                 std::vector<TriggeredEffectResult_t>& rOut)
{
    if (!rCtx.pTile)
    {
        return false;
    }
    int levels = rConfig.levels;
    if (rConfig.levelsStat)
    {
        if (!rCtx.pUnit)
        {
            return false;
        }
        levels = ResolveStat(*rCtx.pUnit, *rConfig.levelsStat);
    }
    // The tile already carries the map rules WorldMap bound to it, so the quake needs no
    // GameDataContext of its own.
    if (!ApplyEarthquake(*rCtx.pTile, levels, rCtx.Rng(), rCtx.pTile->MapRules(),
                         rCtx.rGameState))
    {
        return false;
    }
    rOut.push_back(EarthquakeApplied_t{levels});
    return true;
}

// Aquifer and river are tile bools; other occupants are optional terrain. False when the tile
// already had the requested state.
bool ToggleTerrainFeature_(GameState& rGameState, Tile& rTile, const std::string& rFeatureId,
                           bool bPresent)
{
    if (rFeatureId == magic_enum::enum_name(TerrainFeature_t::Aquifer))
    {
        if (rTile.GetHasAquifer() == bPresent)
        {
            return false;
        }
        rTile.SetHasAquifer(bPresent);
        rGameState.GetTileEffects().RecomputeClimate();
        return true;
    }
    if (rFeatureId == magic_enum::enum_name(TerrainFeature_t::River))
    {
        if (rTile.GetHasRiver() == bPresent)
        {
            return false;
        }
        rTile.SetHasRiver(bPresent);
        return true;
    }
    if (rTile.HasTerrainFeature(rFeatureId) == bPresent)
    {
        return false;
    }
    TileEffectsContext& rTileEffects = rGameState.GetTileEffects();
    if (bPresent)
    {
        rTileEffects.AddOccupantWithEffects(rTile, rFeatureId);
    }
    else
    {
        rTileEffects.RemoveOccupantWithEffects(rTile, rFeatureId);
    }
    return true;
}

bool SetTerrainFeature_(TriggeredEffectContext_t& rCtx, const SetTerrainFeatureEffect_t& rConfig,
                        std::vector<TriggeredEffectResult_t>& rOut)
{
    if (!rCtx.pTile
        || !ToggleTerrainFeature_(rCtx.rGameState, *rCtx.pTile, rConfig.featureId,
                                  rConfig.bPresent))
    {
        return false;
    }
    rOut.push_back(TerrainFeatureChanged_t{rConfig.featureId, rConfig.bPresent});
    return true;
}

bool StepRockiness_(TriggeredEffectContext_t& rCtx, const StepRockinessEffect_t& rConfig,
                    std::vector<TriggeredEffectResult_t>& rOut)
{
    if (!rCtx.pTile || rConfig.steps == 0)
    {
        return false;
    }
    // Ordered axis: the clamp rides magic_enum's index range rather than assumed values.
    const auto& rValues = magic_enum::enum_values<Rockiness_t>();
    const auto index = magic_enum::enum_index(rCtx.pTile->GetRockiness());
    const int current = static_cast<int>(*index);
    const int last = static_cast<int>(rValues.size()) - 1;
    const int stepped = std::clamp(current + rConfig.steps, 0, last);
    if (stepped == current)
    {
        return false;
    }
    rCtx.pTile->SetRockiness(rValues[static_cast<size_t>(stepped)]);
    rOut.push_back(RockinessChanged_t{stepped - current});
    return true;
}

bool ElevationChange_(TriggeredEffectContext_t& rCtx, const ElevationChangeEffect_t& rConfig,
                      std::vector<TriggeredEffectResult_t>& rOut)
{
    if (!rCtx.pTile || !rCtx.pUnit)
    {
        return false;
    }
    const MapRulesConfig_t& rRules = rCtx.pTile->MapRules();
    const UnitDomain_t domain = rCtx.pUnit->GetDomain();
    // Whether a Former may start was checked at order time. Now the cost is paid, so a roll past
    // the floor lands on it; only no change at all reports false.
    const int roll = RollLevelMeters(rCtx.Rng(), rRules);
    const int delta = rConfig.bRaise ? roll : -roll;
    const int floor =
        rConfig.bRaise ? rRules.minElevationMeters : FormerLowerFloorMeters(domain, rRules);
    const std::optional<std::vector<MoistureChange_t>> changes = ApplyElevationDelta(
        *rCtx.pTile, delta, rRules, floor, rRules.maxElevationMeters, rCtx.rGameState);
    if (!changes)
    {
        return false;
    }
    ApplyRainfallGrievance(rCtx.rGameState, rCtx.pUnit->GetFaction(), *changes);
    rOut.push_back(ElevationChanged_t{delta});
    return true;
}

bool FungalBloom_(TriggeredEffectContext_t& rCtx, const FungalBloomEffect_t& rConfig,
                  std::vector<TriggeredEffectResult_t>& rOut)
{
    if (!rCtx.pTile)
    {
        return false;
    }
    FungalBloomResult_t result;
    if (!rConfig.chance.empty())
    {
        result = ApplyFungalBloom(*rCtx.pTile, rCtx.rGameState.GetWorldMap(), rConfig.chance,
                                  rCtx.Rng(), rCtx.rGameState);
    }
    else
    {
        int tiles = rConfig.tiles;
        if (rConfig.tilesStat)
        {
            if (!rCtx.pUnit)
            {
                return false;
            }
            tiles = ResolveStat(*rCtx.pUnit, *rConfig.tilesStat);
        }
        result = ApplyFungalBloom(*rCtx.pTile, rCtx.rGameState.GetWorldMap(), tiles, rCtx.Rng(),
                                  rCtx.rGameState);
    }
    if (result.tilesFungused == 0)
    {
        return false;
    }
    rOut.push_back(FungalBloomApplied_t{result.tilesFungused, result.lifeforms});
    return true;
}

bool Explosion_(TriggeredEffectContext_t& rCtx, const ExplosionEffect_t& rConfig,
                std::vector<TriggeredEffectResult_t>& rOut)
{
    if (!rCtx.pTile)
    {
        return false;
    }
    const std::optional<int> radius = ExplosionRadius(rConfig, rCtx.pUnit);
    if (!radius)
    {
        return false;
    }
    const ExplosionResult_t result =
        ApplyExplosion(*rCtx.pTile, rCtx.rGameState.GetWorldMap(), *radius, rCtx.Rng(),
                       rCtx.rGameState, rCtx.pUnit);
    // Only a unit-borne blast has someone to answer for it. Without one there is no detonator
    // to exclude, so CommitAtrocity falls back to whatever the trigger site targeted.
    if (rCtx.pUnit)
    {
        rCtx.derivedVictim = BlastVictim(result.baseOwnersDestroyed, result.unitOwnersDestroyed,
                                         rCtx.pUnit->GetFaction().GetFactionId());
    }
    if (!result.bChanged)
    {
        return false;
    }
    rOut.push_back(ExplosionApplied_t{result.radius, result.tiles});
    return true;
}

// A faction-subject effect applies once per listed faction (council GrantEnergy); any other
// applies once, so a DestroyFacility cannot hit a base per member.
bool IsPerFactionSubject_(const TriggeredEffectVariant_t& rEffect)
{
    return std::visit(
        [](const auto& rConcrete)
        {
            using T = std::decay_t<decltype(rConcrete)>;
            if constexpr (std::is_same_v<T, GrantTechEffect_t>
                          || std::is_same_v<T, GrantEnergyEffect_t>
                          || std::is_same_v<T, GrantUnitEffect_t>
                          || std::is_same_v<T, SetInfiltrationEffect_t>
                          || std::is_same_v<T, CommitAtrocityEffect_t>
                          || std::is_same_v<T, RecordMindControlEffect_t>
                          || std::is_same_v<T, GrantCleanMineralsEffect_t>
                          || std::is_same_v<T, AddVirtualMineralsEffect_t>)
            {
                return true;
            }
            else if constexpr (std::is_same_v<T, AddBuildingEffect_t>
                               || std::is_same_v<T, GrantResourceEffect_t>
                               || std::is_same_v<T, ModifyPopulationEffect_t>
                               || std::is_same_v<T, GrantXpEffect_t>
                               || std::is_same_v<T, RestoreHitPointsEffect_t>
                               || std::is_same_v<T, DestroyFacilityEffect_t>
                               || std::is_same_v<T, RebelEffect_t>
                               || std::is_same_v<T, DestroyUnitEffect_t>
                               || std::is_same_v<T, EarthquakeEffect_t>
                               || std::is_same_v<T, FungalBloomEffect_t>
                               || std::is_same_v<T, ExplosionEffect_t>
                               || std::is_same_v<T, SetTerrainFeatureEffect_t>
                               || std::is_same_v<T, StepRockinessEffect_t>
                               || std::is_same_v<T, ElevationChangeEffect_t>
                               || std::is_same_v<T, WorldParameterEffect_t>
                               || std::is_same_v<T, AddOccupantEffect_t>
                               || std::is_same_v<T, RevealMapEffect_t>
                               || std::is_same_v<T, TeleportUnitEffect_t>
                               || std::is_same_v<T, FinishProductionEffect_t>
                               || std::is_same_v<T, CloneUnitEffect_t>
                               || std::is_same_v<T, GrantCommlinkEffect_t>
                               || std::is_same_v<T, SpawnNativeEffect_t>
                               || std::is_same_v<T, TerraformClusterEffect_t>
                               || std::is_same_v<T, AddRandomOccupantEffect_t>)
            {
                return false;
            }
            else
            {
                static_assert(k_AlwaysFalse<T>);
            }
        },
        rEffect);
}

// Apply one ModifyPopulation. Returns the signed size delta actually applied (negative when
// pops were removed). Honors minSize when shrinking; stops adding when CanGrow() is false.
int ApplyModifyPopulation_(BaseManager& rBase, const ModifyPopulationEffect_t& rEffect)
{
    PopulationManager& rPops = rBase.GetPopulation();
    const int size = rPops.GetSize();
    const int delta = PredictModifyPopulationDelta(size, rEffect);

    if (delta < 0)
    {
        // Colony-pod (and other) pop costs: if ResourceCollection primed tanks to -1, wipe
        // that mark to 0 before RemovePop so BaseGrowth does not starve the leftover citizens.
        if (rPops.GetNutrientStockpile() < 0)
        {
            rPops.SetNutrientStockpile(0);
        }
        const int toRemove = -delta;
        int removed = 0;
        while (removed < toRemove && rPops.GetSize() > rEffect.minSize)
        {
            rPops.RemovePop();
            ++removed;
        }
        return -removed;
    }

    if (delta > 0)
    {
        int added = 0;
        while (added < delta && rPops.CanGrow())
        {
            rPops.AddPop();
            ++added;
        }
        return added;
    }

    return 0;
}

// The hit points the restore leaves a unit at, from before, against its maximum.
int RestoredHp_(const RestoreHitPointsEffect_t& rRestore, int before, int maxHp)
{
    switch (rRestore.op)
    {
    case RestoreHitPointsOp_t::Add:
        return before + rRestore.amount;
    case RestoreHitPointsOp_t::AddPercent:
        return before + (maxHp * rRestore.amount) / 100;
    case RestoreHitPointsOp_t::MaxClamp:
        return std::min(before, rRestore.amount);
    case RestoreHitPointsOp_t::MinClamp:
        return std::max(before, rRestore.amount);
    case RestoreHitPointsOp_t::SetPercent:
        return std::max(before, (maxHp * rRestore.amount) / 100);
    }
    return before;
}

// Exhaustive over TriggeredEffectVariant_t: a new alternative without an arm fails to compile.
// Each arm returns whether the effect did anything.
struct TriggeredEffectApplier_
{
    const TriggeredEffectConfig_t& rConfig;
    TriggeredEffectContext_t& rCtx;
    Faction& rFaction;
    std::vector<TriggeredEffectResult_t>& rOut;

    bool operator()(const AddBuildingEffect_t& rAdd) const
    {
        return AddBuilding_(rCtx, rAdd, rOut);
    }

    bool operator()(const GrantTechEffect_t& rGrant) const
    {
        return AddTech_(rCtx, rFaction, rGrant, rOut);
    }

    bool operator()(const GrantUnitEffect_t& rGrant) const
    {
        return GrantUnit_(rCtx, rGrant, rFaction, rOut);
    }

    bool operator()(const GrantEnergyEffect_t& rGrant) const
    {
        const int amount = ResolveGrantEnergyAmount_(rGrant, rCtx);
        rFaction.GetEconomy().AddEnergy(amount);
        rOut.push_back(EnergyGranted_t{amount});
        return true;
    }

    bool operator()(const GrantResourceEffect_t& rGrant) const
    {
        if (!rCtx.pBase)
        {
            return false;
        }
        const double seed = static_cast<double>(rGrant.amount);
        double total = seed;
        if (rGrant.modifierStat)
        {
            const EffectContext_t ctx{.pBase = rCtx.pBase};
            total = ResolveBaseStat(rCtx.pBase->GetBaseEffects(), *rGrant.modifierStat, seed, &ctx);
        }
        const int credit = std::max(0, FinalizeResolvedStat(total));
        if (rGrant.resource == StatId_t::Minerals)
        {
            ProductionManager& rProduction = rCtx.pBase->GetProduction();
            rProduction.SetMineralStockpile(rProduction.GetMineralStockpile() + credit);
        }
        else if (rGrant.resource == StatId_t::Nutrients)
        {
            PopulationManager& rPops = rCtx.pBase->GetPopulation();
            rPops.SetNutrientStockpile(rPops.GetNutrientStockpile() + credit);
        }
        else
        {
            throw std::logic_error("GrantResource: unsupported resource stockpile");
        }
        rOut.push_back(ResourceGranted_t{rGrant.resource, credit});
        return true;
    }

    // TODO: change sea level and climate through WorldEvents once it has a trigger API. False
    // meanwhile, so oncePer is not spent.
    bool operator()(const WorldParameterEffect_t&) const { return false; }

    bool operator()(const SetInfiltrationEffect_t&) const
    {
        return SetInfiltration_(rCtx, rConfig, rFaction, rOut);
    }

    bool operator()(const CommitAtrocityEffect_t& rAtrocity) const
    {
        return CommitAtrocity_(rCtx, rAtrocity, rFaction, rOut);
    }

    bool operator()(const RecordMindControlEffect_t& rRecord) const
    {
        rCtx.rGameState.GetMindControlLedger().Record(rFaction.GetFactionId(), rRecord.weight);
        rOut.push_back(MindControlRecorded_t{rRecord.weight});
        return true;
    }

    bool operator()(const GrantCleanMineralsEffect_t& rGrant) const
    {
        EcologyLedger& rEcology = rCtx.rGameState.GetEcologyLedger();
        if (rGrant.bRequiresFirstBloom && rEcology.FungalBlooms(rFaction.GetFactionId()) == 0)
        {
            return false;
        }
        rEcology.GrantCleanMinerals(rFaction.GetFactionId(), rGrant.amount);
        rOut.push_back(CleanMineralsGranted_t{rGrant.amount});
        return true;
    }

    bool operator()(const AddVirtualMineralsEffect_t& rAdd) const
    {
        rCtx.rGameState.GetEcologyLedger().AddVirtualMinerals(rFaction.GetFactionId(),
                                                              rAdd.amount);
        rOut.push_back(VirtualMineralsAdded_t{rAdd.amount});
        return true;
    }

    bool operator()(const ModifyPopulationEffect_t& rModify) const
    {
        if (!rCtx.pBase)
        {
            return false;
        }
        ModifyPopulationEffect_t floored = rModify;
        floored.minSize = std::max(floored.minSize, rCtx.populationFloor);
        const int delta = ApplyModifyPopulation_(*rCtx.pBase, floored);
        rOut.push_back(PopulationChanged_t{delta});
        return delta != 0;
    }

    bool operator()(const GrantXpEffect_t& rGrant) const
    {
        if (!rCtx.pUnit)
        {
            return false;
        }
        const int granted = GrantXp_(*rCtx.pUnit, rGrant);
        rOut.push_back(XpGranted_t{granted});
        if (granted > 0)
        {
            MaybeRemoveHost_(rGrant);
        }
        return granted != 0;
    }

    bool operator()(const RestoreHitPointsEffect_t& rRestore) const
    {
        if (!rCtx.pUnit)
        {
            return false;
        }
        Unit& rUnit = *rCtx.pUnit;
        const int before = rUnit.GetCurrentHp();
        const int maxHp = ResolveStat(rUnit, StatId_t::HitPoints);
        rUnit.SetCurrentHp(RestoredHp_(rRestore, before, maxHp));
        const int restored = rUnit.GetCurrentHp() - before;
        rOut.push_back(HitPointsRestored_t{restored});
        return restored != 0;
    }

    bool operator()(const DestroyFacilityEffect_t& rDestroy) const
    {
        if (!rCtx.pBase)
        {
            return false;
        }
        std::vector<BuildingId_t> destroyed = DestroyRandomFacilities(
            *rCtx.pBase, rDestroy.count, rDestroy.exclusions, rCtx.Rng());
        const bool bDidAnything = !destroyed.empty();
        rOut.push_back(FacilitiesDestroyed_t{std::move(destroyed)});
        return bDidAnything;
    }

    bool operator()(const RebelEffect_t&) const { return Rebel_(rCtx, rOut); }

    bool operator()(const EarthquakeEffect_t& rQuake) const
    {
        return Earthquake_(rCtx, rQuake, rOut);
    }

    bool operator()(const FungalBloomEffect_t& rBloom) const
    {
        return FungalBloom_(rCtx, rBloom, rOut);
    }

    bool operator()(const ExplosionEffect_t& rExplosion) const
    {
        return Explosion_(rCtx, rExplosion, rOut);
    }

    bool operator()(const SetTerrainFeatureEffect_t& rSet) const
    {
        return SetTerrainFeature_(rCtx, rSet, rOut);
    }

    bool operator()(const StepRockinessEffect_t& rStep) const
    {
        return StepRockiness_(rCtx, rStep, rOut);
    }

    bool operator()(const ElevationChangeEffect_t& rChange) const
    {
        return ElevationChange_(rCtx, rChange, rOut);
    }

    bool operator()(const DestroyUnitEffect_t&) const
    {
        if (!rCtx.pUnit)
        {
            return false;
        }
        rOut.push_back(UnitDestroyed_t{rCtx.pUnit->GetUnitId()});
        return true;
    }

    bool operator()(const AddOccupantEffect_t& rAdd) const
    {
        return AddOccupant_(rCtx, rAdd.improvementId, rOut);
    }

    bool operator()(const RevealMapEffect_t& rReveal) const
    {
        return RevealMap_(rCtx, rFaction, rReveal, rOut);
    }

    bool operator()(const TeleportUnitEffect_t& rTeleport) const
    {
        return TeleportUnit_(rCtx, rTeleport, rOut);
    }

    bool operator()(const FinishProductionEffect_t&) const
    {
        return FinishProduction_(rCtx, rFaction, rOut);
    }

    bool operator()(const CloneUnitEffect_t&) const
    {
        return CloneUnit_(rCtx, rFaction, rOut);
    }

    bool operator()(const GrantCommlinkEffect_t&) const
    {
        return GrantCommlink_(rCtx, rFaction, rOut);
    }

    bool operator()(const SpawnNativeEffect_t& rSpawn) const
    {
        return SpawnNative_(rCtx, rSpawn, rOut);
    }

    bool operator()(const TerraformClusterEffect_t& rCluster) const
    {
        return TerraformCluster_(rCtx, rCluster, rOut);
    }

    bool operator()(const AddRandomOccupantEffect_t& rAdd) const
    {
        return AddRandomOccupant_(rCtx, rAdd, rOut);
    }

private:
    // Returns the XP the unit actually gained, after SetXp's own limits.
    static int GrantXp_(Unit& rUnit, const GrantXpEffect_t& rGrant)
    {
        const int before = rUnit.GetXp();
        const double next = ApplyModifierStack(static_cast<double>(before),
                                               {{static_cast<double>(rGrant.amount), rGrant.op}});
        const int nextXp = static_cast<int>(std::lround(next));
        rUnit.SetXp(nextXp);
        return rUnit.GetXp() - before;
    }

    // remove_host_chance: the grant may consume the improvement that gave it.
    void MaybeRemoveHost_(const GrantXpEffect_t& rGrant) const
    {
        if (!rGrant.removeHostChance || !rCtx.hostImprovementId || !rCtx.pTile)
        {
            return;
        }
        if (RollRational(*rGrant.removeHostChance, rCtx.Rng()))
        {
            rCtx.rGameState.GetTileEffects().RemoveOccupantWithEffects(*rCtx.pTile,
                                                                       *rCtx.hostImprovementId);
        }
    }
};

// Whether the entry changed anything; oncePer is spent only on true.
bool ApplyOne_(const TriggeredEffectConfig_t& rConfig, TriggeredEffectContext_t& rCtx,
               Faction& rFaction, std::vector<TriggeredEffectResult_t>& rOut)
{
    return std::visit(TriggeredEffectApplier_{rConfig, rCtx, rFaction, rOut}, rConfig.effect);
}

void ApplyForFaction_(const TriggeredEffectConfig_t& rConfig, TriggeredEffectContext_t& rContext,
                      Faction* pFaction, std::vector<TriggeredEffectResult_t>& rResults)
{
    if (!pFaction)
    {
        return;
    }
    // Stamp before condition so a future faction-identity arm sees the apply subject.
    rContext.pFaction = pFaction;
    if (rConfig.condition
        && !ConditionSatisfied(*rConfig.condition, rContext.Subjects(), nullptr))
    {
        return;
    }
    std::set<std::string>* pConsumed = nullptr;
    if (rConfig.oncePer)
    {
        pConsumed = OnceSubjectFor_(*rConfig.oncePer, rContext, *pFaction);
        if (!pConsumed || pConsumed->count(rConfig.oncePer->key) != 0)
        {
            return;
        }
    }
    // Only a real change spends the key: an entry that found nothing to do has not
    // been used up, and the player should still get it when the situation changes.
    if (ApplyOne_(rConfig, rContext, *pFaction, rResults) && pConsumed)
    {
        pConsumed->insert(rConfig.oncePer->key);
    }
}

Faction* DefaultSubjectFaction_(const TriggeredEffectContext_t& rContext)
{
    if (rContext.pBase)
    {
        return &rContext.pBase->GetFaction();
    }
    return rContext.factions.empty() ? nullptr : rContext.factions.front();
}

void DestroySubjectIfKilled_(TriggeredEffectContext_t& rContext,
                             const std::vector<TriggeredEffectResult_t>& rResults,
                             std::size_t firstNew)
{
    if (!rContext.pUnit)
    {
        return;
    }
    const auto newResults = std::span(rResults).subspan(firstNew);
    const bool bKilled = std::ranges::any_of(newResults, [](const TriggeredEffectResult_t& rResult)
    {
        return std::holds_alternative<UnitDestroyed_t>(rResult);
    });
    if (!bKilled)
    {
        return;
    }
    Unit& rSubject = *rContext.pUnit;
    rContext.pUnit = nullptr;
    rSubject.GetFaction().GetUnitManager().DestroyUnit(rSubject);
}

// Snapshot first: GrantXp remove_host_chance can erase an improvement mid-loop. Config pointers
// stay valid (registry-owned) even after the tile loses the improvement.
std::vector<const ImprovementConfig_t*> VisitHosts_(const Tile& rTile)
{
    std::vector<const ImprovementConfig_t*> hosts;
    rTile.ForEachOccupant([&](const ImprovementConfig_t& rHost) {
        if (ImprovementHasVisitEffects(rHost))
        {
            hosts.push_back(&rHost);
        }
        return false;
    });
    return hosts;
}

} // namespace

std::vector<TriggeredEffectResult_t>
ApplyTriggeredEffects(std::span<const TriggeredEffectConfig_t> rEffects,
                      TriggeredEffectContext_t& rContext)
{
    Faction* const pFactionOnEntry = rContext.pFaction;
    std::vector<TriggeredEffectResult_t> results;
    for (const TriggeredEffectConfig_t& rConfig : rEffects)
    {
        const std::size_t resultsBefore = results.size();
        // Once per listed faction for a faction-subject entry, once otherwise (attributed to the
        // base's owner).
        if (IsPerFactionSubject_(rConfig.effect))
        {
            for (Faction* pFaction : rContext.factions)
            {
                ApplyForFaction_(rConfig, rContext, pFaction, results);
            }
        }
        else
        {
            ApplyForFaction_(rConfig, rContext, DefaultSubjectFaction_(rContext), results);
        }
        DestroySubjectIfKilled_(rContext, results, resultsBefore);
    }
    rContext.pFaction = pFactionOnEntry;
    return results;
}

void ApplyUnitProducedTriggers(GameState& rGameState, Unit& rUnit, BaseManager& rProducedAt)
{
    TriggeredEffectContext_t context(rGameState, rProducedAt);
    context.pUnit = &rUnit;

    const GameDataContext& rData = rGameState.GetGameData();
    if (rData.productionConfig)
    {
        ApplyTriggeredEffects(rData.productionConfig->onUnitProducedEffects, context);
    }
    for (const BuildingConfig_t* pBuilding : rProducedAt.GetBuildingManager().GetBuildings())
    {
        if (pBuilding)
        {
            ApplyTriggeredEffects(pBuilding->onUnitProducedEffects, context);
        }
    }
}

void ApplyVisitEffects(GameState& rGameState, Unit& rMover, std::mt19937& rRng)
{
    Tile* pTile = rGameState.GetWorldMap().GetTile(rMover.GetTile().GetCoord());
    if (!pTile)
    {
        throw std::runtime_error("ApplyVisitEffects: mover tile is not on the world map");
    }

    const std::vector<const ImprovementConfig_t*> hosts = VisitHosts_(*pTile);
    TriggeredEffectContext_t context(rGameState, rMover.GetFaction());
    context.pUnit = &rMover;
    context.pTile = pTile;
    context.pRng = &rRng;

    for (const ImprovementConfig_t* pConfig : hosts)
    {
        if (!pTile->HasFeature(pConfig->id))
        {
            continue;
        }
        context.hostImprovementId = pConfig->id;
        ApplyTriggeredEffects(pConfig->onVisitEffects, context);
    }
    context.hostImprovementId.reset();
}

void ApplyTechDiscoverEffects(GameState& rGameState, Faction& rFaction, const TechId& rTechId)
{
    const TechConfig_t* pTech = rGameState.GetGameData().techRegistry->Find(rTechId);
    if (!pTech || pTech->onDiscoverEffects.empty())
    {
        return;
    }
    TriggeredEffectContext_t context(rGameState, rFaction);
    ApplyTriggeredEffects(pTech->onDiscoverEffects, context);
}

void ApplyHoldLink(GameState& rGameState, Unit& rUnit)
{
    if (!UnitHasHoldLink(rUnit))
    {
        return;
    }
    BaseManager* pBase = rGameState.FindBaseAt(rUnit.GetTile());
    if (!pBase)
    {
        return;
    }

    Tile* pTile = rGameState.GetWorldMap().GetTile(rUnit.GetTile().GetCoord());
    TriggeredEffectContext_t context(rGameState, *pBase);
    context.pUnit = &rUnit;
    context.pTile = pTile;
    context.pRng = &rGameState.GetRng();

    const std::vector<TriggeredEffectConfig_t> effects = rUnit.GetDesign().CollectOnHoldEffects();
    ApplyTriggeredEffects(effects, context);
}

bool ApplyDetonation(GameState& rGameState, Unit& rUnit)
{
    const std::vector<TriggeredEffectConfig_t> effects =
        rUnit.GetDesign().CollectOnDetonateEffects();
    if (effects.empty())
    {
        return false;
    }

    Tile* pTile = rGameState.GetWorldMap().GetTile(rUnit.GetTile().GetCoord());
    if (!pTile)
    {
        throw std::logic_error("ApplyDetonation: the unit's tile is not on the world map");
    }

    TriggeredEffectContext_t context(rGameState, rUnit.GetFaction());
    context.pUnit = &rUnit;
    context.pTile = pTile;
    context.pRng = &rGameState.GetRng();

    ApplyTriggeredEffects(effects, context);
    return true;
}

} // namespace ac
