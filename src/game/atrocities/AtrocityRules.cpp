#include "game/atrocities/AtrocityRules.h"

namespace ac
{

namespace
{

std::optional<FactionId_t> FirstForeign_(const std::vector<FactionId_t>& rOwners,
                                         FactionId_t detonator)
{
    for (const FactionId_t owner : rOwners)
    {
        if (owner != detonator)
        {
            return owner;
        }
    }
    return std::nullopt;
}

} // namespace

AtrocitySeverityId_t EffectiveSeverityId(AtrocitySeverityId_t authored, int priorSimpleCount,
                                         int simpleThreshold)
{
    if (simpleThreshold <= 0 || authored == AtrocitySeverityId_t::Major)
    {
        return authored;
    }
    if (priorSimpleCount + 1 <= simpleThreshold)
    {
        return authored;
    }
    return AtrocitySeverityId_t::Major;
}

bool PenaltiesApply(bool bCharterInForce, bool bSpeciesExempt)
{
    return bCharterInForce && !bSpeciesExempt;
}

bool SpeciesExemptionApplies(FactionSpecies_t perpetrator,
                             std::optional<FactionSpecies_t> victimSpecies)
{
    if (perpetrator == FactionSpecies_t::Progenitor)
    {
        return true;
    }
    return victimSpecies.has_value() && *victimSpecies == FactionSpecies_t::Progenitor;
}

int SanctionYearsAdded(const AtrocitiesConfig_t& rConfig, int simpleCountAfter)
{
    return rConfig.sanctionYearsPerAtrocity * simpleCountAfter;
}

std::optional<FactionId_t> BlastVictim(const std::vector<FactionId_t>& rBaseOwnersDestroyed,
                                       const std::vector<FactionId_t>& rUnitOwnersDestroyed,
                                       FactionId_t detonator)
{
    if (const std::optional<FactionId_t> owner = FirstForeign_(rBaseOwnersDestroyed, detonator))
    {
        return owner;
    }
    return FirstForeign_(rUnitOwnersDestroyed, detonator);
}

} // namespace ac
