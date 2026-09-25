#include "game/effects/TriggeredEffectDispatch.h"

#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/buildings/BuildingConfig.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/InfiltrationRules.h"
#include "game/effects/TileEffectsContext.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/EconomyManager.h"
#include "game/faction/RebelFactionPicker.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/UnitManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/BuildingDestruction.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/production/ProductionConfigParser.h"
#include "game/map/ElevationChange.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/population/pop-types/PopCompositionConfigParser.h"
#include "game/units/AdHocDesign.h"
#include "game/units/MovementRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitOrder.h"
#include "lib/RandomRoll.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
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

// The subject that remembers a oncePer key. Absent when the context has no such subject — an
// entry scoped to a unit cannot fire from a trigger that has none, so it is skipped rather
// than silently treated as un-consumed and re-fired forever.
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

bool AddTech_(TriggeredEffectContext_t& rCtx, Faction& rFaction, const GrantTechEffect_t& rGrant,
              std::vector<TriggeredEffectResult_t>& rOut)
{
    TechId techId;
    if (rGrant.techId.has_value())
    {
        techId = *rGrant.techId;
    }
    else
    {
        const std::optional<TechId> picked =
            rFaction.GetResearch().PickRandomAvailableTech(rCtx.Rng());
        if (!picked)
        {
            return false;
        }
        techId = *picked;
    }

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

bool SetInfiltration_(TriggeredEffectContext_t& rCtx, const TriggeredEffectConfig_t& rConfig,
                      Faction& rBeneficiary, std::vector<TriggeredEffectResult_t>& rOut)
{
    const FactionId_t beneficiaryId = rBeneficiary.GetFactionId();
    DiplomacyLedger& rDiplomacy = rCtx.rGameState.GetDiplomacyLedger();
    InfiltrationSet_t set;
    for (const Faction& rFaction : rCtx.rGameState.Factions())
    {
        const FactionId_t candidateId = rFaction.GetFactionId();
        // No scope to consult, so an absent filter means every other faction.
        if (FactionFilterCoversTarget(rConfig.factionFilter, /*bDefaultCoversAllOthers=*/true,
                                      beneficiaryId, candidateId, rCtx.rGameState,
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
    const GameDataContext& rData = rCtx.pBase->GetFaction().GetDataContext();
    if (!rData.popCompositionConfig)
    {
        throw std::runtime_error("ApplyTriggeredEffects: Rebel requires a popCompositionConfig");
    }
    const BaseId_t baseId = rCtx.pBase->GetBaseId();
    const std::optional<FactionId_t> newOwner =
        PickRebelFactionAndTransfer(*rCtx.pBase, rCtx.rGameState,
                                    rData.popCompositionConfig->rebelSelection, rCtx.Rng());
    if (!newOwner)
    {
        return false;
    }
    rOut.push_back(BaseRebelled_t{baseId, *newOwner});
    return true;
}

// Where a granted unit comes into the world. The trigger may have no base at all (a council
// vote), so the anchor is resolved in order of how specific the context is; the unit is homed
// there, and so pays support like a produced one.
BaseManager* ResolveAnchorBase_(TriggeredEffectContext_t& rCtx, Faction& rFaction)
{
    if (rCtx.pBase && &rCtx.pBase->GetFaction() == &rFaction)
    {
        return rCtx.pBase;
    }
    if (rCtx.pTile)
    {
        if (BaseManager* pNearest = rFaction.FindNearestBase(*rCtx.pTile))
        {
            return pNearest;
        }
    }
    if (BaseManager* pHq = rFaction.GetHeadquarters())
    {
        return pHq;
    }
    for (BaseManager& rBase : rFaction.Bases())
    {
        return &rBase;
    }
    return nullptr;
}

// IDesign-level stand-in for CanHoldTileWithoutCarrier, which needs a live Unit and so cannot
// be asked before the unit exists. A tile that harbors the domain for this faction always
// works (its own base, a port); otherwise the tile's own surface has to suit the domain.
bool DesignCanHoldTile_(const UnitDesign& rDesign, const Tile& rTile, FactionId_t factionId,
                        const WorldMap& rMap)
{
    const UnitDomain_t domain = rDesign.GetDomain();
    if (TileHarbors(rTile, domain, factionId, rMap.GetTerritory()))
    {
        return true;
    }
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

// Tiles a unit of rDesign may be placed on, nearest first: the anchor tile itself, then
// outward rings. Mirrors SpawnEscapePods_' ring search, but ordered rather than shuffled —
// a granted unit should appear as close to its base as it can.
std::vector<Tile*> PlaceableTilesNear_(const Tile& rOrigin, const UnitDesign& rDesign,
                                       WorldMap& rMap, FactionId_t factionId, int needed)
{
    UnitPositionIndex& rPositions = rMap.GetUnitPositions();
    std::vector<Tile*> found;
    const auto consider = [&](Tile* pTile)
    {
        if (!pTile || !CanPlaceUnitOnTile(*pTile, rPositions))
        {
            return;
        }
        if (!DesignCanHoldTile_(rDesign, *pTile, factionId, rMap))
        {
            return;
        }
        found.push_back(pTile);
    };

    consider(rMap.GetTile(rOrigin.GetX(), rOrigin.GetY()));
    // The hardcoded 2 is terrible design, but I think we'll get rid of single-tile mode
    for (int radius = 1; found.size() < static_cast<size_t>(needed) && radius <= 2; ++radius)
    {
        ForEachTileInChebyshevRadius(rOrigin, rMap, radius, /*includeOrigin=*/false,
            [&](Tile* pTile, int distance)
            {
                if (distance == radius)
                {
                    consider(pTile);
                }
            });
    }
    return found;
}

bool GrantUnit_(TriggeredEffectContext_t& rCtx, const GrantUnitEffect_t& rGrant,
                Faction& rFaction, std::vector<TriggeredEffectResult_t>& rOut)
{
    BaseManager* pAnchor = ResolveAnchorBase_(rCtx, rFaction);
    if (!pAnchor || rGrant.count <= 0)
    {
        rOut.push_back(UnitsGranted_t{0, {}});
        return false;
    }

    const GameDataContext& rData = rFaction.GetDataContext();
    const UnitDesign* pDesign =
        EnsureAdHocDesign(rFaction, rData, rGrant.componentIds, "granted");
    if (!pDesign)
    {
        rOut.push_back(UnitsGranted_t{0, {}});
        return false;
    }

    WorldMap& rMap = rCtx.rGameState.GetWorldMap();
    const std::vector<Tile*> tiles = PlaceableTilesNear_(pAnchor->GetTile(), *pDesign, rMap,
                                                         rFaction.GetFactionId(), rGrant.count);

    // A full map grants fewer units rather than throwing: the caller is told the real count.
    int spawned = 0;
    for (Tile* pTile : tiles)
    {
        if (spawned >= rGrant.count)
        {
            break;
        }
        // Homed at the anchor so it pays support, but explicitly produced nowhere: a gift is
        // not a build, so it receives no on_unit_produced train bonuses and no prototype latch.
        rFaction.GetUnitManager().CreateUnit(rCtx.rGameState.AllocateUnitId(), *pDesign,
                                             rMap.GetUnitPositions(), *pTile, pAnchor,
                                             /*pProducedAt=*/nullptr);
        ++spawned;
    }
    rOut.push_back(UnitsGranted_t{spawned, pDesign->GetId()});
    return spawned > 0;
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
        levels = rCtx.pUnit->GetStat(*rConfig.levelsStat);
    }
    // The tile already carries the map rules WorldMap bound to it, so the quake needs no
    // GameDataContext of its own.
    if (!ApplyEarthquake(*rCtx.pTile, rCtx.rGameState.GetWorldMap(), levels, rCtx.Rng(),
                         rCtx.pTile->MapRules()))
    {
        return false;
    }
    rOut.push_back(EarthquakeApplied_t{levels});
    return true;
}

// Which subject an entry acts on. A faction-subject effect applies once per faction in the
// context — that is what makes a council GrantEnergy credit every member. A base-, unit- or
// world-subject effect has exactly one subject and applies once, however many factions the
// context lists, so a DestroyFacility cannot hit the same base once per member.
bool IsPerFactionSubject_(const TriggeredEffectVariant_t& rEffect)
{
    return std::visit(
        [](const auto& rConcrete)
        {
            using T = std::decay_t<decltype(rConcrete)>;
            if constexpr (std::is_same_v<T, GrantTechEffect_t>
                          || std::is_same_v<T, GrantEnergyEffect_t>
                          || std::is_same_v<T, GrantUnitEffect_t>
                          || std::is_same_v<T, SetInfiltrationEffect_t>)
            {
                return true;
            }
            else if constexpr (std::is_same_v<T, AddBuildingEffect_t>
                               || std::is_same_v<T, ModifyPopulationEffect_t>
                               || std::is_same_v<T, GrantXpEffect_t>
                               || std::is_same_v<T, RestoreHitPointsEffect_t>
                               || std::is_same_v<T, DestroyFacilityEffect_t>
                               || std::is_same_v<T, RebelEffect_t>
                               || std::is_same_v<T, DestroyUnitEffect_t>
                               || std::is_same_v<T, EarthquakeEffect_t>
                               || std::is_same_v<T, WorldParameterEffect_t>)
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

// Returns whether the entry actually changed anything. A `oncePer` key is only spent on a
// true — an entry that found nothing to do (no eligible facility, no placeable tile, a tech
// the faction already knew) has not been used up and may fire again later.
bool ApplyOne_(const TriggeredEffectConfig_t& rConfig, TriggeredEffectContext_t& rCtx,
               Faction& rFaction, std::vector<TriggeredEffectResult_t>& rOut)
{
    return std::visit(
        [&](const auto& rConcrete) -> bool
        {
            using T = std::decay_t<decltype(rConcrete)>;
            if constexpr (std::is_same_v<T, AddBuildingEffect_t>)
            {
                return AddBuilding_(rCtx, rConcrete, rOut);
            }
            else if constexpr (std::is_same_v<T, GrantTechEffect_t>)
            {
                return AddTech_(rCtx, rFaction, rConcrete, rOut);
            }
            else if constexpr (std::is_same_v<T, GrantUnitEffect_t>)
            {
                return GrantUnit_(rCtx, rConcrete, rFaction, rOut);
            }
            else if constexpr (std::is_same_v<T, GrantEnergyEffect_t>)
            {
                rFaction.GetEconomy().AddEnergy(rConcrete.amount);
                rOut.push_back(EnergyGranted_t{rConcrete.amount});
                return true;
            }
            else if constexpr (std::is_same_v<T, WorldParameterEffect_t>)
            {
                // TODO: world-map mutation (sea level, climate) unfolds gradually over turns
                // through the WorldEvents system. Once that system exposes a trigger API,
                // request the world event here. The council must never mutate the map itself.
                // Reports false meanwhile, so a oncePer key is not spent on the no-op.
                return false;
            }
            else if constexpr (std::is_same_v<T, SetInfiltrationEffect_t>)
            {
                return SetInfiltration_(rCtx, rConfig, rFaction, rOut);
            }
            else if constexpr (std::is_same_v<T, ModifyPopulationEffect_t>)
            {
                if (!rCtx.pBase)
                {
                    return false;
                }
                const int delta = ApplyModifyPopulation(*rCtx.pBase, rConcrete);
                rOut.push_back(PopulationChanged_t{delta});
                return delta != 0;
            }
            else if constexpr (std::is_same_v<T, GrantXpEffect_t>)
            {
                if (!rCtx.pUnit)
                {
                    return false;
                }
                const int before = rCtx.pUnit->GetXp();
                const double next = ApplyModifierStack(
                    static_cast<double>(before),
                    {{static_cast<double>(rConcrete.amount), rConcrete.op}});
                rCtx.pUnit->SetXp(static_cast<int>(std::lround(next)));
                const int granted = rCtx.pUnit->GetXp() - before;
                rOut.push_back(XpGranted_t{granted});
                if (granted > 0 && rConcrete.removeHostChance && rCtx.hostImprovementId
                    && rCtx.pTile)
                {
                    if (RollRational(*rConcrete.removeHostChance, rCtx.Rng()))
                    {
                        rCtx.rGameState.GetTileEffects().RemoveImprovementWithEffects(
                            *rCtx.pTile, *rCtx.hostImprovementId);
                    }
                }
                return granted != 0;
            }
            else if constexpr (std::is_same_v<T, RestoreHitPointsEffect_t>)
            {
                if (!rCtx.pUnit)
                {
                    return false;
                }
                Unit& rUnit = *rCtx.pUnit;
                const int before = rUnit.GetCurrentHp();
                const int maxHp = ResolveStat(rUnit, StatId_t::HitPoints);
                int next = before;
                switch (rConcrete.op)
                {
                case RestoreHitPointsOp_t::Add:
                    next = before + rConcrete.amount;
                    break;
                case RestoreHitPointsOp_t::AddPercent:
                    next = before + (maxHp * rConcrete.amount) / 100;
                    break;
                case RestoreHitPointsOp_t::MaxClamp:
                    next = std::min(before, rConcrete.amount);
                    break;
                case RestoreHitPointsOp_t::MinClamp:
                    next = std::max(before, rConcrete.amount);
                    break;
                case RestoreHitPointsOp_t::SetPercent:
                    next = std::max(before, (maxHp * rConcrete.amount) / 100);
                    break;
                }
                rUnit.SetCurrentHp(next);
                const int restored = rUnit.GetCurrentHp() - before;
                rOut.push_back(HitPointsRestored_t{restored});
                return restored != 0;
            }
            else if constexpr (std::is_same_v<T, DestroyFacilityEffect_t>)
            {
                if (!rCtx.pBase)
                {
                    return false;
                }
                std::vector<BuildingId_t> destroyed = DestroyRandomFacilities(
                    *rCtx.pBase, rConcrete.count, rConcrete.excludeHq,
                    rConcrete.excludeSecretProjects, rCtx.Rng());
                const bool bDidAnything = !destroyed.empty();
                rOut.push_back(FacilitiesDestroyed_t{std::move(destroyed)});
                return bDidAnything;
            }
            else if constexpr (std::is_same_v<T, RebelEffect_t>)
            {
                return Rebel_(rCtx, rOut);
            }
            else if constexpr (std::is_same_v<T, EarthquakeEffect_t>)
            {
                return Earthquake_(rCtx, rConcrete, rOut);
            }
            else if constexpr (std::is_same_v<T, DestroyUnitEffect_t>)
            {
                if (!rCtx.pUnit)
                {
                    return false;
                }
                rOut.push_back(UnitDestroyed_t{rCtx.pUnit->GetUnitId()});
                return true;
            }
            else
            {
                static_assert(k_AlwaysFalse<T>);
            }
        },
        rConfig.effect);
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
        // One application per subject: every listed faction for a faction-subject entry, and
        // exactly one for a base- or unit-subject entry (attributed to the base's owner, which
        // need not be a listed faction — a probe mission acts on the target's base).
        const auto applyFor = [&](Faction* pFaction)
        {
            if (!pFaction)
            {
                return;
            }
            // Stamp before condition so a future faction-identity arm sees the apply subject.
            rContext.pFaction = pFaction;
            if (rConfig.condition
                && !ConditionSatisfied(*rConfig.condition, rContext.Subjects()))
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
            if (ApplyOne_(rConfig, rContext, *pFaction, results) && pConsumed)
            {
                pConsumed->insert(rConfig.oncePer->key);
            }
        };

        const std::size_t resultsBefore = results.size();
        if (IsPerFactionSubject_(rConfig.effect))
        {
            for (Faction* pFaction : rContext.factions)
            {
                applyFor(pFaction);
            }
        }
        else
        {
            applyFor(rContext.pBase ? &rContext.pBase->GetFaction()
                                    : (rContext.factions.empty() ? nullptr
                                                                 : rContext.factions.front()));
        }
        for (std::size_t i = resultsBefore; i < results.size(); ++i)
        {
            if (!std::holds_alternative<UnitDestroyed_t>(results[i]) || !rContext.pUnit)
            {
                continue;
            }
            Unit& rSubject = *rContext.pUnit;
            rContext.pUnit = nullptr;
            rSubject.GetFaction().GetUnitManager().DestroyUnit(rSubject);
            break;
        }
    }
    rContext.pFaction = pFactionOnEntry;
    return results;
}

void ApplyUnitProducedTriggers(GameState& rGameState, Unit& rUnit, BaseManager& rProducedAt)
{
    TriggeredEffectContext_t context(rGameState, rProducedAt);
    context.pUnit = &rUnit;

    const GameDataContext& rData = rProducedAt.GetFaction().GetDataContext();
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

bool TileHasVisitEffects(const Tile& rTile)
{
    for (const ImprovementConfig_t* pImprovement : rTile.GetImprovements())
    {
        if (pImprovement && !pImprovement->onVisitEffects.empty())
        {
            return true;
        }
    }
    return false;
}

void ApplyVisitEffects(GameState& rGameState, Unit& rMover, std::mt19937& rRng)
{
    Tile* pTile = rGameState.GetWorldMap().GetTile(rMover.GetTile().GetX(), rMover.GetTile().GetY());
    if (!pTile)
    {
        throw std::runtime_error("ApplyVisitEffects: mover tile is not on the world map");
    }

    // Snapshot hosts first: GrantXp remove_host_chance can erase an improvement mid-loop.
    // Config pointers stay valid (registry-owned) even after the tile loses the improvement.
    std::vector<const ImprovementConfig_t*> hosts;
    for (const ImprovementConfig_t* pImprovement : pTile->GetImprovements())
    {
        if (pImprovement && !pImprovement->onVisitEffects.empty())
        {
            hosts.push_back(pImprovement);
        }
    }

    TriggeredEffectContext_t context(rGameState, rMover.GetFaction());
    context.pUnit = &rMover;
    context.pTile = pTile;
    context.pRng = &rRng;

    for (const ImprovementConfig_t* pConfig : hosts)
    {
        if (!pTile->HasImprovement(pConfig->id))
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
    const TechConfig_t* pTech = rFaction.GetResearch().GetTechRegistry().Find(rTechId);
    if (!pTech || pTech->onDiscoverEffects.empty())
    {
        return;
    }
    TriggeredEffectContext_t context(rGameState, rFaction);
    ApplyTriggeredEffects(pTech->onDiscoverEffects, context);
}

namespace
{

bool HoldOrderActive_(const Unit& rUnit)
{
    const std::optional<UnitOrder_t>& rOrder = rUnit.GetOrder();
    return rOrder.has_value() && std::holds_alternative<HoldOrder_t>(*rOrder);
}

const BaseManager* OwnBaseAt_(const GameState& rGameState, const Unit& rUnit)
{
    const BaseManager* pBase =
        rGameState.FindBaseAt(rUnit.GetTile().GetX(), rUnit.GetTile().GetY());
    if (!pBase || pBase->GetFaction().GetFactionId() != rUnit.GetFaction().GetFactionId())
    {
        return nullptr;
    }
    return pBase;
}

bool HoldEffectMatches_(const TriggeredEffectConfig_t& rEffect, const Unit& rUnit,
                        const BaseManager& rBase)
{
    if (!rEffect.condition)
    {
        return true;
    }
    EffectContext_t ctx;
    ctx.pUnit = &rUnit;
    ctx.pBase = &rBase;
    ctx.pFaction = &rUnit.GetFaction();
    ctx.targetTile = &rUnit.GetTile();
    return ConditionSatisfied(*rEffect.condition, ctx);
}

void CollectBaseHasBuilding_(const Condition_t& rCondition, std::vector<std::string>& rIds)
{
    std::visit(
        [&](const auto& rAlt)
        {
            using T = std::decay_t<decltype(rAlt)>;
            if constexpr (std::is_same_v<T, BaseHasBuilding_t>)
            {
                rIds.push_back(rAlt.buildingId);
            }
            else if constexpr (std::is_same_v<T, AllOf_t>)
            {
                for (const Condition_t& rNested : rAlt.conditions)
                {
                    CollectBaseHasBuilding_(rNested, rIds);
                }
            }
            else
            {
                (void)rAlt;
            }
        },
        rCondition.AsVariant());
}

std::string HostNameForEffect_(const TriggeredEffectConfig_t& rEffect, const BaseManager& rBase)
{
    if (rEffect.condition)
    {
        std::vector<std::string> buildingIds;
        CollectBaseHasBuilding_(*rEffect.condition, buildingIds);
        for (const std::string& rId : buildingIds)
        {
            if (const BuildingConfig_t* pBuilding = rBase.GetBuildingManager().FindBuilding(rId))
            {
                return pBuilding->name.empty() ? pBuilding->id : pBuilding->name;
            }
        }
    }
    return rBase.GetName();
}

} // namespace

bool UnitHasHoldLink(const GameState& rGameState, const Unit& rUnit)
{
    if (!HoldOrderActive_(rUnit))
    {
        return false;
    }
    const BaseManager* pBase = OwnBaseAt_(rGameState, rUnit);
    if (!pBase)
    {
        return false;
    }
    for (const TriggeredEffectConfig_t& rEffect : rUnit.GetDesign().CollectOnHoldEffects())
    {
        if (HoldEffectMatches_(rEffect, rUnit, *pBase))
        {
            return true;
        }
    }
    return false;
}

std::string HoldLinkHostName(const GameState& rGameState, const Unit& rUnit)
{
    if (!HoldOrderActive_(rUnit))
    {
        return {};
    }
    const BaseManager* pBase = OwnBaseAt_(rGameState, rUnit);
    if (!pBase)
    {
        return {};
    }
    for (const TriggeredEffectConfig_t& rEffect : rUnit.GetDesign().CollectOnHoldEffects())
    {
        if (HoldEffectMatches_(rEffect, rUnit, *pBase))
        {
            return HostNameForEffect_(rEffect, *pBase);
        }
    }
    return {};
}

void ApplyHoldLink(GameState& rGameState, Unit& rUnit)
{
    if (!UnitHasHoldLink(rGameState, rUnit))
    {
        return;
    }
    BaseManager* pBase = rGameState.FindBaseAt(rUnit.GetTile().GetX(), rUnit.GetTile().GetY());
    if (!pBase)
    {
        return;
    }

    Tile* pTile = rGameState.GetWorldMap().GetTile(rUnit.GetTile().GetX(), rUnit.GetTile().GetY());
    TriggeredEffectContext_t context(rGameState, *pBase);
    context.pUnit = &rUnit;
    context.pTile = pTile;
    context.pRng = &rGameState.GetRng();

    const std::vector<TriggeredEffectConfig_t> effects = rUnit.GetDesign().CollectOnHoldEffects();
    ApplyTriggeredEffects(effects, context);
}

bool UnitCanDetonate(const Unit& rUnit)
{
    return !rUnit.GetDesign().CollectOnDetonateEffects().empty();
}

bool ApplyDetonation(GameState& rGameState, Unit& rUnit)
{
    const std::vector<TriggeredEffectConfig_t> effects =
        rUnit.GetDesign().CollectOnDetonateEffects();
    if (effects.empty())
    {
        return false;
    }

    Tile* pTile = rGameState.GetWorldMap().GetTile(rUnit.GetTile().GetX(), rUnit.GetTile().GetY());
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
