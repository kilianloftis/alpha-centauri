#include "game/ecology/BaseEcology.h"

#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/effects/ActiveEffect.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/faction/base/population/PopulationManager.h"
#include "game/faction/base/resources/WorkerAssignmentManager.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"

#include <stdexcept>

namespace ac
{

namespace
{

double ResolveTileContribution_(const Tile& rTile, bool bWorked)
{
    const std::vector<ActiveEffect_t> effects = CollectTileEffects(rTile);
    EffectContext_t ctx;
    ctx.targetTile = &rTile;
    const auto resolve = [&effects, &ctx](StatId_t stat)
    {
        return ResolveStatModifiersTotal(FilterByStatIdInContext(effects, stat, ctx),
                                         SeedFor(stat));
    };
    double total = resolve(StatId_t::EcoDamageContribution);
    if (bWorked)
    {
        total += resolve(StatId_t::EcoDamageWorkedContribution);
    }
    return total;
}

} // namespace

BaseEcology::BaseEcology(const BaseManager& rBase)
    : m_rBase(rBase)
{
}

int BaseEcology::GetDamage() const
{
    const GameState& rGameState = RequireGameState_();
    CollectRevisions_(rGameState, m_scratchRevisions);
    if (m_scratchRevisions != m_cachedStamp)
    {
        m_cachedDamage =
            m_rBase.GetFaction().GetDataContext().ecoDamageCalculator->Calculate(
                CollectInputs_(rGameState));
        m_cachedStamp = m_scratchRevisions;
    }
    return m_cachedDamage;
}

const GameState& BaseEcology::RequireGameState_() const
{
    const GameState* pGameState = m_rBase.GetFaction().GetGameState();
    if (!pGameState)
    {
        throw std::logic_error("BaseEcology: the owning faction is not bound to a GameState");
    }
    return *pGameState;
}

EcoDamageInputs_t BaseEcology::CollectInputs_(const GameState& rGameState) const
{
    const Faction& rFaction = m_rBase.GetFaction();
    const FactionId_t factionId = rFaction.GetFactionId();
    const BaseEffects_t& rEffects = m_rBase.GetBaseEffects();
    const EcologyLedger& rEcology = rGameState.GetEcologyLedger();

    EcoDamageInputs_t inputs;
    inputs.terraformRaw = TerraformRaw_();
    inputs.terraformScale = ResolveBaseStat(rEffects, StatId_t::EcoTerraformScale,
                                            SeedFor(StatId_t::EcoTerraformScale));
    inputs.minerals = m_rBase.GetMineralProduction();
    inputs.mineralOffset = FinalizeResolvedStat(ResolveBaseStat(
        rEffects, StatId_t::EcoMineralOffset, SeedFor(StatId_t::EcoMineralOffset)));
    inputs.cleanMinerals = FinalizeResolvedStat(ResolveFactionStat(
        rFaction.GetActiveEffects(), StatId_t::EcoCleanMinerals,
        SeedFor(StatId_t::EcoCleanMinerals)));
    inputs.fungalBlooms = rEcology.FungalBlooms(factionId);
    inputs.cleanMineralGrants = rEcology.CleanMineralGrants(factionId);
    inputs.virtualMinerals =
        rGameState.GetAtrocityLedger().EcoVirtualMinerals(
            factionId, *rFaction.GetDataContext().atrocitiesConfig)
        + rEcology.VirtualMinerals(factionId);
    inputs.damageReduction = FinalizeResolvedStat(ResolveBaseStat(
        rEffects, StatId_t::EcoDamageReduction, SeedFor(StatId_t::EcoDamageReduction)));
    inputs.techs = static_cast<int>(rFaction.GetResearch().GetDiscoveredTechs().size());
    inputs.ecoScale = ResolveBaseStat(rEffects, StatId_t::EcologicalDamage,
                                      SeedFor(StatId_t::EcologicalDamage));
    return inputs;
}

double BaseEcology::TerraformRaw_() const
{
    // The base's own tile is outside the workable set but still carries improvements (the
    // sea-base term rides Base). No pop works it, so it counts its unworked weight only.
    double total = ResolveTileContribution_(m_rBase.GetTile(), /*bWorked=*/false);
    const WorkerAssignmentManager& rWorkers = m_rBase.GetWorkerAssignments();
    for (const Tile* pTile : rWorkers.GetWorkableTiles())
    {
        // This base's pops only: a tile a supply crawler, a neighbour or an enemy works counts
        // its unworked weight.
        total += ResolveTileContribution_(*pTile, rWorkers.IsTileWorkedByThisBase(pTile));
    }
    return total;
}

void BaseEcology::CollectRevisions_(const GameState& rGameState, std::vector<uint64_t>& rOut) const
{
    const Faction& rFaction = m_rBase.GetFaction();
    const WorldMap& rMap = m_rBase.GetTileEffects().GetWorldMap();
    rOut.clear();
    rOut.push_back(rFaction.GetFactionId());
    rOut.push_back(rFaction.GetEffectsVersion());
    rOut.push_back(rFaction.GetResearch().GetRevision());
    rOut.push_back(m_rBase.GetPopulation().GetRevision());
    rOut.push_back(m_rBase.GetPopulation().GetMoodRevision());
    rOut.push_back(rMap.GetWorkedTiles().GetRevision());
    rOut.push_back(rMap.GetAppearanceRevision());
    rOut.push_back(rGameState.GetAtrocityLedger().GetRevision());
    rOut.push_back(rGameState.GetEcologyLedger().GetRevision());
}

} // namespace ac
