#include "game/effects/InfiltrationRules.h"

#include "game/Faction.h"
#include "game/GameState.h"
#include "game/council/PlanetaryCouncil.h"
#include "game/effects/ActiveEffect.h"
#include "game/faction/DiplomacyLedger.h"

#include <variant>

namespace ac
{

namespace
{

bool IsCouncilMemberTarget_(const GameState& rState, FactionId_t candidate)
{
    const Faction* pCandidate = rState.FindFaction(candidate);
    if (!pCandidate)
    {
        return false;
    }
    const PlanetaryCouncil* pCouncil = rState.GetPlanetaryCouncil();
    if (!pCouncil)
    {
        // No council ⇒ no council members. Do not treat participatesInCouncil
        // (eligibility for council construction) as membership.
        return false;
    }
    return pCouncil->IsCouncilMember(*pCandidate);
}

bool ContinuousEffectGrantsInfiltration_(const ActiveEffect_t& rEffect,
                                         FactionId_t infiltrator,
                                         FactionId_t target,
                                         const GameState& rState)
{
    if (rEffect.config->persistence != EffectPersistence_t::Continuous
        || !std::get_if<InfiltrationEffect_t>(&rEffect.config->effect))
    {
        return false;
    }
    return FactionFilterCoversTarget(*rEffect.config, infiltrator, target, rState);
}

} // namespace

bool FactionFilterCoversTarget(const EffectConfig_t& rConfig,
                               FactionId_t beneficiary,
                               FactionId_t candidate,
                               const GameState& rState,
                               std::optional<FactionId_t> actionTarget)
{
    if (candidate == beneficiary)
    {
        return false;
    }

    if (!rConfig.factionFilter)
    {
        // Default: WorldGlobal reaches every other faction; other scopes need an explicit filter.
        return rConfig.scope == EffectScope_t::WorldGlobal;
    }

    switch (rConfig.factionFilter->kind)
    {
        case FactionFilterKind_t::ActionTarget:
            return actionTarget.has_value() && *actionTarget == candidate;
        case FactionFilterKind_t::CouncilMembers:
            return IsCouncilMemberTarget_(rState, candidate);
    }
    return false;
}

void ApplyInfiltrationEffect(GameState& rState,
                             const Faction& rBeneficiary,
                             const EffectConfig_t& rConfig,
                             std::optional<FactionId_t> actionTarget)
{
    if (rConfig.persistence != EffectPersistence_t::Instantaneous
        || !std::get_if<InfiltrationEffect_t>(&rConfig.effect))
    {
        return;
    }

    const FactionId_t beneficiaryId = rBeneficiary.GetFactionId();
    DiplomacyLedger& rDiplomacy = rState.GetDiplomacyLedger();
    for (const Faction& rFaction : rState.Factions())
    {
        const FactionId_t candidateId = rFaction.GetFactionId();
        if (FactionFilterCoversTarget(rConfig, beneficiaryId, candidateId, rState, actionTarget))
        {
            rDiplomacy.SetInfiltration(beneficiaryId, candidateId, true);
        }
    }
}

void ApplyInfiltrationEffects(GameState& rState,
                              const Faction& rBeneficiary,
                              const std::vector<EffectConfig_t>& rEffects,
                              std::optional<FactionId_t> actionTarget)
{
    for (const EffectConfig_t& rEffect : rEffects)
    {
        ApplyInfiltrationEffect(rState, rBeneficiary, rEffect, actionTarget);
    }
}

bool HasInfiltration(const GameState& rState, FactionId_t infiltrator, FactionId_t target)
{
    if (infiltrator == target)
    {
        return false;
    }
    if (rState.GetDiplomacyLedger().HasInfiltration(infiltrator, target))
    {
        return true;
    }

    const Faction* pInfiltrator = rState.FindFaction(infiltrator);
    if (!pInfiltrator)
    {
        return false;
    }

    for (const ActiveEffect_t& rEffect : pInfiltrator->GetActiveEffects().effects)
    {
        if (ContinuousEffectGrantsInfiltration_(rEffect, infiltrator, target, rState))
        {
            return true;
        }
    }

    if (const PlanetaryCouncil* pCouncil = rState.GetPlanetaryCouncil())
    {
        for (const ActiveEffect_t& rEffect : pCouncil->CollectFactionEffects(*pInfiltrator))
        {
            if (ContinuousEffectGrantsInfiltration_(rEffect, infiltrator, target, rState))
            {
                return true;
            }
        }
    }

    return false;
}

} // namespace ac
