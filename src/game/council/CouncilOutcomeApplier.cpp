#include "game/council/CouncilOutcomeApplier.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/council/CouncilProposalConfig.h"
#include "game/council/CouncilRulesConfig.h"
#include "game/effects/InfiltrationRules.h"
#include "game/faction/EconomyManager.h"

#include <variant>

namespace ac
{

CouncilOutcomeApplier::CouncilOutcomeApplier(const CouncilRulesConfig_t& rRules)
    : m_rRules(rRules)
{
}

void CouncilOutcomeApplier::ApplyInstantaneousEffects(const std::vector<Faction*>& rMembers,
                                                      const CouncilProposalConfig_t& rConfig)
{
    for (const EffectConfig_t& rEffect : rConfig.effects)
    {
        if (rEffect.persistence != EffectPersistence_t::Instantaneous)
        {
            continue;
        }
        if (const auto* pGrant = std::get_if<GrantEnergyEffect_t>(&rEffect.effect))
        {
            for (Faction* pMember : rMembers)
            {
                pMember->GetEconomy().AddEnergy(pGrant->amount);
            }
        }
        else if (std::get_if<WorldParameterEffect_t>(&rEffect.effect))
        {
            // TODO: world-map mutation (sea level, climate) unfolds gradually over turns
            // through the WorldEvents system. Once that system exposes a trigger API,
            // request the world event here. The council must never mutate the map itself.
        }
    }
}

void CouncilOutcomeApplier::ApplyGovernor(GameState& rGameState,
                                          const std::vector<Faction*>& /*rMembers*/,
                                          const Faction& rGovernor)
{
    // Instantaneous governor_effects (e.g. sticky Infiltration). Continuous entries —
    // including Continuous Infiltration with CouncilMembers — live in CouncilEffects and
    // are honored by HasInfiltration at query time.
    ApplyInfiltrationEffects(rGameState, rGovernor, m_rRules.governorEffects);
}

} // namespace ac
