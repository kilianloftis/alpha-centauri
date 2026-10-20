#include "game/ecology/EcologyRules.h"

#include "game/CurrentGameState.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/atrocities/AtrocityConfig.h"
#include "game/atrocities/AtrocityLedger.h"
#include "game/base/BaseManager.h"
#include "game/ecology/EcoDamageConfig.h"
#include "game/ecology/EcologyLedger.h"
#include "game/economy/WorkerAssignmentManager.h"
#include "game/effects/ActiveEffect.h"
#include "game/faction/Faction.h"
#include "game/map/ImprovementIds.h"
#include "game/map/MapUtils.h"
#include "game/map/Tile.h"
#include "game/map/WorldMap.h"
#include "game/natives/NativeDesign.h"
#include "game/natives/NativeUnitRegistry.h"
#include "game/research/ResearchManager.h"
#include "game/units/UnitDomain.h"

#include <algorithm>
#include <vector>

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

double TerraformRaw_(const BaseManager& rBase)
{
    // The base's own tile is outside the workable set but still carries improvements (the
    // sea-base term rides Base). No pop works it, so it counts its unworked weight only.
    double total = ResolveTileContribution_(rBase.GetTile(), /*bWorked=*/false);
    const WorkerAssignmentManager& rWorkers = rBase.GetWorkerAssignments();
    for (const Tile* pTile : rWorkers.GetWorkableTiles())
    {
        // This base's pops only: a tile a supply crawler, a neighbour or an enemy works counts
        // its unworked weight.
        total += ResolveTileContribution_(*pTile, rWorkers.IsTileWorkedByThisBase(pTile));
    }
    return total;
}

} // namespace

EcoDamageInputs_t EcoDamageInputs(const BaseManager& rBase)
{
    const GameState& rGameState = CurrentGameState();
    const Faction& rFaction = rBase.GetFaction();
    const FactionId_t factionId = rFaction.GetFactionId();
    const BaseEffects_t& rEffects = rBase.GetBaseEffects();
    const EcologyLedger& rEcology = rGameState.GetEcologyLedger();

    EcoDamageInputs_t inputs;
    inputs.terraformRaw = TerraformRaw_(rBase);
    inputs.terraformScale = ResolveBaseStat(rEffects, StatId_t::EcoTerraformScale,
                                            SeedFor(StatId_t::EcoTerraformScale));
    inputs.minerals = rBase.GetMineralProduction();
    inputs.mineralOffset = FinalizeResolvedStat(ResolveBaseStat(
        rEffects, StatId_t::EcoMineralOffset, SeedFor(StatId_t::EcoMineralOffset)));
    inputs.cleanMinerals = FinalizeResolvedStat(ResolveFactionStat(
        rFaction.GetActiveEffects(), StatId_t::EcoCleanMinerals,
        SeedFor(StatId_t::EcoCleanMinerals)));
    inputs.fungalBlooms = rEcology.FungalBlooms(factionId);
    inputs.cleanMineralGrants = rEcology.CleanMineralGrants(factionId);
    const int atrocityMinerals =
        AtrocityEcoMinerals(rGameState.GetAtrocityLedger(), factionId);
    inputs.virtualMinerals = atrocityMinerals + rEcology.VirtualMinerals(factionId);
    inputs.damageReduction = FinalizeResolvedStat(ResolveBaseStat(
        rEffects, StatId_t::EcoDamageReduction, SeedFor(StatId_t::EcoDamageReduction)));
    inputs.techs = static_cast<int>(rFaction.GetResearch().GetDiscoveredTechs().size());
    inputs.ecoScale = ResolveBaseStat(rEffects, StatId_t::EcologicalDamage,
                                      SeedFor(StatId_t::EcologicalDamage));
    return inputs;
}

int AtrocityEcoMinerals(const AtrocityLedger& rLedger, FactionId_t perpetrator)
{
    const AtrocitiesConfig_t& rConfig = *CurrentGameState().GetGameData().atrocitiesConfig;
    int total = 0;
    for (const AtrocityRecord_t& rRecord : rLedger.Records())
    {
        if (rRecord.perpetrator != perpetrator || !rRecord.bCounted)
        {
            continue;
        }
        total += rConfig.For(rRecord.severity).ecoVirtualMinerals;
    }
    return total;
}

int FungalPopChance(const BaseManager& rBase)
{
    const EcoDamageConfig_t& rConfig = *CurrentGameState().GetGameData().ecoDamageConfig;
    return std::min(rConfig.maxChancePercent, rBase.GetEcologicalDamage());
}

std::vector<const Tile*> FungalPopTiles(const BaseManager& rBase)
{
    std::vector<const Tile*> candidates;
    for (const Tile* pTile : rBase.GetWorkerAssignments().GetWorkableTiles())
    {
        if (pTile->HasImprovement(ImprovementIds::k_Base)
            || pTile->HasTerrainFeature(ImprovementIds::k_Fungus))
        {
            continue;
        }
        candidates.push_back(pTile);
    }
    return candidates;
}

namespace
{

bool DomainCanHold_(UnitDomain_t domain, const Tile& rTile)
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

} // namespace

BloomCandidates_t BloomCandidateTiles(const Tile& rOrigin)
{
    BloomCandidates_t candidates;
    if (!rOrigin.HasImprovement(ImprovementIds::k_Base))
    {
        candidates.pOrigin = &rOrigin;
    }
    const WorldMap& rWorldMap = CurrentGameState().GetWorldMap();
    ForEachTileInChebyshevRadius(rOrigin, rWorldMap, /*radius=*/1, /*includeOrigin=*/false,
        [&](const Tile* pTile, int /*distance*/)
        {
            if (pTile->HasImprovement(ImprovementIds::k_Base)
                || pTile->HasTerrainFeature(ImprovementIds::k_Fungus))
            {
                return;
            }
            candidates.neighbours.push_back(pTile);
        });
    return candidates;
}

std::vector<const NativeUnitConfig_t*> EligibleLifeforms(const Tile& rTile,
                                                         const NativeUnitRegistry& rNatives)
{
    std::vector<const NativeUnitConfig_t*> eligible;
    for (const NativeUnitConfig_t& rConfig : rNatives.GetAll())
    {
        const NativeDesign design(rConfig);
        if (design.IsNativeLife() && DomainCanHold_(design.GetDomain(), rTile))
        {
            eligible.push_back(&rConfig);
        }
    }
    return eligible;
}

} // namespace ac
