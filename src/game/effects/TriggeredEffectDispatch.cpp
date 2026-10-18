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
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/economy/EconomyManager.h"
#include "game/base/RebelMutations.h"
#include "game/research/ResearchManager.h"
#include "game/research/ResearchRules.h"
#include "game/units/UnitManager.h"
#include "game/base/BaseManager.h"
#include "game/buildings/BuildingMutations.h"
#include "game/buildings/BuildingManager.h"
#include "game/production/ProductionConfigParser.h"
#include "game/terraform/ElevationChange.h"
#include "game/terraform/TerraformRules.h"
#include "game/combat/ExplosionMutations.h"
#include "game/combat/ExplosionRules.h"
#include "game/ecology/FungalBloom.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/MapUtils.h"
#include "game/map/RiverGeneration.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorldMap.h"
#include "game/population/PopCompositionConfigParser.h"
#include "game/population/PopulationManager.h"
#include "game/units/AdHocDesign.h"
#include "game/natives/NativeUnitRegistry.h"
#include "game/movement/MovementRules.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitPlacementRules.h"
#include "lib/RandomRoll.h"

#include <algorithm>
#include <cmath>
#include <magic_enum.hpp>
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
        const std::vector<const TechConfig_t*> available = AvailableTechs(rFaction.GetResearch());
        if (available.empty())
        {
            return false;
        }
        std::uniform_int_distribution<std::size_t> dist(0, available.size() - 1);
        const std::size_t picked = dist(rCtx.Rng());
        techId = available[picked]->id;
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

// The subject faction is the perpetrator; the victim is whoever actionTarget names (the probed
// base's owner, or the territory a warhead went off in). A victimless act is legal — an atrocity
// is answered for on the strength of the act, not on finding someone to have wronged.
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
        // No scope to consult, so an absent filter means every other faction.
        if (FactionFilterCoversTarget(rConfig.factionFilter, /*bDefaultCoversAllOthers=*/true,
                                      beneficiaryId, candidateId, rCtx.actionTarget))
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

bool GrantUnit_(TriggeredEffectContext_t& rCtx, const GrantUnitEffect_t& rGrant,
                Faction& rFaction, std::vector<TriggeredEffectResult_t>& rOut)
{
    // A grant may have no base at all (a council vote); the anchor homes the unit, so it pays
    // support like a produced one.
    const std::optional<BaseId_t> anchorId = GrantAnchorBase(rFaction, rCtx.pBase, rCtx.pTile);
    BaseManager* pAnchor = anchorId ? rFaction.FindBase(*anchorId) : nullptr;
    if (!pAnchor || rGrant.count <= 0)
    {
        rOut.push_back(UnitsGranted_t{0, {}});
        return false;
    }

    const UnitDesign* pDesign =
        EnsureAdHocDesign(rFaction, *rCtx.rGameState.GetGameData().unitComponentRegistry,
                          rGrant.componentIds, "granted");
    if (!pDesign)
    {
        rOut.push_back(UnitsGranted_t{0, {}});
        return false;
    }

    WorldMap& rMap = rCtx.rGameState.GetWorldMap();
    const std::vector<const Tile*> tiles = PlacementTiles(
        pAnchor->GetTile(), *pDesign, rFaction.GetFactionId(), rGrant.count, rMap);

    // A full map grants fewer units rather than throwing: the caller is told the real count.
    int spawned = 0;
    for (const Tile* pTile : tiles)
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
                         rCtx.pTile->MapRules(), &rCtx.rGameState.GetTileEffects(),
                         &rCtx.rGameState))
    {
        return false;
    }
    rOut.push_back(EarthquakeApplied_t{levels});
    return true;
}

// Aquifer and river are tile bools; every other terrain occupant is a config pointer in the
// tile's optional-terrain list. Terrain derived from other state has no branch here and is
// rejected at load.
bool SetTerrainFeature_(TriggeredEffectContext_t& rCtx, const SetTerrainFeatureEffect_t& rConfig,
                        std::vector<TriggeredEffectResult_t>& rOut)
{
    if (!rCtx.pTile)
    {
        return false;
    }
    Tile& rTile = *rCtx.pTile;
    TileEffectsContext& rTileEffects = rCtx.rGameState.GetTileEffects();
    const std::string_view aquifer = magic_enum::enum_name(TerrainFeature_t::Aquifer);
    const std::string_view river = magic_enum::enum_name(TerrainFeature_t::River);

    bool bChanged = false;
    if (rConfig.featureId == aquifer)
    {
        bChanged = rTile.GetHasAquifer() != rConfig.bPresent;
        if (bChanged)
        {
            rTile.SetHasAquifer(rConfig.bPresent);
            RecomputeRivers(rCtx.rGameState.GetWorldMap());
        }
    }
    else if (rConfig.featureId == river)
    {
        bChanged = rTile.GetHasRiver() != rConfig.bPresent;
        if (bChanged)
        {
            rTile.SetHasRiver(rConfig.bPresent);
        }
    }
    else
    {
        bChanged = rTile.HasTerrainFeature(rConfig.featureId) != rConfig.bPresent;
        if (bChanged)
        {
            if (rConfig.bPresent)
            {
                rTileEffects.AddOccupantWithEffects(rTile, rConfig.featureId);
            }
            else
            {
                rTileEffects.RemoveOccupantWithEffects(rTile, rConfig.featureId);
            }
        }
    }

    if (!bChanged)
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
    const int current = static_cast<int>(*magic_enum::enum_index(rCtx.pTile->GetRockiness()));
    const int stepped =
        std::clamp(current + rConfig.steps, 0, static_cast<int>(rValues.size()) - 1);
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
    const ElevationRulesConfig_t& rRules = rCtx.pTile->MapRules();
    const UnitDomain_t domain = rCtx.pUnit->GetDomain();
    // No room check here: whether a Former may start is FormerElevationChangeAllowed's job at
    // order time. By the time this fires the energy is spent and the turns are gone, so a roll
    // past the floor lands on the floor rather than failing, and only a delta that changes
    // nothing at all reports false.
    const int roll = RollLevelMeters(rCtx.Rng(), rRules);
    const int delta = rConfig.bRaise ? roll : -roll;
    const int floor =
        rConfig.bRaise ? rRules.minElevationMeters : FormerLowerFloorMeters(domain, rRules);
    if (!ApplyElevationDelta(*rCtx.pTile, rCtx.rGameState.GetWorldMap(), delta, rRules, floor,
                             rRules.maxElevationMeters, &rCtx.rGameState.GetTileEffects(),
                             &rCtx.rGameState))
    {
        return false;
    }
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
    int tiles = rConfig.tiles;
    if (rConfig.tilesStat)
    {
        if (!rCtx.pUnit)
        {
            return false;
        }
        tiles = rCtx.pUnit->GetStat(*rConfig.tilesStat);
    }
    const FungalBloomResult_t result = ApplyFungalBloom(
        *rCtx.pTile, rCtx.rGameState.GetWorldMap(), tiles, rCtx.Rng(), rCtx.rGameState);
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
                          || std::is_same_v<T, SetInfiltrationEffect_t>
                          || std::is_same_v<T, CommitAtrocityEffect_t>
                          || std::is_same_v<T, RecordMindControlEffect_t>
                          || std::is_same_v<T, GrantCleanMineralsEffect_t>
                          || std::is_same_v<T, AddVirtualMineralsEffect_t>)
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
                               || std::is_same_v<T, FungalBloomEffect_t>
                               || std::is_same_v<T, ExplosionEffect_t>
                               || std::is_same_v<T, SetTerrainFeatureEffect_t>
                               || std::is_same_v<T, StepRockinessEffect_t>
                               || std::is_same_v<T, ElevationChangeEffect_t>
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

