#include "game/research/ResearchRules.h"

#include "game/effects/ActiveEffect.h"
#include "game/effects/IEffectsProvider.h"
#include "game/research/ResearchManager.h"
#include "game/research/TechRegistry.h"

namespace ac
{

TechCostInputs_t TechCostInputs(const ResearchManager& rResearch,
                                const IEffectsProvider* pEffectsProvider,
                                const TechConfig_t& rTech)
{
    const std::vector<TechId>& rDiscovered = rResearch.GetDiscoveredTechs();
    TechCostInputs_t inputs;
    inputs.techs      = static_cast<int>(rDiscovered.size()); // TODO: subtract starting techs; add unknownVarA - unknownVarB
    inputs.mostTechs  = inputs.techs;                         // TODO: max across all factions + unknownVarA
    if (pEffectsProvider)
    {
        const FactionEffects_t& rFactionEffects = pEffectsProvider->GetActiveEffects();
        const double techCost = ResolveFactionStat(rFactionEffects, StatId_t::TechCost,
                                                   SeedFor(StatId_t::TechCost));
        const double techCostDiff = ResolveFactionStat(rFactionEffects, StatId_t::TechCostDiff,
                                                       SeedFor(StatId_t::TechCostDiff));
        inputs.factionTechCostModifier = FinalizeResolvedStat(techCost);
        inputs.diff = FinalizeResolvedStat(techCostDiff);
    }

    std::vector<ActiveEffect_t> thisTechEffects;
    for (const EffectConfig_t& rEffect : rTech.effects)
    {
        if (rEffect.scope == EffectScope_t::ThisTech)
        {
            thisTechEffects.emplace_back(rEffect, rTech.id);
        }
    }
    auto techCostEffects = FilterByStatId(thisTechEffects, StatId_t::TechCost);
    const double techModifier =
        ResolveStatModifiersTotal(techCostEffects, SeedFor(StatId_t::TechCost));
    inputs.techTechCostModifier = FinalizeResolvedStat(techModifier);
    // Remaining fields are placeholder defaults (turns=0, bIsAI=false, etc.)
    return inputs;
}

int ResearchCarriedOver(int accumulated, int cost)
{
    return accumulated - cost;
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

        bool bPrerequisitesMet = true;
        for (const TechId& rPrereq : rConfig.prerequisites)
        {
            if (!rResearch.HasDiscoveredTech(rPrereq))
            {
                bPrerequisitesMet = false;
                break;
            }
        }

        if (bPrerequisitesMet)
        {
            available.push_back(&rConfig);
        }
    }

    return available;
}

} // namespace ac
