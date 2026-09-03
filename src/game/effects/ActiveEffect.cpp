#include "game/effects/ActiveEffect.h"

#include "game/IEffectsProvider.h"
#include "game/Faction.h"
#include "game/GameState.h"
#include "game/buildings/BuildingConfig.h"
#include "game/effects/InfiltrationRules.h"
#include "game/buildings/BuildingRegistry.h"
#include "game/faction/RebelFactionPicker.h"
#include "game/faction/SocialEngineeringManager.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/BuildingDestruction.h"
#include "game/faction/base/buildings/BuildingManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/population/pop-types/Pop.h"
#include "game/population/pop-types/PopCompositionConfigParser.h"
#include "game/population/pop-types/PopTypeConfigParser.h"
#include "game/GameDataContext.h"
#include "game/social-engineering/SocialPolicyConfig.h"
#include "game/units/UnitComponentConfig.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/TileYieldRulesConfig.h"
#include <magic_enum.hpp>
#include <algorithm>
#include <cmath>
#include <span>
#include <stdexcept>
#include <iostream>
#include <set>
#include <limits>
#include <string_view>
#include <type_traits>
#include <variant>

namespace ac
{

namespace
{

// A source reached a subject overload with no evaluation for it. Unreachable from the
// EffectContext_t dispatch, which routes every source to its own subject — this fires only if
// a new source's case there names a subject whose overload was never taught to evaluate it,
// so the message says which pairing is missing rather than "wrong subject".
[[noreturn]] void ThrowNoEvaluation_(StatModifierEffect_t::AmountSource_t source,
                                     std::string_view subject)
{
    throw std::runtime_error("AmountSourceValue: amount_source "
                             + std::string(magic_enum::enum_name(source))
                             + " has no evaluation for a " + std::string(subject) + " subject");
}

} // namespace

UnitEffects_t::UnitEffects_t(const Unit& rUnit)
    : pUnit(&rUnit)
    , pDesign(&rUnit.GetDesign())
{
}

UnitEffects_t::UnitEffects_t(const UnitDesign& rDesign)
    : pUnit(nullptr)
    , pDesign(&rDesign)
{
}

UnitEffects_t::UnitEffects_t(const Unit& rUnit, std::vector<ActiveEffect_t> effectsIn)
    : pUnit(&rUnit)
    , pDesign(&rUnit.GetDesign())
    , effects(std::move(effectsIn))
{
}

UnitEffects_t::UnitEffects_t(const UnitDesign& rDesign, std::vector<ActiveEffect_t> effectsIn)
    : pUnit(nullptr)
    , pDesign(&rDesign)
    , effects(std::move(effectsIn))
{
}

double AmountSourceValue(StatModifierEffect_t::AmountSource_t source, double scale,
                         const BaseManager& rBase)
{
    switch (source)
    {
        case StatModifierEffect_t::AmountSource_t::BaseSize:
            // Vanilla per-source floor: University floor(size×0.25) does not share fractional
            // residue with another fractional BaseSize source (e.g. Commons).
            return std::floor(static_cast<double>(rBase.GetPopulation().GetSize()) * scale);
        case StatModifierEffect_t::AmountSource_t::BuildingUpkeep:
            return static_cast<double>(rBase.GetBuildingUpkeep()) * scale;
        default:
            ThrowNoEvaluation_(source, "Base");
    }
}

double AmountSourceValue(StatModifierEffect_t::AmountSource_t source, double scale,
                         const Tile& rTile, const TileYieldRulesConfig_t& rYieldRules)
{
    switch (source)
    {
        case StatModifierEffect_t::AmountSource_t::ElevationEnergy:
        {
            if (rYieldRules.elevationEnergyStepMeters <= 0)
            {
                throw std::runtime_error(
                    "AmountSourceValue: elevation_energy_step_meters must be > 0");
            }
            // Rounded up, so any land above sea level is worth at least one point. Clamped
            // at 0 rather than reading a negative band off the sea floor — an ocean tile
            // yields nothing here, whatever a mod lets you build on it.
            const double bands = std::ceil(static_cast<double>(rTile.GetElevation())
                                           / static_cast<double>(rYieldRules.elevationEnergyStepMeters));
            return std::max(0.0, bands) * scale;
        }
        default:
            ThrowNoEvaluation_(source, "Tile");
    }
}

double AmountSourceValue(StatModifierEffect_t::AmountSource_t source, double scale,
                         const StockpileConversionSubject_t& rStockpile)
{
    switch (source)
    {
        case StatModifierEffect_t::AmountSource_t::MineralsConverted:
            return static_cast<double>(rStockpile.mineralsConverted) * scale;
        default:
            ThrowNoEvaluation_(source, "Stockpile");
    }
}

double AmountSourceValue(StatModifierEffect_t::AmountSource_t source, double scale,
                         const Faction& rFaction)
{
    switch (source)
    {
        case StatModifierEffect_t::AmountSource_t::BasesOwned:
            return static_cast<double>(rFaction.GetBaseCount()) * scale;
        default:
            ThrowNoEvaluation_(source, "Faction");
    }
}

double AmountSourceValue(const StatModifierEffect_t& rMod, const EffectContext_t* pCtx)
{
    if (!rMod.amountSource.has_value())
    {
        return rMod.amount;
    }
    switch (*rMod.amountSource)
    {
        case StatModifierEffect_t::AmountSource_t::ElevationEnergy:
            if (!pCtx || !pCtx->targetTile || !pCtx->pTileYieldRules)
            {
                throw std::runtime_error(
                    "AmountSourceValue: ElevationEnergy requires targetTile and pTileYieldRules");
            }
            return AmountSourceValue(*rMod.amountSource, rMod.amount, *pCtx->targetTile,
                                     *pCtx->pTileYieldRules);
        case StatModifierEffect_t::AmountSource_t::MineralsConverted:
            if (!pCtx || !pCtx->pStockpile)
            {
                throw std::runtime_error(
                    "AmountSourceValue: MineralsConverted requires pStockpile");
            }
            return AmountSourceValue(*rMod.amountSource, rMod.amount, *pCtx->pStockpile);
        case StatModifierEffect_t::AmountSource_t::BaseSize:
            if (!pCtx || !pCtx->pBase)
            {
                throw std::runtime_error(
                    "AmountSourceValue: BaseSize requires pBase");
            }
            return AmountSourceValue(*rMod.amountSource, rMod.amount, *pCtx->pBase);
        case StatModifierEffect_t::AmountSource_t::BuildingUpkeep:
            if (!pCtx || !pCtx->pBase)
            {
                throw std::runtime_error(
                    "AmountSourceValue: BuildingUpkeep requires pBase");
            }
            return AmountSourceValue(*rMod.amountSource, rMod.amount, *pCtx->pBase);
        case StatModifierEffect_t::AmountSource_t::BasesOwned:
            if (!pCtx || !pCtx->pFaction)
            {
                throw std::runtime_error(
                    "AmountSourceValue: BasesOwned requires pFaction");
            }
            return AmountSourceValue(*rMod.amountSource, rMod.amount, *pCtx->pFaction);
    }
    throw std::runtime_error("AmountSourceValue: unknown amount_source");
}

EffectContext_t UnitSubjectContext(const Unit* pUnit, const EffectContext_t& rCtx)
{
    EffectContext_t ctx = rCtx;
    if (pUnit)
    {
        if (!ctx.pFaction)
        {
            ctx.pFaction = &pUnit->GetFaction();
        }
        if (!ctx.pUnit)
        {
            ctx.pUnit = pUnit;
        }
    }
    return ctx;
}

namespace
{

// The one config->ActiveEffect_t loop. Every public collect/append helper funnels through
// here so the Instantaneous exclusion (those fire once via DispatchInstantaneousEffects)
// and TagsOriginBase origin tagging can never be forgotten by an individual collector.
template <typename IncludePred>
void AppendActiveEffectsIf_(std::span<const EffectConfig_t> rEffects,
                            const BaseManager* pOriginBase,
                            const std::string& sourceId,
                            IncludePred include,
                            std::vector<ActiveEffect_t>& rOut)
{
    for (const EffectConfig_t& rEffect : rEffects)
    {
        if (rEffect.persistence == EffectPersistence_t::Instantaneous)
            continue;
        if (!include(rEffect))
            continue;
        rOut.emplace_back(rEffect, sourceId, TagsOriginBase(rEffect.scope) ? pOriginBase : nullptr);
    }
}

} // namespace

void AppendActiveEffects(std::span<const EffectConfig_t> rEffects,
                         const BaseManager* pOriginBase,
                         const std::string& sourceId,
                         std::vector<ActiveEffect_t>& rOut)
{
    AppendActiveEffectsIf_(rEffects, pOriginBase, sourceId,
                           [](const EffectConfig_t&) { return true; }, rOut);
}

void AppendFactionLaneEffects(std::span<const EffectConfig_t> rEffects,
                              const BaseManager* pOriginBase,
                              const std::string& sourceId,
                              std::vector<ActiveEffect_t>& rOut)
{
    AppendActiveEffectsIf_(rEffects, pOriginBase, sourceId,
                           [](const EffectConfig_t& rEffect) { return IsFactionLane(rEffect.scope); },
                           rOut);
}

void AppendBaseLaneEffects(std::span<const EffectConfig_t> rEffects,
                           const BaseManager* pOriginBase,
                           const std::string& sourceId,
                           std::vector<ActiveEffect_t>& rOut)
{
    AppendActiveEffectsIf_(rEffects, pOriginBase, sourceId,
                           [](const EffectConfig_t& rEffect)
                           { return LaneFor(rEffect.scope) == EffectLane_t::Base; },
                           rOut);
}

bool TileEffectReaches(const EffectConfig_t& rEffect, int distance)
{
    return LaneFor(rEffect.scope) == EffectLane_t::TileLocal
        && rEffect.persistence != EffectPersistence_t::Instantaneous
        && distance >= rEffect.minRadius
        && distance <= rEffect.radius;
}

void AppendTileEffects(std::span<const EffectConfig_t> rEffects,
                       const std::string& sourceId,
                       int distance,
                       std::vector<ActiveEffect_t>& rOut)
{
    AppendActiveEffectsIf_(rEffects, nullptr, sourceId,
                           [distance](const EffectConfig_t& rEffect)
                           { return TileEffectReaches(rEffect, distance); },
                           rOut);
}

namespace
{

// Key: (originBase*, grantedBuildingId). Pointer identity is intentional — two
// different bases granting the same building must each expand independently.
using ProcessedGrantedIds_t = std::set<std::pair<const BaseManager*, std::string>>;

// True if buildingId appears as a segment of a chained sourceId ("a -> b -> c").
// Used to break grant cycles: a grant whose target is already an ancestor in its own
// chain (the constructed root or an earlier grant) is already contributing its effects,
// so expanding it again would duplicate them.
bool GrantChainContains_(const std::string& rSourceChain, const std::string& rBuildingId)
{
    const std::string_view chain(rSourceChain);
    const std::string_view separator(" -> ");
    size_t pos = 0;
    while (true)
    {
        const size_t next = chain.find(separator, pos);
        const std::string_view segment = chain.substr(pos, (next == std::string_view::npos ? chain.size() : next) - pos);
        if (segment == rBuildingId)
        {
            return true;
        }
        if (next == std::string_view::npos)
        {
            return false;
        }
        pos = next + separator.size();
    }
}

bool IsPerBaseGrantScope_(EffectScope_t scope)
{
    return scope == EffectScope_t::ThisBase
        || scope == EffectScope_t::ProducedAtThisBase;
}

// Mark buildings already constructed on rBases as processed so a later grant of the same
// id does not double-count continuous effects (SMAC Command Nexus / Perimeter Defense).
// Seeds both {pBase, id} (ThisBase half) and {nullptr, id} (FactionGlobal half).
void SeedProcessedGrantedIdsFromConstructed_(
    const std::vector<const BaseManager*>& rBases,
    ProcessedGrantedIds_t& rProcessed)
{
    for (const BaseManager* pBase : rBases)
    {
        if (!pBase)
        {
            continue;
        }
        for (const BuildingConfig_t* pBuilding : pBase->GetBuildingManager().GetBuildings())
        {
            if (!pBuilding)
            {
                throw std::runtime_error(
                    "ExpandGrantBuildingEffects: null constructed building");
            }
            rProcessed.insert({pBase, pBuilding->id});
            rProcessed.insert({nullptr, pBuilding->id});
        }
    }
}

// ThisBase-scoped grant: expand the granted building once for the originating base.
void ExpandLocalGrant_(const BuildingConfig_t& rGranted,
                       const BaseManager& rOriginBase,
                       const std::string& rGrantedBuildingId,
                       const std::string& rSourceId,
                       ProcessedGrantedIds_t& rProcessed,
                       std::vector<ActiveEffect_t>& rEffects)
{
    const std::pair<const BaseManager*, std::string> key = {&rOriginBase, rGrantedBuildingId};
    if (rProcessed.count(key))
    {
        return;
    }
    rProcessed.insert(key);
    AppendActiveEffects(rGranted.effects, &rOriginBase, rSourceId, rEffects);
}

// FactionGlobal / AllOwnerBases grant: non-per-base effects once globally; ThisBase /
// ProducedAtThisBase sub-effects cloned once per base that has not already constructed it.
void ExpandGlobalGrant_(const BuildingConfig_t& rGranted,
                        const std::string& rGrantedBuildingId,
                        const std::string& rSourceId,
                        const std::vector<const BaseManager*>& rBases,
                        ProcessedGrantedIds_t& rProcessed,
                        std::vector<ActiveEffect_t>& rEffects)
{
    const std::pair<const BaseManager*, std::string> globalKey = {nullptr, rGrantedBuildingId};
    if (!rProcessed.count(globalKey))
    {
        rProcessed.insert(globalKey);
        AppendActiveEffectsIf_(rGranted.effects, nullptr, rSourceId,
                               [](const EffectConfig_t& rEffect)
                               { return !IsPerBaseGrantScope_(rEffect.scope); },
                               rEffects);
    }

    for (const BaseManager* pBase : rBases)
    {
        if (!pBase)
        {
            continue;
        }
        const std::pair<const BaseManager*, std::string> key = {pBase, rGrantedBuildingId};
        if (rProcessed.count(key))
        {
            continue;
        }
        rProcessed.insert(key);
        AppendActiveEffectsIf_(rGranted.effects, pBase, rSourceId,
                               [](const EffectConfig_t& rEffect)
                               { return IsPerBaseGrantScope_(rEffect.scope); },
                               rEffects);
    }
}

// Resolve one GrantBuilding entry: look up the target, skip cycles, dispatch local/global.
// Copies fields needed for expansion before appending — rEffects may reallocate.
void ExpandOneGrant_(const ActiveEffect_t& rGrantEffect,
                     const BuildingRegistry& rRegistry,
                     const std::vector<const BaseManager*>& rBases,
                     ProcessedGrantedIds_t& rProcessed,
                     std::vector<ActiveEffect_t>& rEffects)
{
    const GrantBuildingEffect_t* pGrant =
        std::get_if<GrantBuildingEffect_t>(&rGrantEffect.config->effect);
    if (!pGrant)
    {
        return;
    }

    const BuildingConfig_t* pGranted = rRegistry.Find(pGrant->buildingId);
    if (!pGranted)
    {
        throw std::runtime_error("Unknown granted building id '" + pGrant->buildingId + "'");
    }

    if (GrantChainContains_(rGrantEffect.sourceId, pGrant->buildingId))
    {
        return;
    }

    const std::string grantedBuildingId = pGrant->buildingId;
    const std::string sourceId = rGrantEffect.sourceId + " -> " + pGranted->id;
    const BaseManager* pOriginBase = rGrantEffect.originBase;
    if (pOriginBase != nullptr)
    {
        ExpandLocalGrant_(*pGranted, *pOriginBase, grantedBuildingId, sourceId,
                          rProcessed, rEffects);
    }
    else
    {
        ExpandGlobalGrant_(*pGranted, grantedBuildingId, sourceId, rBases, rProcessed, rEffects);
    }
}

} // namespace

std::vector<ActiveEffect_t> ExpandGrantBuildingEffects(
    std::vector<ActiveEffect_t> effects,
    const BuildingRegistry& rRegistry,
    const std::vector<const BaseManager*>& rBases)
{
    ProcessedGrantedIds_t processedGrantedIds;
    SeedProcessedGrantedIdsFromConstructed_(rBases, processedGrantedIds);

    // Index-based: AppendActiveEffects may push new GrantBuilding entries that must expand too.
    for (size_t i = 0; i < effects.size(); ++i)
    {
        ExpandOneGrant_(effects[i], rRegistry, rBases, processedGrantedIds, effects);
    }

    return effects;
}

double ApplyModifierStack(double base, const std::vector<std::pair<double, ModifierOp_t>>& contributions)
{
    double addTotal = base;
    double arithmeticFactor = 1.0;
    double geometricFactor = 1.0;
    // Infinite bounds are the no-clamp identity, so no "was one present" flag is needed.
    double maxClamp = std::numeric_limits<double>::infinity();
    double minClamp = -std::numeric_limits<double>::infinity();
    for (const auto& [amount, op] : contributions)
    {
        switch (op)
        {
            case ModifierOp_t::Add:               addTotal += amount; break;
            case ModifierOp_t::AddPercent:        arithmeticFactor += amount / 100.0; break;
            case ModifierOp_t::MultiplyGeometric: geometricFactor *= amount; break;
            case ModifierOp_t::MaxClamp:          maxClamp = std::min(maxClamp, amount); break;
            case ModifierOp_t::MinClamp:          minClamp = std::max(minClamp, amount); break;
        }
    }
    const double result = addTotal * arithmeticFactor * geometricFactor;
    // MinClamp applied last, so it wins when a MinClamp and a MaxClamp cross.
    return std::max(std::min(result, maxClamp), minClamp);
}

const FactionEffects_t& CollectActiveEffects(const IEffectsProvider& rProvider)
{
    return rProvider.GetActiveEffects();
}

namespace
{

bool ConditionBodySatisfied_(const Condition_t& condition, const EffectContext_t& ctx,
                             const BaseManager* pOriginBase)
{
    return std::visit(
        [&](const auto& rAlt) -> bool
        {
            using T = std::decay_t<decltype(rAlt)>;
            if constexpr (std::is_same_v<T, TargetTileHas_t>)
            {
                return ctx.targetTile != nullptr && ctx.targetTile->HasFeature(rAlt.featureId);
            }
            else if constexpr (std::is_same_v<T, IsDefending_t>)
            {
                return ctx.combatRole == CombatRole_t::Defender;
            }
            else if constexpr (std::is_same_v<T, OriginBaseIsTargetBase_t>)
            {
                return pOriginBase != nullptr && ctx.targetTile != nullptr
                    && &pOriginBase->GetTile() == ctx.targetTile;
            }
            else if constexpr (std::is_same_v<T, OriginBaseIsHomeBase_t>)
            {
                return pOriginBase != nullptr && ctx.pUnit != nullptr
                    && ctx.pUnit->GetHomeBase() == pOriginBase;
            }
            else if constexpr (std::is_same_v<T, AttackerIsEmbarked_t>)
            {
                return ctx.pAttacker != nullptr && ctx.pAttacker->IsEmbarked();
            }
            else if constexpr (std::is_same_v<T, IsHeadquarters_t>)
            {
                return ctx.pBase != nullptr
                    && ResolveFlag(*ctx.pBase, RuleFlagId_t::Headquarters);
            }
            else if constexpr (std::is_same_v<T, AllOf_t>)
            {
                if (rAlt.conditions.empty())
                {
                    return false;
                }
                for (const Condition_t& rNested : rAlt.conditions)
                {
                    if (!ConditionBodySatisfied_(rNested, ctx, pOriginBase))
                    {
                        return false;
                    }
                }
                return true;
            }
            else
            {
                static_assert(k_AlwaysFalse<T>, "Unhandled Condition_t alternative");
            }
        },
        condition.AsVariant());
}

} // namespace

bool ConditionSatisfied(const EffectConfig_t& config, const EffectContext_t& ctx,
                        const BaseManager* pOriginBase)
{
    if (!config.condition)
    {
        return true;
    }
    return ConditionBodySatisfied_(*config.condition, ctx, pOriginBase);
}

bool UnitFilterSatisfied(const EffectConfig_t& config, const Unit& rUnit)
{
    if (!config.unitFilter)
    {
        return true;
    }
    return std::visit(
        [&](const auto& rAlt) -> bool
        {
            using T = std::decay_t<decltype(rAlt)>;
            if constexpr (std::is_same_v<T, UnitFilterDomain_t>)
            {
                return rUnit.GetDomain() == rAlt.domain;
            }
            else if constexpr (std::is_same_v<T, UnitFilterHasComponent_t>)
            {
                return rUnit.GetDesign().HasComponent(rAlt.component);
            }
            else if constexpr (std::is_same_v<T, UnitFilterHasFlag_t>)
            {
                // Design-only: avoid CollectLiveUnitEffects recursion (HasFlag is evaluated
                // while building that list). Native / probe filters key off chassis/specials.
                return ResolveFlag(rUnit.GetDesign(), rAlt.flag);
            }
            else if constexpr (std::is_same_v<T, UnitFilterIsPrototype_t>)
            {
                return rUnit.IsPrototype();
            }
            else if constexpr (std::is_same_v<T, UnitFilterIsCombatUnit_t>)
            {
                return rUnit.GetDesign().IsCombatUnit();
            }
            else
            {
                static_assert(k_AlwaysFalse<T>, "Unhandled UnitFilter_t alternative");
            }
        },
        *config.unitFilter);
}

bool BuildingFilterSatisfied(const EffectConfig_t& config, const BuildingConfig_t& rBuilding)
{
    if (!config.buildingFilter)
    {
        return true;
    }
    return std::visit(
        [&](const auto& rAlt) -> bool
        {
            using T = std::decay_t<decltype(rAlt)>;
            if constexpr (std::is_same_v<T, BuildingFilterAll_t>)
            {
                return true;
            }
            else if constexpr (std::is_same_v<T, BuildingFilterId_t>)
            {
                return rBuilding.id == rAlt.buildingId;
            }
            else if constexpr (std::is_same_v<T, BuildingFilterCategory_t>)
            {
                return rBuilding.category == rAlt.category;
            }
            else
            {
                static_assert(k_AlwaysFalse<T>, "Unhandled BuildingFilter_t alternative");
            }
        },
        *config.buildingFilter);
}

bool FactionFilterMatchesOwner(const EffectConfig_t& rConfig, bool bPlayerControlled)
{
    if (!rConfig.factionFilter
        || rConfig.factionFilter->kind != FactionFilterKind_t::PlayerType)
    {
        return true;
    }
    return (rConfig.factionFilter->playerType == PlayerType_t::Player) == bPlayerControlled;
}

BaseEffects_t FilterForBase(const FactionEffects_t& rFactionEffects, const BaseManager& rBase)
{
    BaseEffects_t matching(rBase);
    for (const ActiveEffect_t& effect : rFactionEffects.effects)
    {
        switch (LaneFor(effect.config->scope))
        {
            case EffectLane_t::Base:
                if (effect.originBase == &rBase)
                {
                    matching.effects.push_back(effect);
                }
                break;
            case EffectLane_t::FactionWide:
                matching.effects.push_back(effect);
                break;
            case EffectLane_t::FactionUnits:
            case EffectLane_t::ProducedAtBase:
            case EffectLane_t::UnitLocal:
            case EffectLane_t::PopLocal:
            case EffectLane_t::TileLocal:
                // Resolved by their own unit/pop/tile; never apply to base-level calculations.
                break;
        }
    }
    return matching;
}

namespace
{

std::vector<ActiveEffect_t> CollectUnitComponentEffects_(
    const std::vector<const UnitComponentConfig_t*>& components)
{
    std::vector<ActiveEffect_t> result;
    for (const UnitComponentConfig_t* pComp : components)
    {
        if (pComp)
        {
            AppendActiveEffects(pComp->effects, nullptr, pComp->id, result);
        }
    }
    return result;
}

} // namespace

std::vector<ActiveEffect_t> CollectPopEffects(const PopTypeConfig_t& rConfig)
{
    std::vector<ActiveEffect_t> result;
    AppendActiveEffects(rConfig.effects, nullptr, rConfig.id, result);
    return result;
}

std::vector<ActiveEffect_t> CollectFromPops(const PopulationManager& rPops, const BaseManager& rOriginBase)
{
    std::vector<ActiveEffect_t> result;
    for (const Pop& rPop : rPops.Pops())
    {
        // Origin is tagged at append time (TagsOriginBase); keep only ThisBase for the base pool.
        std::vector<ActiveEffect_t> popEffects;
        AppendActiveEffects(rPop.GetConfig().effects, &rOriginBase, rPop.GetConfig().id, popEffects);
        auto flatEffectsView = FilterByScope(popEffects, EffectScope_t::ThisBase);
        result.insert(result.end(), flatEffectsView.begin(), flatEffectsView.end());
    }
    return result;
}

std::vector<ActiveEffect_t> CollectTileEffects(const Tile& rTile)
{
    std::vector<ActiveEffect_t> result;

    // Terrain features and improvements are both held as config pointers on the tile.
    // Own-tile collection is the distance-0 case of the shared tile-reach filter.
    for (const ImprovementConfig_t* pFeature : rTile.GetTerrainFeatures())
    {
        if (pFeature)
        {
            AppendTileEffects(pFeature->effects, pFeature->id, 0, result);
        }
    }

    for (const ImprovementConfig_t* pImprovement : rTile.GetImprovements())
    {
        AppendTileEffects(pImprovement->effects, pImprovement->id, 0, result);
    }

    return result;
}

int ApplyModifyPopulation(BaseManager& rBase, const ModifyPopulationEffect_t& rEffect)
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

int PredictModifyPopulationDelta(int size, const ModifyPopulationEffect_t& rEffect)
{
    int delta = 0;
    switch (rEffect.op)
    {
    case ModifierOp_t::Add:
        delta = rEffect.amount;
        break;
    case ModifierOp_t::AddPercent:
        delta = (size * rEffect.amount) / 100;
        break;
    case ModifierOp_t::MultiplyGeometric:
        throw std::runtime_error(
            "PredictModifyPopulationDelta: MultiplyGeometric is not supported");
    case ModifierOp_t::MaxClamp:
    case ModifierOp_t::MinClamp:
        throw std::runtime_error(
            "PredictModifyPopulationDelta: clamp ops are not supported");
    }

    if (delta < 0)
    {
        const int toRemove = -delta;
        const int canRemove = std::max(0, size - rEffect.minSize);
        return -std::min(toRemove, canRemove);
    }
    return delta;
}

int PredictInstantaneousPopulationSize(std::span<const EffectConfig_t> rEffects, int size)
{
    int current = size;
    for (const EffectConfig_t& rEffect : rEffects)
    {
        if (rEffect.persistence != EffectPersistence_t::Instantaneous)
        {
            continue;
        }
        const ModifyPopulationEffect_t* pModify =
            std::get_if<ModifyPopulationEffect_t>(&rEffect.effect);
        if (!pModify)
        {
            continue;
        }
        current += PredictModifyPopulationDelta(current, *pModify);
    }
    return current;
}

int PredictUnitProductionPopulationSize(const UnitDesign& rDesign, int size)
{
    int current = size;
    for (const UnitComponentConfig_t* pComp : rDesign.GetComponents())
    {
        if (!pComp)
        {
            continue;
        }
        current = PredictInstantaneousPopulationSize(pComp->effects, current);
    }
    return current;
}

void DispatchInstantaneousEffects(std::span<const EffectConfig_t> rEffects, BaseManager& rBase,
                                  GameState& rGameState)
{
    for (const EffectConfig_t& effect : rEffects)
    {
        if (effect.persistence != EffectPersistence_t::Instantaneous)
            continue;

        if (const GrantBuildingEffect_t* pGrant = std::get_if<GrantBuildingEffect_t>(&effect.effect))
        {
            // A grant whose target the base already holds is an ordinary outcome, not an error.
            if (rBase.GetBuildingManager().CanAddBuilding(pGrant->buildingId))
            {
                rBase.GetBuildingManager().AddBuilding(pGrant->buildingId);
            }
        }
        else if (const GrantTechEffect_t* pGrant = std::get_if<GrantTechEffect_t>(&effect.effect))
        {
            rBase.GetFaction().GetResearch().AddDiscoveredTech(pGrant->techId);
        }
        else if (std::get_if<GrantUnitEffect_t>(&effect.effect))
        {
            std::cerr << "[TODO] Instantaneous GrantUnit not yet implemented\n";
        }
        else if (std::get_if<InfiltrationEffect_t>(&effect.effect))
        {
            ApplyInfiltrationEffect(rGameState, rBase.GetFaction(), effect);
        }
        else if (const ModifyPopulationEffect_t* pModify =
                     std::get_if<ModifyPopulationEffect_t>(&effect.effect))
        {
            ApplyModifyPopulation(rBase, *pModify);
        }
        else if (const DestroyFacilityEffect_t* pDestroy =
                     std::get_if<DestroyFacilityEffect_t>(&effect.effect))
        {
            DestroyRandomFacilities(rGameState, rBase, pDestroy->count, pDestroy->excludeHq,
                                    pDestroy->excludeSecretProjects, rGameState.GetRng());
        }
        else if (std::get_if<RebelEffect_t>(&effect.effect))
        {
            const GameDataContext& rData = rBase.GetFaction().GetDataContext();
            if (!rData.popCompositionConfig)
            {
                throw std::runtime_error(
                    "DispatchInstantaneousEffects: Rebel requires a popCompositionConfig");
            }
            PickRebelFactionAndTransfer(rBase, rGameState,
                                        rData.popCompositionConfig->rebelSelection,
                                        rGameState.GetRng());
        }
    }
}

void DispatchInstantaneousEffects(const BuildingConfig_t& rBuilding, BaseManager& rBase,
                                  GameState& rGameState)
{
    DispatchInstantaneousEffects(std::span<const EffectConfig_t>{rBuilding.effects}, rBase,
                                 rGameState);
}

void DispatchInstantaneousEffects(const UnitDesign& rDesign, BaseManager& rBase,
                                  GameState& rGameState)
{
    for (const UnitComponentConfig_t* pComp : rDesign.GetComponents())
    {
        if (pComp)
        {
            DispatchInstantaneousEffects(std::span<const EffectConfig_t>{pComp->effects}, rBase,
                                         rGameState);
        }
    }
}

// A live unit's full effect list: design components, all FactionUnits, and ProducedAtThisBase
// effects whose origin matches the unit's production base. unitFilter and ProducedAt origin
// are applied here — see CollectLiveUnitEffects declaration.
UnitEffects_t CollectLiveUnitEffects(const Unit& rUnit)
{
    std::vector<ActiveEffect_t> effects = rUnit.GetDesign().CollectEffects();
    const auto& rPool = rUnit.GetFaction().GetActiveEffects().effects;
    auto factionEffects = FilterByScope(rPool, EffectScope_t::FactionUnits);
    effects.insert(effects.end(), factionEffects.begin(), factionEffects.end());
    auto producedEffects = FilterByScope(rPool, EffectScope_t::ProducedAtThisBase);
    effects.insert(effects.end(), producedEffects.begin(), producedEffects.end());

    const BaseManager* pProducedAt = rUnit.GetProducedAtBase();
    std::erase_if(effects, [&](const ActiveEffect_t& rEffect)
    {
        if (!UnitFilterSatisfied(*rEffect.config, rUnit))
        {
            return true;
        }
        if (rEffect.config->scope == EffectScope_t::ProducedAtThisBase
            && rEffect.originBase != pProducedAt)
        {
            return true;
        }
        return false;
    });
    return UnitEffects_t(rUnit, std::move(effects));
}

UnitEffects_t CollectUnitEffects(const UnitDesign& rDesign)
{
    return UnitEffects_t(rDesign, CollectUnitComponentEffects_(rDesign.GetComponents()));
}

double ResolveBaseStat(const BaseEffects_t& rBaseEffects, StatId_t statId, double seed,
                       const EffectContext_t* pCtx)
{
    if (DomainFor(statId) != ResolveDomain_t::Base)
    {
        throw std::logic_error("ResolveBaseStat: DomainFor(stat) is not Base");
    }
    EffectContext_t ctx = pCtx ? *pCtx : EffectContext_t{};
    if (!ctx.pBase)
    {
        ctx.pBase = rBaseEffects.pBase;
    }
    return ResolveStatModifiers(FilterBaseLevelByStatId(rBaseEffects, statId, &ctx), seed, &ctx)
        .total;
}

double ResolveFactionStat(const FactionEffects_t& rFactionEffects, StatId_t statId, double seed,
                          const EffectContext_t* pCtx)
{
    if (DomainFor(statId) != ResolveDomain_t::Faction)
    {
        throw std::logic_error("ResolveFactionStat: DomainFor(stat) is not Faction");
    }
    // Same stamping ResolveBaseStat / ResolveUnitStat do: the bundle's subject is what faction
    // amount sources resolve against, so a caller never supplies it twice. Conditions still
    // need a caller context — without one this stays a context-free resolve and
    // condition-carrying effects are skipped, exactly as FilterByStatId does.
    EffectContext_t ctx = pCtx ? *pCtx : EffectContext_t{};
    if (!ctx.pFaction)
    {
        ctx.pFaction = rFactionEffects.pFaction;
    }
    const bool bConditionsAllowed = pCtx != nullptr;
    auto matching = rFactionEffects.effects
                    | std::views::filter([statId, &ctx, bConditionsAllowed](const ActiveEffect_t& rEffect)
    {
        if (!bConditionsAllowed && rEffect.config->condition)
        {
            return false;
        }
        return StatModifierMatchesInContext(rEffect, statId, ctx);
    });
    return ResolveStatModifiers(matching, seed, &ctx).total;
}

double ResolveUnitStat(const UnitEffects_t& rUnitEffects, StatId_t statId, double seed,
                       const EffectContext_t* pCtx)
{
    if (DomainFor(statId) != ResolveDomain_t::Unit)
    {
        throw std::logic_error("ResolveUnitStat: DomainFor(stat) is not Unit");
    }
    const EffectContext_t ctx =
        UnitSubjectContext(rUnitEffects.pUnit, pCtx ? *pCtx : EffectContext_t{});
    return ResolveStatModifiers(
               FilterByStatIdInContext(rUnitEffects.effects, statId, ctx), seed, &ctx)
        .total;
}

namespace
{

// The one multiplicative-only unit resolve: AddPercent / MultiplyGeometric contributions
// seeded at baseValue, Adds deliberately ignored. Shared by psi combat and the SE morale
// PositiveMoraleScale step so the two cannot drift.
double ResolveUnitMultiplicative_(const UnitEffects_t& rUnitEffects, StatId_t statId,
                                  double baseValue, const EffectContext_t& rCtx)
{
    const EffectContext_t ctx = UnitSubjectContext(rUnitEffects.pUnit, rCtx);
    std::vector<std::pair<double, ModifierOp_t>> contributions;
    for (const ActiveEffect_t& rEffect :
         FilterByStatIdInContext(rUnitEffects.effects, statId, ctx))
    {
        const StatModifierEffect_t* pModifier =
            std::get_if<StatModifierEffect_t>(&rEffect.config->effect);
        if (pModifier && pModifier->op != ModifierOp_t::Add)
        {
            contributions.emplace_back(AmountSourceValue(*pModifier, &ctx), pModifier->op);
        }
    }
    return ApplyModifierStack(baseValue, contributions);
}

// A live unit's effects plus the morale level's own effects, as one Unit-domain bundle.
UnitEffects_t LiveUnitEffectsWithMorale_(const Unit& rUnit,
                                         std::span<const EffectConfig_t> moraleLevelEffects)
{
    UnitEffects_t unitEffects = CollectLiveUnitEffects(rUnit);
    AppendActiveEffects(moraleLevelEffects, nullptr, "morale_level", unitEffects.effects);
    return unitEffects;
}

} // namespace

int ResolveCombatUnitStat(const Unit& rUnit, StatId_t statId, const EffectContext_t& rCtx,
                          std::span<const EffectConfig_t> moraleLevelEffects)
{
    return FinalizeResolvedStat(
        ResolveUnitStat(LiveUnitEffectsWithMorale_(rUnit, moraleLevelEffects), statId,
                        SeedFor(statId), &rCtx));
}

double ResolveCombatUnitMultiplicativeStat(const Unit& rUnit, StatId_t statId, double baseValue,
                                           const EffectContext_t& rCtx,
                                           std::span<const EffectConfig_t> moraleLevelEffects)
{
    return ResolveUnitMultiplicative_(LiveUnitEffectsWithMorale_(rUnit, moraleLevelEffects),
                                      statId, baseValue, rCtx);
}

namespace
{

template <std::ranges::input_range Range>
bool ResolveFlagFromEffects_(Range&& effects, RuleFlagId_t flagId)
{
    for (const ActiveEffect_t& rEffect : effects)
    {
        if (rEffect.config->condition.has_value())
        {
            continue;
        }
        const RuleFlagEffect_t* pFlag = std::get_if<RuleFlagEffect_t>(&rEffect.config->effect);
        if (pFlag && pFlag->flag == flagId)
        {
            return true;
        }
    }
    return false;
}

} // namespace

int ResolveStat(const UnitDesign& rDesign, StatId_t statId)
{
    // Materialize first: FilterByStatId rejects rvalues (borrowing view).
    const std::vector<ActiveEffect_t> effects = rDesign.CollectEffects();
    return FinalizeResolvedStat(
        ResolveStatModifiers(FilterByStatId(effects, statId), SeedFor(statId)).total);
}

int ResolveStat(const UnitDesign& rDesign, StatId_t statId, const EffectContext_t& rCtx)
{
    const std::vector<ActiveEffect_t> effects = rDesign.CollectEffects();
    return FinalizeResolvedStat(
        ResolveStatModifiers(FilterByStatIdInContext(effects, statId, rCtx),
                             SeedFor(statId), &rCtx).total);
}

int ResolveAdditiveStat(const UnitDesign& rDesign, StatId_t statId)
{
    // Materialize first: FilterByStatId returns a borrowing view (see its contract).
    const std::vector<ActiveEffect_t> effects = rDesign.CollectEffects();
    double addTotal = SeedFor(statId);
    for (const ActiveEffect_t& rEffect : FilterByStatId(effects, statId))
    {
        const StatModifierEffect_t* pStatModifier =
            std::get_if<StatModifierEffect_t>(&rEffect.config->effect);
        if (pStatModifier && pStatModifier->op == ModifierOp_t::Add)
        {
            addTotal += pStatModifier->amount;
        }
    }
    return FinalizeResolvedStat(addTotal);
}

bool ResolveFlag(const UnitDesign& rDesign, RuleFlagId_t flagId)
{
    return ResolveFlagFromEffects_(rDesign.CollectEffects(), flagId);
}

int ResolveStat(const Unit& rUnit, StatId_t statId)
{
    return FinalizeResolvedStat(
        ResolveUnitStat(CollectLiveUnitEffects(rUnit), statId, SeedFor(statId)));
}

int ResolveStat(const Unit& rUnit, StatId_t statId, const EffectContext_t& rCtx)
{
    return FinalizeResolvedStat(
        ResolveUnitStat(CollectLiveUnitEffects(rUnit), statId, SeedFor(statId), &rCtx));
}

double ResolveMultiplicativeStat(const Unit& rUnit, StatId_t statId, double baseValue,
                                 const EffectContext_t& rCtx)
{
    return ResolveUnitMultiplicative_(CollectLiveUnitEffects(rUnit), statId, baseValue, rCtx);
}

bool ResolveFlag(const Unit& rUnit, RuleFlagId_t flagId)
{
    return ResolveFlagFromEffects_(CollectLiveUnitEffects(rUnit).effects, flagId);
}

bool ResolveFlag(const Faction& rFaction, RuleFlagId_t flagId)
{
    return ResolveFlagFromEffects_(CollectActiveEffects(rFaction).effects, flagId);
}

bool ResolveFlag(const BaseManager& rBase, RuleFlagId_t flagId)
{
    return ResolveFlagFromEffects_(rBase.GetBaseEffects().effects, flagId);
}

bool HasPermission(const Unit& rUnit, PermissionId_t permission, const EffectContext_t& rCtx)
{
    for (const ActiveEffect_t& rEffect : CollectLiveUnitEffects(rUnit).effects)
    {
        const PermissionEffect_t* pPerm =
            std::get_if<PermissionEffect_t>(&rEffect.config->effect);
        if (!pPerm || pPerm->permission != permission)
        {
            continue;
        }
        if (!ConditionSatisfied(*rEffect.config, rCtx, rEffect.originBase))
        {
            continue;
        }
        return true;
    }
    return false;
}

namespace
{

// Shared by both tile-flag overloads: does rEffect declare flagId as an unconditional
// ThisTile flag on the host tile itself (radius 0, no condition)? Radius auras are excluded
// deliberately — a tile capability describes its host, not the host's neighbourhood.
bool DeclaresTileFlag_(const EffectConfig_t& rEffect, RuleFlagId_t flagId)
{
    const RuleFlagEffect_t* pFlag = std::get_if<RuleFlagEffect_t>(&rEffect.effect);
    return pFlag && pFlag->flag == flagId && rEffect.scope == EffectScope_t::ThisTile
        && rEffect.radius == 0 && !rEffect.condition.has_value();
}

bool AnyDeclaresTileFlag_(const std::vector<EffectConfig_t>& rEffects, RuleFlagId_t flagId)
{
    for (const EffectConfig_t& rEffect : rEffects)
    {
        if (DeclaresTileFlag_(rEffect, flagId))
        {
            return true;
        }
    }
    return false;
}

} // namespace

bool ResolveFlag(const Tile& rTile, RuleFlagId_t flagId)
{
    for (const ImprovementConfig_t* pConfig : rTile.GetTerrainFeatures())
    {
        if (pConfig && AnyDeclaresTileFlag_(pConfig->effects, flagId))
        {
            return true;
        }
    }
    for (const ImprovementConfig_t* pConfig : rTile.GetImprovements())
    {
        if (pConfig && AnyDeclaresTileFlag_(pConfig->effects, flagId))
        {
            return true;
        }
    }
    return false;
}

bool TileProvidesFlag(const Tile& rTile, RuleFlagId_t flagId, const WorldMap& rWorldMap,
                      FactionId_t factionId)
{
    if (ResolveFlag(rTile, flagId))
    {
        return true;
    }
    for (const Unit* pUnit : rWorldMap.GetUnitsOnTile(rTile))
    {
        // An embarked unit is cargo, not a site: its deck is unavailable while stowed.
        if (!pUnit || pUnit->IsEmbarked()
            || pUnit->GetFaction().GetFactionId() != factionId)
        {
            continue;
        }
        // Live effects, not design-only: a tile capability must honour unitFilter and pick
        // up FactionUnits-scoped grants, exactly as the transport rules that consume it do.
        for (const ActiveEffect_t& rEffect : CollectLiveUnitEffects(*pUnit).effects)
        {
            if (DeclaresTileFlag_(*rEffect.config, flagId))
            {
                return true;
            }
        }
    }
    return false;
}

} // namespace ac
