#include "game/effects/TileEffectsContext.h"

#include "game/faction/Faction.h"
#include "game/effects/IWorldEffectsSource.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/OccupantCoexistence.h"
#include "game/map/ImprovementIds.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/MapUtils.h"
#include "game/map/RiverGeneration.h"
#include "game/map/TerritoryMap.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitDesign.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/TileYieldRulesConfig.h"
#include "game/effects/InteractionGridsConfig.h"
#include <algorithm>
#include <cmath>
#include <span>
#include <type_traits>
#include <unordered_set>
#include <variant>

namespace ac
{

namespace
{

bool IsClampOp_(ModifierOp_t op)
{
    return op == ModifierOp_t::MaxClamp || op == ModifierOp_t::MinClamp;
}

std::vector<const ActiveEffect_t*> WithoutClamps_(const std::vector<const ActiveEffect_t*>& rEffects)
{
    std::vector<const ActiveEffect_t*> kept;
    for (const ActiveEffect_t* pEffect : rEffects)
    {
        const auto* pMod = std::get_if<StatModifierEffect_t>(&pEffect->config->effect);
        if (!pMod || !IsClampOp_(pMod->op))
        {
            kept.push_back(pEffect);
        }
    }
    return kept;
}

// How far an improvement's effects can reach: the max per-effect radius.
int MaxEffectReach_(const ImprovementConfig_t& rConfig)
{
    int reach = 0;
    for (const EffectConfig_t& rEffect : rConfig.effects)
    {
        reach = std::max(reach, rEffect.radius);
    }
    return reach;
}

void AppendOwnedImprovementEffects_(const Tile& rHostTile, const ImprovementConfig_t& rConfig,
                                    int distance, const WorldMap& rWorldMap,
                                    std::vector<ActiveEffect_t>& rOut)
{
    const size_t before = rOut.size();
    AppendTileEffects(rConfig.effects, rConfig.id, distance, rOut);
    if (!rConfig.ownedByTerritory)
    {
        return;
    }
    const FactionId_t owner = rWorldMap.GetTerritory().GetOwner(rHostTile);
    for (size_t i = before; i < rOut.size(); ++i)
    {
        rOut[i].ownerFaction = owner;
    }
}

void AppendOccupantEffects_(const Tile& rTile, int distance, const WorldMap& rWorldMap,
                            std::vector<ActiveEffect_t>& rOut)
{
    for (const ImprovementConfig_t* pFeature : rTile.GetTerrainFeatures())
    {
        if (pFeature)
        {
            AppendTileEffects(pFeature->effects, pFeature->id, distance, rOut);
        }
    }
    for (const ImprovementConfig_t* pImprovement : rTile.GetImprovements())
    {
        AppendOwnedImprovementEffects_(rTile, *pImprovement, distance, rWorldMap, rOut);
    }
}

void AppendUnitAuraEffects_(const Tile& rTile, int distance, const WorldMap& rWorldMap,
                            std::vector<ActiveEffect_t>& rOut)
{
    for (const Unit* pUnit : rWorldMap.GetUnitsOnTile(rTile))
    {
        if (!pUnit)
        {
            continue;
        }
        const FactionId_t owner = pUnit->GetFaction().GetFactionId();
        for (ActiveEffect_t& rActive : pUnit->GetDesign().CollectEffects())
        {
            if (TileEffectReaches(*rActive.config, distance))
            {
                rActive.ownerFaction = owner;
                rOut.push_back(std::move(rActive));
            }
        }
    }
}

// Single Chebyshev walk: neighbor improvement/terrain auras (distance > 0) plus unit auras
// (including units on the origin tile at distance 0).
void AppendNeighborAndUnitAreaEffects_(const Tile& rOrigin, const WorldMap& rWorldMap,
                                       int maxRadius, std::vector<ActiveEffect_t>& rOut)
{
    ForEachTileInChebyshevRadius(rOrigin, rWorldMap, maxRadius,
        [&](const Tile* pNearby, int distance)
        {
            if (distance > 0)
            {
                AppendOccupantEffects_(*pNearby, distance, rWorldMap, rOut);
            }
            AppendUnitAuraEffects_(*pNearby, distance, rWorldMap, rOut);
        });
}

bool TileMatchesSelector_(const TileSelector_t& selector, const Tile& rTile, bool bIsBaseTile)
{
    return std::visit(
        [&](const auto& rAlt) -> bool
        {
            using T = std::decay_t<decltype(rAlt)>;
            if constexpr (std::is_same_v<T, TileSelectorBaseTile_t>)
            {
                return bIsBaseTile;
            }
            else if constexpr (std::is_same_v<T, TileSelectorHasImprovement_t>)
            {
                // HasFeature so terrain ids (Fungus, Rocky, …) match the same way as Farm/Mine.
                return rTile.HasFeature(rAlt.improvement);
            }
            else if constexpr (std::is_same_v<T, TileSelectorAnyTile_t>)
            {
                (void)rTile;
                (void)bIsBaseTile;
                return true;
            }
            else
            {
                static_assert(k_AlwaysFalse<T>, "Unhandled TileSelector_t alternative");
            }
        },
        selector);
}

// Appends every base-wide StatModifier that carries a tile selector matching rTile.
// Non-selector StatModifiers (flat base bonuses) are left for base-level resolution.
void AppendMatchingTileModifiers_(const std::vector<ActiveEffect_t>& baseEffects,
                                  const Tile& rTile, bool bIsBaseTile,
                                  std::vector<ActiveEffect_t>& rOut)
{
    for (const ActiveEffect_t& effect : baseEffects)
    {
        const StatModifierEffect_t* pModifier = std::get_if<StatModifierEffect_t>(&effect.config->effect);
        if (pModifier && pModifier->selector
            && TileMatchesSelector_(*pModifier->selector, rTile, bIsBaseTile))
        {
            rOut.push_back(effect);
        }
    }
}

void RecomputeMoistureInRadius_(const Tile& rChangedTile, int radius, TileEffectsContext& rCtx,
                                 WorldMap& rWorldMap)
{
    ForEachTileInChebyshevRadius(rChangedTile, rWorldMap, radius,
        [&](Tile* pAffected, int /*distance*/)
        {
            rCtx.RecomputeMoisture(*pAffected);
        });
}

std::unordered_set<std::string> SuppressedYieldSources_(const Tile& rTile)
{
    std::unordered_set<std::string> suppress;
    for (const auto* pConfigs : {&rTile.GetImprovements(), &rTile.GetTerrainFeatures()})
    {
        for (const ImprovementConfig_t* pConfig : *pConfigs)
        {
            if (!pConfig)
            {
                continue;
            }
            suppress.insert(pConfig->suppressYieldSources.begin(),
                            pConfig->suppressYieldSources.end());
        }
    }
    return suppress;
}

// Scan bound: only ThisTile-scoped radii (same rule for improvements and unit components).
// Nonzero radius on any other scope is rejected at parse time.
int MaxThisTileRadius_(const std::vector<EffectConfig_t>& rEffects)
{
    int radius = 0;
    for (const EffectConfig_t& rEffect : rEffects)
    {
        if (rEffect.scope == EffectScope_t::ThisTile)
        {
            radius = std::max(radius, rEffect.radius);
        }
    }
    return radius;
}

int MaxAuraRadius_(const ImprovementRegistry& rImprovements,
                   const UnitComponentRegistry* pUnitComponents)
{
    int radius = 0;
    for (const ImprovementConfig_t& rConfig : rImprovements.GetAll())
    {
        radius = std::max(radius, MaxThisTileRadius_(rConfig.effects));
    }
    if (pUnitComponents)
    {
        for (const UnitComponentConfig_t& rComponent : pUnitComponents->GetAll())
        {
            radius = std::max(radius, MaxThisTileRadius_(rComponent.effects));
        }
    }
    return radius;
}

TileResources_t SumResources_(const TileResources_t& a, const TileResources_t& b)
{
    return TileResources_t{a.nutrients + b.nutrients, a.energy + b.energy,
                           a.minerals + b.minerals};
}

// Pointer-partition into pre-clamp vs bypass_clamp lanes, skipping suppressed sources.
// Does not clone ActiveEffect_t — callers resolve through the pointer lists.
void PartitionYieldEffects_(const Tile& rTile, const std::vector<ActiveEffect_t>& effects,
                            std::vector<const ActiveEffect_t*>& rBeforeClamp,
                            std::vector<const ActiveEffect_t*>& rBypassClamp)
{
    const std::unordered_set<std::string> suppress = SuppressedYieldSources_(rTile);
    rBeforeClamp.reserve(effects.size());
    rBypassClamp.reserve(effects.size());
    for (const ActiveEffect_t& rEffect : effects)
    {
        if (suppress.count(rEffect.sourceId) > 0)
        {
            continue;
        }
        const auto* pMod =
            rEffect.config ? std::get_if<StatModifierEffect_t>(&rEffect.config->effect) : nullptr;
        if (pMod && pMod->bypassClamp)
        {
            rBypassClamp.push_back(&rEffect);
        }
        else
        {
            rBeforeClamp.push_back(&rEffect);
        }
    }
}

} // namespace

TileEffectsContext::TileEffectsContext(WorldMap& rWorldMap, const ImprovementRegistry& rImprovements,
                                       const UnitComponentRegistry* pUnitComponents,
                                       const TileYieldRulesConfig_t& rYieldRules,
                                       const InteractionGridsConfig_t& rInteractionGrids)
    : m_rWorldMap(rWorldMap)
    , m_rImprovements(rImprovements)
    , m_rYieldRules(rYieldRules)
    , m_rInteractionGrids(rInteractionGrids)
    , m_maxRadius(MaxAuraRadius_(rImprovements, pUnitComponents))
{
    for (const std::unique_ptr<Tile>& pTile : rWorldMap.GetTiles())
    {
        pTile->BindOccupants(rImprovements);
        pTile->BindTileChangeListener(this);
    }

}

TileEffectsContext::~TileEffectsContext()
{
    for (const std::unique_ptr<Tile>& pTile : m_rWorldMap.GetTiles())
    {
        if (pTile)
        {
            pTile->UnbindTileChangeListener(*this);
        }
    }
}

WorldMap& TileEffectsContext::GetWorldMap()
{
    return m_rWorldMap;
}

const WorldMap& TileEffectsContext::GetWorldMap() const
{
    return m_rWorldMap;
}

const ImprovementRegistry& TileEffectsContext::GetImprovements() const
{
    return m_rImprovements;
}

void TileEffectsContext::BindWorldEffects(IWorldEffectsSource& rWorldEffects)
{
    m_pWorldEffects = &rWorldEffects;
}

void TileEffectsContext::UnbindWorldEffects()
{
    m_pWorldEffects = nullptr;
}

std::vector<ActiveEffect_t> TileEffectsContext::CollectAreaEffects(const Tile& rTile) const
{
    std::vector<ActiveEffect_t> effects;
    AppendOccupantEffects_(rTile, 0, m_rWorldMap, effects);
    AppendNeighborAndUnitAreaEffects_(rTile, m_rWorldMap, m_maxRadius, effects);
    if (m_pWorldEffects)
    {
        AppendWorldGlobalEffects(m_pWorldEffects->CollectSessionWorldEffects(),
                                 ResolveDomain_t::Tile, effects);
    }
    return effects;
}

TileYieldView_t TileEffectsContext::ResolveTileYield(const Tile& rTile) const
{
    return ResolveYieldFromEffects_(rTile, CollectAreaEffects(rTile));
}

TileYieldView_t TileEffectsContext::ResolveTileYield(const Tile& rTile, bool bIsBaseTile,
                                                     const BaseEffects_t& rBaseEffects) const
{
    std::vector<ActiveEffect_t> effects = CollectAreaEffects(rTile);
    AppendMatchingTileModifiers_(rBaseEffects.effects, rTile, bIsBaseTile, effects);
    return ResolveYieldFromEffects_(rTile, effects);
}

int TileEffectsContext::ResolveResource_(const Tile& rTile,
                                         std::span<const ActiveEffect_t*> effects,
                                         StatId_t stat) const
{
    // The receiving tile: only radius-0 effects use ElevationEnergy, and min_radius keeps the
    // Mirror's ring flat.
    const EffectContext_t ctx{.targetTile = &rTile, .pTileYieldRules = &m_rYieldRules};
    auto matching = effects
        | std::views::transform([](const ActiveEffect_t* pEffect) -> const ActiveEffect_t& {
              return *pEffect;
          })
        | std::views::filter([&](const ActiveEffect_t& effect) {
              return StatModifierMatchesInContext(effect, stat, ctx);
          });
    const double total = ResolveStatModifiersTotal(matching, SeedFor(stat), &ctx);
    return FinalizeResolvedStat(total);
}

TileEffectsContext::YieldLanes_t TileEffectsContext::ResolveYieldLanes_(
    const Tile& rTile, const std::vector<ActiveEffect_t>& effects) const
{
    std::vector<const ActiveEffect_t*> beforeClamp;
    std::vector<const ActiveEffect_t*> bypassClamp;
    PartitionYieldEffects_(rTile, effects, beforeClamp, bypassClamp);

    const TileResources_t preClamped = ResolveResources_(rTile, beforeClamp);
    // UI potential yield: the same effects with the clamps left out.
    std::vector<const ActiveEffect_t*> unclamped = WithoutClamps_(beforeClamp);
    const TileResources_t preUnclamped = ResolveResources_(rTile, unclamped);
    const TileResources_t bypass = ResolveResources_(rTile, bypassClamp);
    return YieldLanes_t{SumResources_(preClamped, bypass), SumResources_(preUnclamped, bypass)};
}

// Every resource seeds at 0; elevation energy comes from ElevationEnergy effects.
TileResources_t TileEffectsContext::ResolveResources_(
    const Tile& rTile, std::span<const ActiveEffect_t*> effects) const
{
    return TileResources_t{
        ResolveResource_(rTile, effects, StatId_t::Nutrients),
        ResolveResource_(rTile, effects, StatId_t::Energy),
        ResolveResource_(rTile, effects, StatId_t::Minerals),
    };
}

TileYieldView_t TileEffectsContext::ResolveYieldFromEffects_(
    const Tile& rTile, const std::vector<ActiveEffect_t>& effects) const
{
    const YieldLanes_t lanes = ResolveYieldLanes_(rTile, effects);
    return TileYieldView_t{lanes.effective, lanes.potential};
}

double TileEffectsContext::ResolveTileDefenseMultiplier(const Tile& rTile, FactionId_t forFaction) const
{
    const std::vector<ActiveEffect_t> effects = CollectAreaEffects(rTile);
    std::vector<ActiveEffect_t> applicable;
    applicable.reserve(effects.size());
    for (const ActiveEffect_t& rEffect : effects)
    {
        if (AppliesForFaction(rEffect, forFaction))
        {
            applicable.push_back(rEffect);
        }
    }
    return ResolveStatModifiers(FilterByStatId(applicable, StatId_t::TileDefense),
                                SeedFor(StatId_t::TileDefense), nullptr)
        .total;
}

void TileEffectsContext::RecomputeMoisture(Tile& rTile)
{
    const std::vector<ActiveEffect_t> effects = CollectAreaEffects(rTile);
    const double baseTier = static_cast<double>(rTile.GetBaseMoisture());
    const double tier = ResolveStatModifiers(
        FilterByStatId(effects, StatId_t::MoistureTier), baseTier, nullptr).total;

    const int rounded = static_cast<int>(std::lround(tier));
    const int clamped = std::clamp(rounded, static_cast<int>(Moisture_t::Arid),
                                   static_cast<int>(Moisture_t::Wet));
    rTile.SetMoisture(static_cast<Moisture_t>(clamped));
}

void TileEffectsContext::AddOccupantWithEffects(Tile& rTile, const std::string& occupantId)
{
    const ImprovementConfig_t& rConfig = m_rImprovements.Get(occupantId);
    rTile.AddOccupant(rConfig);
    RecomputeMoistureInRadius_(rTile, MaxEffectReach_(rConfig), *this, m_rWorldMap);
    if (rConfig.terminatesRiver)
    {
        RecomputeRivers(m_rWorldMap);
    }
}

void TileEffectsContext::OnTileChanged(Tile& rTile, std::string_view keepId)
{
    if (m_bSweepingOccupancy)
    {
        m_pendingOccupancy.push_back(&rTile);
        return;
    }

    m_bSweepingOccupancy = true;
    // First clears what the new occupant displaces, then re-judges it. It survives a stock
    // exclude only on a waiver the tile recorded.
    RemoveImprovementsThatCannotRemain_(rTile, keepId);
    if (!keepId.empty())
    {
        RemoveImprovementsThatCannotRemain_(rTile, {});
    }
    m_bSweepingOccupancy = false;

    while (!m_pendingOccupancy.empty())
    {
        Tile* pTile = m_pendingOccupancy.back();
        m_pendingOccupancy.pop_back();
        if (pTile)
        {
            OnTileChanged(*pTile, {});
        }
    }
}

void TileEffectsContext::RemoveImprovementsThatCannotRemain_(Tile& rTile, std::string_view keepId)
{
    for (;;)
    {
        std::vector<std::string> removeIds;
        for (const ImprovementConfig_t* pConfig : rTile.GetImprovements())
        {
            if (!pConfig || pConfig->id == keepId || pConfig->id == ImprovementIds::k_Base)
            {
                continue;
            }
            // The copy already on the tile is not a reason to remove itself
            // (Monolith excludes Monolith so a second one cannot be placed).
            if (!CanBuildImprovement(rTile, *pConfig, std::span(&pConfig->id, 1), {}))
            {
                removeIds.push_back(pConfig->id);
            }
        }
        if (removeIds.empty())
        {
            break;
        }
        for (const std::string& rId : removeIds)
        {
            RemoveOccupantWithEffects(rTile, rId);
        }
    }
}

void TileEffectsContext::RemoveOccupantWithEffects(Tile& rTile, const std::string& occupantId)
{
    const ImprovementConfig_t* pConfig = m_rImprovements.Find(occupantId);
    if (!pConfig)
    {
        return;
    }
    const int radius = MaxEffectReach_(*pConfig);
    const bool bTerminatesRiver = pConfig->terminatesRiver;

    rTile.RemoveOccupant(*pConfig);
    RecomputeMoistureInRadius_(rTile, radius, *this, m_rWorldMap);
    if (bTerminatesRiver)
    {
        RecomputeRivers(m_rWorldMap);
    }
}

} // namespace ac
