#include "lib/effects/TileEffectsContext.h"

#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "lib/effects/ActiveEffect.h"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace ac
{

namespace
{

// Appends a nearby feature's effects when its radius reaches `distance` tiles
// (radius >= distance). Same filtering as a tile's own features (CollectTileEffects):
// only continuous ThisTile-scoped effects take part in tile-level resolution.
void AppendReachingEffects_(const ImprovementConfig_t& rFeature, int distance,
                            std::vector<ActiveEffect_t>& rOut)
{
    if (rFeature.radius < distance)
    {
        return;
    }
    for (const EffectConfig_t& rEffect : rFeature.effects)
    {
        if (rEffect.scope != EffectScope_t::ThisTile)
        {
            continue;
        }
        if (rEffect.persistence == EffectPersistence_t::Instantaneous)
        {
            continue;
        }
        ActiveEffect_t active;
        active.config = &rEffect;
        active.sourceId = rFeature.id;
        rOut.push_back(active);
    }
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
                    AppendReachingEffects_(*pFeature, distance, rOut);
                }
            }

            for (const ImprovementConfig_t* pImprovement : pNearby->GetImprovements())
            {
                AppendReachingEffects_(*pImprovement, distance, rOut);
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

void RecomputeMoistureInRadius_(const Tile& rChangedTile, int radius, const TileEffectsContext& rCtx,
                                 WorldMap& rWorldMap)
{
    ForEachTileInManhattanRadius(rChangedTile, rWorldMap, radius, true,
        [&](Tile* pAffected, int /*distance*/)
        {
            rCtx.RecomputeMoisture(*pAffected);
        });
}

} // namespace

TileEffectsContext::TileEffectsContext(WorldMap& rWorldMap, const ImprovementRegistry& rImprovements)
    : m_rWorldMap(rWorldMap)
    , m_rImprovements(rImprovements)
    , m_maxRadius(0)
{
    for (const ImprovementConfig_t& rConfig : rImprovements.GetAll())
    {
        m_maxRadius = std::max(m_maxRadius, rConfig.radius);
    }
}

std::vector<ActiveEffect_t> TileEffectsContext::CollectTileEffects(const Tile& rTile) const
{
    return ac::CollectTileEffects(rTile, m_rImprovements);
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
    return effects;
}

TileResources_t TileEffectsContext::ResolveTileYield(const Tile& rTile) const
{
    return ResolveYieldFromEffects_(rTile, CollectAreaEffects(rTile));
}

TileResources_t TileEffectsContext::ResolveTileYield(const Tile& rTile, bool isBaseTile,
                                                     const std::vector<ActiveEffect_t>& baseEffects) const
{
    std::vector<ActiveEffect_t> effects = CollectAreaEffects(rTile);
    AppendMatchingTileModifiers_(baseEffects, rTile, isBaseTile, effects);
    return ResolveYieldFromEffects_(rTile, effects);
}

TileResources_t TileEffectsContext::ResolveYieldFromEffects_(const Tile& rTile,
                                                             const std::vector<ActiveEffect_t>& effects) const
{
    const double nutrients = ResolveStatModifiers(FilterByStatId(effects, StatId::Nutrients)).total;
    const double minerals  = ResolveStatModifiers(FilterByStatId(effects, StatId::Minerals)).total;
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
    return ResolveStatModifiers(FilterByStatId(effects, StatId::Defense), 1.0).total;
}

void TileEffectsContext::RecomputeMoisture(Tile& rTile) const
{
    const std::vector<ActiveEffect_t> effects = CollectAreaEffects(rTile);
    const double tier = ResolveStatModifiers(
        FilterByStatId(effects, StatId::MoistureTier),
        static_cast<double>(rTile.GetBaseMoisture())).total;

    const int clamped = std::clamp(static_cast<int>(std::lround(tier)),
                                    static_cast<int>(Moisture::Arid), static_cast<int>(Moisture::Wet));
    rTile.SetMoisture(static_cast<Moisture>(clamped));
}

void TileEffectsContext::AddImprovementWithEffects(Tile& rTile, const std::string& improvementId) const
{
    const ImprovementConfig_t* pConfig = m_rImprovements.Find(improvementId);
    if (!pConfig)
    {
        std::cerr << "[TileEffectsContext] Unknown improvement id '" << improvementId
                  << "' - not added.\n";
        return;
    }

    rTile.AddImprovement(*pConfig);
    RecomputeMoistureInRadius_(rTile, pConfig->radius, *this, m_rWorldMap);
}

void TileEffectsContext::RemoveImprovementWithEffects(Tile& rTile, const std::string& improvementId) const
{
    const ImprovementConfig_t* pConfig = m_rImprovements.Find(improvementId);
    const int radius = pConfig ? pConfig->radius : 0;

    rTile.RemoveImprovement(improvementId);
    RecomputeMoistureInRadius_(rTile, radius, *this, m_rWorldMap);
}

} // namespace ac
