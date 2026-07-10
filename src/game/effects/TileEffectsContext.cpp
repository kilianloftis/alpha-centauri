#include "game/effects/TileEffectsContext.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/Unit.h"
#include "game/units/UnitComponentRegistry.h"
#include "game/units/UnitDesign.h"
#include "game/effects/ActiveEffect.h"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace ac
{

namespace
{

// Appends ThisTile-scoped effects projected by units within maxRadius of rOrigin — a unit
// component with a radius effect is a mobile aura (e.g. a sensor pod). Units standing on
// rOrigin itself project at distance 0. TileEffectReaches is the same filter improvements
// and terrain use.
void AppendUnitAuraEffects_(const Tile& rOrigin, const WorldMap& rWorldMap, int maxRadius,
                            std::vector<ActiveEffect_t>& rOut)
{
    ForEachTileInManhattanRadius(rOrigin, rWorldMap, maxRadius, true,
        [&](const Tile* pNearby, int distance)
        {
            for (const Unit* pUnit : rWorldMap.GetUnitsOnTile(*pNearby))
            {
                if (!pUnit)
                {
                    continue;
                }
                for (ActiveEffect_t& rActive : pUnit->GetDesign().CollectEffects())
                {
                    if (TileEffectReaches(*rActive.config, distance))
                    {
                        rOut.push_back(std::move(rActive));
                    }
                }
            }
        });
}

// How far an improvement's effects can reach: the improvement-level radius (kept as the
// parse-time default) or any larger per-effect radius.
int MaxEffectReach_(const ImprovementConfig_t& rConfig)
{
    int reach = rConfig.radius;
    for (const EffectConfig_t& rEffect : rConfig.effects)
    {
        reach = std::max(reach, rEffect.radius);
    }
    return reach;
}

void AppendAreaEffectsFromNeighbors_(const Tile& rOrigin, const WorldMap& rWorldMap,
                                      const ImprovementRegistry& rImprovements,
                                      int maxRadius,
                                      std::vector<ActiveEffect_t>& rOut)
{
    ForEachTileInManhattanRadius(rOrigin, rWorldMap, maxRadius, false,
        [&](const Tile* pNearby, int distance)
        {
            // Terrain features resolve by id via the registry; improvements are already configs.
            for (const std::string& featureId : pNearby->GetTerrainFeatureIds())
            {
                if (const ImprovementConfig_t* pFeature = rImprovements.Find(featureId))
                {
                    AppendTileEffects(pFeature->effects, pFeature->id, distance, rOut);
                }
            }

            for (const ImprovementConfig_t* pImprovement : pNearby->GetImprovements())
            {
                AppendTileEffects(pImprovement->effects, pImprovement->id, distance, rOut);
            }
        });
}

bool TileMatchesSelector_(const TileSelector_t& selector, const Tile& rTile, bool isBaseTile)
{
    switch (selector.kind)
    {
        case TileSelectorKind::BaseTile:
            return isBaseTile;
        case TileSelectorKind::HasImprovement:
            return selector.improvement && rTile.HasImprovement(*selector.improvement);
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
        if (!effect.config)
        {
            continue;
        }
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
    ForEachTileInManhattanRadius(rChangedTile, rWorldMap, radius, true,
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

std::vector<ActiveEffect_t> TileEffectsContext::CollectAreaEffects(const Tile& rTile) const
{
    std::vector<ActiveEffect_t> effects = ac::CollectTileEffects(rTile, m_rImprovements);
    AppendAreaEffectsFromNeighbors_(rTile, m_rWorldMap, m_rImprovements, m_maxRadius, effects);
    AppendUnitAuraEffects_(rTile, m_rWorldMap, m_maxRadius, effects);
    return effects;
}

TileResources_t TileEffectsContext::ResolveTileYield(const Tile& rTile) const
{
    return ResolveYieldFromEffects_(rTile, CollectAreaEffects(rTile));
}

TileResources_t TileEffectsContext::ResolveTileYield(const Tile& rTile, bool isBaseTile,
                                                     const BaseEffects_t& rBaseEffects) const
{
    std::vector<ActiveEffect_t> effects = CollectAreaEffects(rTile);
    AppendMatchingTileModifiers_(rBaseEffects.effects, rTile, isBaseTile, effects);
    return ResolveYieldFromEffects_(rTile, effects);
}

TileResources_t TileEffectsContext::ResolveYieldFromEffects_(const Tile& rTile,
                                                             const std::vector<ActiveEffect_t>& effects) const
{
    // Energy deliberately bypasses SeedFor: it is an Additive stat, but this site scales a
    // raw base — the tile's elevation energy seed.
    const double nutrients = ResolveStatModifiers(FilterByStatId(effects, StatId::Nutrients), SeedFor(StatId::Nutrients)).total;
    const double minerals  = ResolveStatModifiers(FilterByStatId(effects, StatId::Minerals), SeedFor(StatId::Minerals)).total;
    const double energy    = ResolveStatModifiers(
        FilterByStatId(effects, StatId::Energy), static_cast<double>(rTile.GetElevationEnergySeed())).total;

    return TileResources_t{
        static_cast<int>(nutrients),
        static_cast<int>(energy),
        static_cast<int>(minerals)
    };
}

double TileEffectsContext::ResolveTileDefenseMultiplier(const Tile& rTile) const
{
    const std::vector<ActiveEffect_t> effects = CollectAreaEffects(rTile);
    // Explicit 1.0, not SeedFor: Defense is Additive as a unit stat (armor rating), but the
    // tile lane resolves a different quantity — a multiplier composed of the tile's percent
    // effects, seeded at identity.
    return ResolveStatModifiers(FilterByStatId(effects, StatId::Defense), 1.0).total;
}

void TileEffectsContext::RecomputeMoisture(Tile& rTile)
{
    const std::vector<ActiveEffect_t> effects = CollectAreaEffects(rTile);
    const double tier = ResolveStatModifiers(
        FilterByStatId(effects, StatId::MoistureTier),
        static_cast<double>(rTile.GetBaseMoisture())).total;

    const int clamped = std::clamp(static_cast<int>(std::lround(tier)),
                                    static_cast<int>(Moisture::Arid), static_cast<int>(Moisture::Wet));
    rTile.SetMoisture(static_cast<Moisture>(clamped));
}

void TileEffectsContext::AddImprovementWithEffects(Tile& rTile, const std::string& improvementId)
{
    const ImprovementConfig_t* pConfig = m_rImprovements.Find(improvementId);
    if (!pConfig)
    {
        std::cerr << "[TileEffectsContext] Unknown improvement id '" << improvementId
                  << "' - not added.\n";
        return;
    }

    rTile.AddImprovement(*pConfig);
    RecomputeMoistureInRadius_(rTile, MaxEffectReach_(*pConfig), *this, m_rWorldMap);
}

void TileEffectsContext::RemoveImprovementWithEffects(Tile& rTile, const std::string& improvementId)
{
    const ImprovementConfig_t* pConfig = m_rImprovements.Find(improvementId);
    const int radius = pConfig ? MaxEffectReach_(*pConfig) : 0;

    rTile.RemoveImprovement(improvementId);
    RecomputeMoistureInRadius_(rTile, radius, *this, m_rWorldMap);
}

} // namespace ac