// Apply one ModifyPopulation. Returns the signed size delta actually applied (negative when
// pops were removed). Honors minSize when shrinking; stops adding when CanGrow() is false.
int ApplyModifyPopulation_(BaseManager& rBase, const ModifyPopulationEffect_t& rEffect)
{
    PopulationManager& rPops = rBase.GetPopulation();
    const int size = rPops.GetSize();
    const int delta = PredictModifyPopulationDelta(size, rEffect);

    if (delta < 0)
    {
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
            else if constexpr (std::is_same_v<T, CommitAtrocityEffect_t>)
            {
                return CommitAtrocity_(rCtx, rConcrete, rFaction, rOut);
            }
            else if constexpr (std::is_same_v<T, RecordMindControlEffect_t>)
            {
                rCtx.rGameState.GetMindControlLedger().Record(rFaction.GetFactionId(),
                                                              rConcrete.weight);
                rOut.push_back(MindControlRecorded_t{rConcrete.weight});
                return true;
            }
            else if constexpr (std::is_same_v<T, GrantCleanMineralsEffect_t>)
            {
                EcologyLedger& rEcology = rCtx.rGameState.GetEcologyLedger();
                if (rConcrete.bRequiresFirstBloom
                    && rEcology.FungalBlooms(rFaction.GetFactionId()) == 0)
                {
                    return false;
                }
                rEcology.GrantCleanMinerals(rFaction.GetFactionId(), rConcrete.amount);
                rOut.push_back(CleanMineralsGranted_t{rConcrete.amount});
                return true;
            }
            else if constexpr (std::is_same_v<T, AddVirtualMineralsEffect_t>)
            {
                rCtx.rGameState.GetEcologyLedger().AddVirtualMinerals(rFaction.GetFactionId(),
                                                                      rConcrete.amount);
                rOut.push_back(VirtualMineralsAdded_t{rConcrete.amount});
                return true;
            }
            else if constexpr (std::is_same_v<T, ModifyPopulationEffect_t>)
            {
                if (!rCtx.pBase)
                {
                    return false;
                }
                const int delta = ApplyModifyPopulation_(*rCtx.pBase, rConcrete);
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
                        rCtx.rGameState.GetTileEffects().RemoveOccupantWithEffects(
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
            else if constexpr (std::is_same_v<T, FungalBloomEffect_t>)
            {
                return FungalBloom_(rCtx, rConcrete, rOut);
            }
            else if constexpr (std::is_same_v<T, ExplosionEffect_t>)
            {
                return Explosion_(rCtx, rConcrete, rOut);
            }
            else if constexpr (std::is_same_v<T, SetTerrainFeatureEffect_t>)
            {
                return SetTerrainFeature_(rCtx, rConcrete, rOut);
            }
            else if constexpr (std::is_same_v<T, StepRockinessEffect_t>)
            {
                return StepRockiness_(rCtx, rConcrete, rOut);
            }
            else if constexpr (std::is_same_v<T, ElevationChangeEffect_t>)
            {
                return ElevationChange_(rCtx, rConcrete, rOut);
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
    Tile* pTile = rGameState.GetWorldMap().GetTile(rMover.GetTile().GetX(), rMover.GetTile().GetY());
    if (!pTile)
    {
        throw std::runtime_error("ApplyVisitEffects: mover tile is not on the world map");
    }

    // Snapshot hosts first: GrantXp remove_host_chance can erase an improvement mid-loop.
    // Config pointers stay valid (registry-owned) even after the tile loses the improvement.
    std::vector<const ImprovementConfig_t*> hosts;
    pTile->ForEachOccupant([&](const ImprovementConfig_t& rHost) {
        if (ImprovementHasVisitEffects(rHost))
        {
            hosts.push_back(&rHost);
        }
        return false;
    });

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
