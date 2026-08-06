#include "game/effects/TileEffectsContext.h"

#include "game/Faction.h"
#include "game/map/ImprovementConfigParser.h"
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
#include "game/effects/BonusEffect.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <unordered_set>

namespace ac
{

namespace
{

std::optional<int> CapForStat_(StatId_t stat, const std::vector<ActiveEffect_t>& rEffects)
{
    std::optional<int> cap;
    for (const ActiveEffect_t& rEffect : rEffects)
    {
        const auto* pCap = std::get_if<TileResourceCapEffect_t>(&rEffect.config->effect);
        if (!pCap || pCap->stat != stat)
        {
            continue;
        }
        // Multiple caps: keep the tightest (lowest max).
        cap = cap.has_value() ? std::min(*cap, pCap->max) : pCap->max;
    }
    return cap;
}

int ApplyCap_(int value, std::optional<int> cap)
{
    return cap.has_value() ? std::min(value, *cap) : value;
}

TileResources_t ApplyTileResourceRestrictions_(TileResources_t yield,
                                               const std::vector<ActiveEffect_t>& rEffects)
{
    yield.nutrients = ApplyCap_(yield.nutrients, CapForStat_(StatId_t::Nutrients, rEffects));
    yield.minerals  = ApplyCap_(yield.minerals, CapForStat_(StatId_t::Minerals, rEffects));
    yield.energy    = ApplyCap_(yield.energy, CapForStat_(StatId_t::Energy, rEffects));
    return yield;
}

TileResources_t AssembleRestrictedTileYield_(TileResources_t subjectToRestriction,
                                             TileResources_t afterRestriction,
                                             const std::vector<ActiveEffect_t>& rEffects)
{
    const TileResources_t capped =
        ApplyTileResourceRestrictions_(subjectToRestriction, rEffects);
    return TileResources_t{
        capped.nutrients + afterRestriction.nutrients,
        capped.energy + afterRestriction.energy,
        capped.minerals + afterRestriction.minerals,
    };
}

// Appends ThisTile-scoped effects projected by units within maxRadius of rOrigin — a unit
// component with a radius effect is a mobile aura (e.g. a sensor pod). Units standing on
// rOrigin itself project at distance 0. TileEffectReaches is the same filter improvements
// and terrain use. Each aura is attributed to the projecting unit's faction.
void AppendUnitAuraEffects_(const Tile& rOrigin, const WorldMap& rWorldMap, int maxRadius,
                            std::vector<ActiveEffect_t>& rOut)
{
    ForEachTileInChebyshevRadius(rOrigin, rWorldMap, maxRadius, true,
        [&](const Tile* pNearby, int distance)
        {
            for (const Unit* pUnit : rWorldMap.GetUnitsOnTile(*pNearby))
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
        });
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

void AppendAreaEffectsFromNeighbors_(const Tile& rOrigin, const WorldMap& rWorldMap,
                                      int maxRadius,
                                      std::vector<ActiveEffect_t>& rOut)
{
    ForEachTileInChebyshevRadius(rOrigin, rWorldMap, maxRadius, false,
        [&](const Tile* pNearby, int distance)
        {
            for (const ImprovementConfig_t* pFeature : pNearby->GetTerrainFeatures())
            {
                if (pFeature)
                {
                    AppendTileEffects(pFeature->effects, pFeature->id, distance, rOut);
                }
            }

            for (const ImprovementConfig_t* pImprovement : pNearby->GetImprovements())
            {
                AppendOwnedImprovementEffects_(*pNearby, *pImprovement, distance, rWorldMap, rOut);
            }
        });
}

void AppendOwnTileEffects_(const Tile& rTile, const WorldMap& rWorldMap,
                           std::vector<ActiveEffect_t>& rOut)
{
    for (const ImprovementConfig_t* pFeature : rTile.GetTerrainFeatures())
    {
        if (pFeature)
        {
            AppendTileEffects(pFeature->effects, pFeature->id, 0, rOut);
        }
    }

    for (const ImprovementConfig_t* pImprovement : rTile.GetImprovements())
    {
        AppendOwnedImprovementEffects_(rTile, *pImprovement, 0, rWorldMap, rOut);
    }
}

bool TileMatchesSelector_(const TileSelector_t& selector, const Tile& rTile, bool isBaseTile)
{
    switch (selector.kind)
    {
        case TileSelectorKind_t::BaseTile:
            return isBaseTile;
        case TileSelectorKind_t::HasImprovement:
            // HasFeature so terrain ids (Fungus, Rocky, …) match the same way as Farm/Mine.
            return selector.improvement && rTile.HasFeature(*selector.improvement);
    }
    return false;
}

// Appends every base-wide StatModifier that carries a tile selector matching rTile.
// Non-selector StatModifiers (flat base bonuses) are left for base-level resolution.
void AppendMatchingTileModifiers_(const std::vector<ActiveEffect_t>& baseEffects,
                                  const Tile& rTile, bool isBaseTile,
                                  std::vector<ActiveEffect_t>& rOut)
{
    for (const ActiveEffect_t& effect : baseEffects)
    {
        const StatModifierEffect_t* pModifier = std::get_if<StatModifierEffect_t>(&effect.config->effect);
        if (pModifier && pModifier->selector && TileMatchesSelector_(*pModifier->selector, rTile, isBaseTile))
        {
            rOut.push_back(effect);
        }
    }
}

void RecomputeMoistureInRadius_(const Tile& rChangedTile, int radius, TileEffectsContext& rCtx,
                                 WorldMap& rWorldMap)
{
    ForEachTileInChebyshevRadius(rChangedTile, rWorldMap, radius, true,
        [&](Tile* pAffected, int /*distance*/)
        {
            rCtx.RecomputeMoisture(*pAffected);
        });
}

} // namespace

TileEffectsContext::TileEffectsContext(WorldMap& rWorldMap, const ImprovementRegistry& rImprovements,
                                       const UnitComponentRegistry* pUnitComponents)
    : m_rWorldMap(rWorldMap)
    , m_rImprovements(rImprovements)
    , m_maxRadius(0)
{
    // Mirror terrain enums/bools as ImprovementConfig_t pointers so hot-path collectors
    // (effects, move costs) never re-resolve string ids against the registry.
    for (std::unique_ptr<Tile>& pTile : rWorldMap.GetTiles())
    {
        pTile->BindImprovements(rImprovements);
    }

    for (const ImprovementConfig_t& rConfig : rImprovements.GetAll())
    {
        m_maxRadius = std::max(m_maxRadius, MaxEffectReach_(rConfig));
    }
    if (pUnitComponents)
    {
        for (const UnitComponentConfig_t& rComponent : pUnitComponents->GetAll())
        {
            for (const EffectConfig_t& rEffect : rComponent.effects)
            {
                if (rEffect.scope == EffectScope_t::ThisTile)
                {
                    m_maxRadius = std::max(m_maxRadius, rEffect.radius);
                }
            }
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

std::vector<ActiveEffect_t> TileEffectsContext::CollectAreaEffects(const Tile& rTile) const
{
    std::vector<ActiveEffect_t> effects;
    AppendOwnTileEffects_(rTile, m_rWorldMap, effects);
    AppendAreaEffectsFromNeighbors_(rTile, m_rWorldMap, m_maxRadius, effects);
    AppendUnitAuraEffects_(rTile, m_rWorldMap, m_maxRadius, effects);
    return effects;
}

TileYieldView_t TileEffectsContext::ResolveTileYield(const Tile& rTile) const
{
    return ResolveYieldFromEffects_(rTile, CollectAreaEffects(rTile), /*pCapEffects*/ nullptr);
}

TileYieldView_t TileEffectsContext::ResolveTileYield(const Tile& rTile, bool isBaseTile,
                                                     const BaseEffects_t& rBaseEffects) const
{
    std::vector<ActiveEffect_t> effects = CollectAreaEffects(rTile);
    AppendMatchingTileModifiers_(rBaseEffects.effects, rTile, isBaseTile, effects);
    return ResolveYieldFromEffects_(rTile, effects, &rBaseEffects);
}

TileYieldView_t TileEffectsContext::ResolvePreviewTileYield(const Tile& rTile,
                                                            const BaseEffects_t& rCapEffects) const
{
    return ResolveYieldFromEffects_(rTile, CollectAreaEffects(rTile), &rCapEffects);
}

int TileEffectsContext::ResolveResource_(const Tile& rTile,
                                         const std::vector<ActiveEffect_t>& effects,
                                         StatId_t stat) const
{
    const EffectContext_t ctx{&rTile};
    return FinalizeResolvedStat(
        ResolveStatModifiers(FilterByStatIdInContext(effects, stat, ctx), SeedFor(stat), &ctx).total);
}

TileYieldView_t TileEffectsContext::ResolveYieldFromEffects_(
    const Tile& rTile, const std::vector<ActiveEffect_t>& effects,
    const BaseEffects_t* pCapEffects) const
{
    std::vector<ActiveEffect_t> filtered = effects;

    // Forest / Borehole / Fungus: drop effects whose sourceId is suppressed.
    // Terrain features (Fungus) carry suppress lists too — not only GetImprovements().
    std::unordered_set<std::string> suppress;
    auto absorbSuppress = [&](const std::vector<const ImprovementConfig_t*>& rConfigs)
    {
        for (const ImprovementConfig_t* pConfig : rConfigs)
        {
            if (!pConfig)
            {
                continue;
            }
            for (const std::string& rId : pConfig->suppressYieldSources)
            {
                suppress.insert(rId);
            }
        }
    };
    absorbSuppress(rTile.GetImprovements());
    absorbSuppress(rTile.GetTerrainFeatures());

    if (!suppress.empty())
    {
        filtered.erase(std::remove_if(filtered.begin(), filtered.end(),
                           [&](const ActiveEffect_t& rEffect) {
                               return suppress.count(rEffect.sourceId) > 0;
                           }),
                       filtered.end());
    }

    std::vector<ActiveEffect_t> beforeRestriction;
    std::vector<ActiveEffect_t> afterRestriction;
    beforeRestriction.reserve(filtered.size());
    afterRestriction.reserve(filtered.size());
    for (const ActiveEffect_t& rEffect : filtered)
    {
        const auto* pMod =
            rEffect.config ? std::get_if<StatModifierEffect_t>(&rEffect.config->effect) : nullptr;
        if (pMod && pMod->applyAfterRestriction)
        {
            afterRestriction.push_back(rEffect);
        }
        else
        {
            beforeRestriction.push_back(rEffect);
        }
    }

    // Energy seed is 0 here: elevation bands come from SolarCollector/Mirror amount_source
    // effects (ElevationEnergySeed), not a hardcoded improvement-id gate.
    const TileResources_t subject{
        ResolveResource_(rTile, beforeRestriction, StatId_t::Nutrients),
        ResolveResource_(rTile, beforeRestriction, StatId_t::Energy),
        ResolveResource_(rTile, beforeRestriction, StatId_t::Minerals),
    };
    const TileResources_t after{
        ResolveResource_(rTile, afterRestriction, StatId_t::Nutrients),
        ResolveResource_(rTile, afterRestriction, StatId_t::Energy),
        ResolveResource_(rTile, afterRestriction, StatId_t::Minerals),
    };

    const TileResources_t potential{
        subject.nutrients + after.nutrients,
        subject.energy + after.energy,
        subject.minerals + after.minerals,
    };
    const TileResources_t effective = pCapEffects
        ? AssembleRestrictedTileYield_(subject, after, pCapEffects->effects)
        : potential;
    return TileYieldView_t{effective, potential};
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
    // Explicit 1.0, not SeedFor: Defense is Additive as a unit stat (armor rating), but the
    // tile lane resolves a different quantity — a multiplier composed of the tile's percent
    // effects, seeded at identity.
    return ResolveStatModifiers(FilterByStatId(applicable, StatId_t::Defense), 1.0).total;
}

void TileEffectsContext::RecomputeMoisture(Tile& rTile)
{
    const std::vector<ActiveEffect_t> effects = CollectAreaEffects(rTile);
    const double tier = ResolveStatModifiers(
        FilterByStatId(effects, StatId_t::MoistureTier),
        static_cast<double>(rTile.GetBaseMoisture())).total;

    const int clamped = std::clamp(static_cast<int>(std::lround(tier)),
                                    static_cast<int>(Moisture_t::Arid), static_cast<int>(Moisture_t::Wet));
    rTile.SetMoisture(static_cast<Moisture_t>(clamped));
}

void TileEffectsContext::AddImprovementWithEffects(Tile& rTile, const std::string& improvementId)
{
    const ImprovementConfig_t& rConfig = m_rImprovements.Get(improvementId);
    rTile.AddImprovement(rConfig);
    RecomputeMoistureInRadius_(rTile, MaxEffectReach_(rConfig), *this, m_rWorldMap);
    if (rConfig.terminatesRiver)
    {
        RecomputeRivers(m_rWorldMap);
    }
}

void TileEffectsContext::RemoveImprovementWithEffects(Tile& rTile, const std::string& improvementId)
{
    const ImprovementConfig_t* pConfig = m_rImprovements.Find(improvementId);
    const int radius = pConfig ? MaxEffectReach_(*pConfig) : 0;
    const bool bTerminatesRiver = pConfig && pConfig->terminatesRiver;

    rTile.RemoveImprovement(improvementId);
    RecomputeMoistureInRadius_(rTile, radius, *this, m_rWorldMap);
    if (bTerminatesRiver)
    {
        RecomputeRivers(m_rWorldMap);
    }
}

} // namespace ac
