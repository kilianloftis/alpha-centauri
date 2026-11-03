#include "game/research/ResearchRules.h"

#include "game/CurrentGameState.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/effects/ActiveEffect.h"
#include "game/map/WorldMap.h"
#include "game/effects/IEffectsProvider.h"
#include "game/faction/Faction.h"
#include "game/research/ResearchManager.h"
#include "game/research/TechCostCalculator.h"
#include "game/research/TechRegistry.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <string>

namespace ac
{
namespace
{

int MostTechsAcrossFactions_(const GameState& rGame)
{
    int most = 0;
    for (const Faction& rFaction : rGame.Factions())
    {
        const std::size_t count = rFaction.GetResearch().GetDiscoveredTechs().size();
        const int discovered = static_cast<int>(count);
        most = std::max(most, discovered);
    }
    return most;
}

const Faction* FindResearchOwner_(const GameState& rGame, const ResearchManager& rResearch)
{
    for (const Faction& rFaction : rGame.Factions())
    {
        if (&rFaction.GetResearch() == &rResearch)
        {
            return &rFaction;
        }
    }
    return nullptr;
}

double ThisTechCostScale_(const TechConfig_t& rTech)
{
    std::vector<ActiveEffect_t> thisTechEffects;
    for (const EffectConfig_t& rEffect : rTech.effects)
    {
        if (rEffect.scope == EffectScope_t::ThisTech)
        {
            thisTechEffects.emplace_back(rEffect, rTech.id, nullptr);
        }
    }
    auto techCostEffects = FilterByStatId(thisTechEffects, StatId_t::TechCost);
    return ResolveStatModifiersTotal(techCostEffects, SeedFor(StatId_t::TechCost), nullptr);
}

void FillFactionEffectInputs_(TechCostInputs_t& rInputs, const IEffectsProvider* pEffectsProvider)
{
    if (pEffectsProvider == nullptr)
    {
        return;
    }
    const FactionEffects_t& rFactionEffects = pEffectsProvider->GetActiveEffects();
    const double techCostDiff = ResolveFactionStat(rFactionEffects, StatId_t::TechCostDiff,
                                                   SeedFor(StatId_t::TechCostDiff), nullptr);
    const double catchUp = ResolveFactionStat(rFactionEffects, StatId_t::TechCostCatchUp,
                                              SeedFor(StatId_t::TechCostCatchUp), nullptr);
    const double turnLag = ResolveFactionStat(rFactionEffects, StatId_t::TechCostTurnLag,
                                              SeedFor(StatId_t::TechCostTurnLag), nullptr);
    const double research = ResolveFactionStat(rFactionEffects, StatId_t::TechCostResearch,
                                               SeedFor(StatId_t::TechCostResearch), nullptr);
    rInputs.diff = FinalizeResolvedStat(techCostDiff);
    rInputs.catchUpDiff = FinalizeResolvedStat(catchUp);
    rInputs.bTurnLag = FinalizeResolvedStat(turnLag) != 0;
    rInputs.research = FinalizeResolvedStat(research);
}

double FactionTechCostScale_(const IEffectsProvider* pEffectsProvider)
{
    if (pEffectsProvider == nullptr)
    {
        return SeedFor(StatId_t::TechCost);
    }
    const FactionEffects_t& rEffects = pEffectsProvider->GetActiveEffects();
    return ResolveFactionStat(rEffects, StatId_t::TechCost, SeedFor(StatId_t::TechCost), nullptr);
}

double TechCostScale_(const IEffectsProvider* pEffectsProvider, const TechConfig_t& rTech)
{
    return FactionTechCostScale_(pEffectsProvider) * ThisTechCostScale_(rTech);
}

// rLevels memoises the walk, since prerequisite chains share their roots.
int TechLevel_(const TechRegistry& rRegistry, const TechId& rTechId,
               std::map<TechId, int>& rLevels)
{
    const auto it = rLevels.find(rTechId);
    if (it != rLevels.end())
    {
        return it->second;
    }
    int deepest = 0;
    for (const TechId& rPrerequisite : rRegistry.Get(rTechId).prerequisites)
    {
        deepest = std::max(deepest, TechLevel_(rRegistry, rPrerequisite, rLevels));
    }
    rLevels.emplace(rTechId, deepest + 1);
    return deepest + 1;
}

} // namespace

TechCostInputs_t TechCostInputs(const ResearchManager& rResearch,
                                const IEffectsProvider* pEffectsProvider)
{
    // TODO: terranx leaves techs granted during setup (faction TECH, CurrentTurn 0) and with
    // SkipTechScreenA set out of TECHS and MOSTTECHS. Exclude them once factions start with techs.
    const std::size_t discoveredCount = rResearch.GetDiscoveredTechs().size();
    const int discovered = static_cast<int>(discoveredCount);

    TechCostInputs_t inputs;
    inputs.techs = std::max(1, discovered);
    inputs.mostTechs = inputs.techs;
    FillFactionEffectInputs_(inputs, pEffectsProvider);

    const GameState* pGame = FindCurrentGameState();
    if (pGame == nullptr)
    {
        return inputs;
    }

    inputs.mostTechs = MostTechsAcrossFactions_(*pGame);
    inputs.turns = pGame->GetTurnNumber();
    inputs.mapAreaSqRoot = pGame->GetWorldMap().GetAreaSqRoot();
    const Faction* pOwner = FindResearchOwner_(*pGame, rResearch);
    if (pOwner != nullptr)
    {
        inputs.bIsAI = !pOwner->IsPlayerControlled();
    }
    return inputs;
}

int ResearchFormulaCost(const ResearchManager& rResearch,
                        const IEffectsProvider* pEffectsProvider,
                        const TechCostCalculator& rCalculator)
{
    return rCalculator.CalculateCost(TechCostInputs(rResearch, pEffectsProvider));
}

int ScaledTechCost(int formulaCost, const IEffectsProvider* pEffectsProvider,
                   const TechConfig_t& rTech)
{
    const double scaled = static_cast<double>(formulaCost) * TechCostScale_(pEffectsProvider, rTech);
    const int cost = static_cast<int>(std::trunc(scaled));
    if (cost <= 0)
    {
        throw std::runtime_error("Tech cost scale produced " + std::to_string(cost)
                                 + " for tech '" + rTech.id + "'; costs must be positive");
    }
    return cost;
}

std::vector<const TechConfig_t*> AvailableTechs(const ResearchManager& rResearch)
{
    std::vector<const TechConfig_t*> available;
    const auto& allConfigs = rResearch.GetTechRegistry().GetAll();

    for (const TechConfig_t& rConfig : allConfigs)
    {
        if (rResearch.HasDiscoveredTech(rConfig.id))
        {
            continue;
        }

        const bool bPrerequisitesMet =
            std::ranges::all_of(rConfig.prerequisites, [&rResearch](const TechId& rPrereq) {
                return rResearch.HasDiscoveredTech(rPrereq);
            });
        if (bPrerequisitesMet)
        {
            available.push_back(&rConfig);
        }
    }

    return available;
}

int TechLevel(const TechId& rTechId)
{
    std::map<TechId, int> levels;
    return TechLevel_(*CurrentGameState().GetGameData().techRegistry, rTechId, levels);
}

int HighestTechLevel(const ResearchManager& rResearch)
{
    const TechRegistry& rRegistry = *CurrentGameState().GetGameData().techRegistry;
    std::map<TechId, int> levels;
    int highest = 0;
    for (const TechId& rTechId : rResearch.GetDiscoveredTechs())
    {
        highest = std::max(highest, TechLevel_(rRegistry, rTechId, levels));
    }
    return highest;
}

} // namespace ac
