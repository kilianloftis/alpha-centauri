#include "game/units/MoveCostCalculator.h"

#include "game/Faction.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/FactionExploredMap.h"
#include "game/map/ImprovementConfigParser.h"
#include "game/map/ImprovementRegistry.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/units/MovementRules.h"
#include "game/units/Unit.h"

#include <magic_enum.hpp>
#include <algorithm>
#include <string_view>
#include <vector>

namespace ac
{

namespace
{

constexpr std::string_view k_FungusId = magic_enum::enum_name(TerrainFeature_t::Fungus);

} // namespace

// --- Query -------------------------------------------------------------------

MoveCostCalculator::Query::Query(const MoveCostCalculator& rCalc,
                                 const Unit& rUnit,
                                 const WorldMap& rWorldMap)
    : m_rCalc(rCalc)
    , m_rUnit(rUnit)
    , m_rWorldMap(rWorldMap)
{
    m_profile.ignoresDifficultTerrain = ResolveFlag(rUnit, RuleFlagId_t::IgnoreDifficultTerrain);
}

EntryTerms_t MoveCostCalculator::Query::EntryTerms(const Tile& rTile) const
{
    const int seed = m_rCalc.MaxFeatureCost_(rTile, m_profile)
                         .value_or(m_rCalc.m_constants.DefaultMoveCostFragments());

    std::vector<ActiveEffect_t> effects = CollectTileEffects(rTile);
    const UnitEffects_t live = CollectLiveUnitEffects(m_rUnit);
    effects.insert(effects.end(), live.effects.begin(), live.effects.end());

    EffectContext_t ctx;
    ctx.targetTile = &rTile;
    ctx.pUnit = &m_rUnit;
    const StatBreakdown_t breakdown = ResolveStatModifiers(
        FilterByStatIdInContext(effects, StatId_t::MoveCost, ctx),
        static_cast<double>(seed),
        &ctx);

    EntryTerms_t terms;
    terms.costFragments = FinalizeResolvedStat(breakdown.total);

    bool bClamp = false;
    for (const StatBreakdown_t::Contribution_t& rContribution : breakdown.contributions)
    {
        if (rContribution.op == ModifierOp_t::MaxClamp)
        {
            bClamp = true;
            break;
        }
    }
    if (rTile.GetHasFungus() && !bClamp)
    {
        terms.bEndsTurn = true;
        terms.bRequiresFullCost = !HasFriendlyOccupant(m_rUnit, rTile, m_rWorldMap);
    }
    return terms;
}

int MoveCostCalculator::Query::PlannedCostFragments(const Tile& rTile) const
{
    const FactionExploredMap& rExplored = m_rUnit.GetFaction().GetExploredMap();
    if (rExplored.IsSized() && !rExplored.IsExplored(rTile))
    {
        return m_rCalc.m_constants.DefaultMoveCostFragments();
    }

    const EntryTerms_t terms = EntryTerms(rTile);
    if (!terms.bEndsTurn)
    {
        return terms.costFragments;
    }

    const int allotment =
        m_rUnit.GetMovementPoints() * MovementConstants_t::k_moveFragmentsPerPoint;
    if (allotment <= 0)
    {
        return terms.costFragments;
    }
    const int bankingTurns = (terms.costFragments + allotment - 1) / allotment;
    const int turns = terms.bRequiresFullCost ? std::max(bankingTurns, 1) : 1;
    return turns * allotment;
}

// --- MoveCostCalculator ------------------------------------------------------

MoveCostCalculator::MoveCostCalculator(const ImprovementRegistry& rImprovements,
                                       MovementConstants_t constants)
    : m_rImprovements(rImprovements)
    , m_constants(std::move(constants))
{
}

MoveCostCalculator::Query MoveCostCalculator::ForUnit(const Unit& rUnit,
                                                      const WorldMap& rWorldMap) const
{
    return Query(*this, rUnit, rWorldMap);
}

int MoveCostCalculator::FeatureMoveCostFragments_(const ImprovementConfig_t& rConfig,
                                                  const Query::UnitMoveProfile_t& rProfile,
                                                  int defaultFragments) const
{
    int cost = *rConfig.moveCostFragments;
    // Difficult terrain (Rocky, Forest, …) is anything above the default cost. Fungus
    // stays at its own move_cost; a move_cost MaxClamp ceilings it afterwards.
    if (rProfile.ignoresDifficultTerrain && rConfig.id != k_FungusId)
    {
        cost = std::min(cost, defaultFragments);
    }
    return cost;
}

std::optional<int> MoveCostCalculator::MaxFeatureCost_(
    const Tile& rTile, const Query::UnitMoveProfile_t& rProfile) const
{
    const int defaultFragments = m_constants.DefaultMoveCostFragments();
    std::optional<int> maxCost;
    const auto accumulate = [&](const ImprovementConfig_t* pConfig)
    {
        if (!pConfig || !pConfig->moveCostFragments.has_value())
        {
            return;
        }
        const int cost = FeatureMoveCostFragments_(*pConfig, rProfile, defaultFragments);
        maxCost = maxCost.has_value() ? std::max(*maxCost, cost) : cost;
    };
    for (const ImprovementConfig_t* pConfig : rTile.GetTerrainFeatures())
    {
        accumulate(pConfig);
    }
    for (const ImprovementConfig_t* pConfig : rTile.GetImprovements())
    {
        accumulate(pConfig);
    }
    return maxCost;
}

} // namespace ac
