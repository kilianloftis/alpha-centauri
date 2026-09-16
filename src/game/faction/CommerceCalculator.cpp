#include "game/faction/CommerceCalculator.h"

#include "game/Faction.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/CommerceConfig.h"
#include "game/faction/DiplomacyLedger.h"
#include "game/faction/ResearchManager.h"
#include "game/faction/base/BaseManager.h"
#include "game/research/TechConfigParser.h"
#include "game/research/TechRegistry.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <variant>
#include <vector>

namespace ac
{
namespace
{

// Sum of commerce_rating Add amounts on a faction's discovered techs only (no SE / faction
// bonuses). Used for totalCommerceTech across the planet.
int TechCommerceRatingContribution_(const Faction& rFaction)
{
    const TechRegistry& rTechs = *rFaction.GetDataContext().techRegistry;
    int total = 0;
    for (const TechId& rId : rFaction.GetResearch().GetDiscoveredTechs())
    {
        const TechConfig_t* pTech = rTechs.Find(rId);
        if (pTech == nullptr)
        {
            continue;
        }
        for (const EffectConfig_t& rEffect : pTech->effects)
        {
            const auto* pStat = std::get_if<StatModifierEffect_t>(&rEffect.effect);
            if (pStat == nullptr || pStat->stat != StatId_t::CommerceRating
                || pStat->op != ModifierOp_t::Add)
            {
                continue;
            }
            total += FinalizeResolvedStat(pStat->amount);
        }
    }
    return total;
}

int TotalTechCommerceRating_(const GameState& rGameState)
{
    int total = 0;
    for (const Faction& rFaction : rGameState.Factions())
    {
        total += TechCommerceRatingContribution_(rFaction);
    }
    return total;
}

struct RankedBase_t
{
    const BaseManager* pBase = nullptr;
    int energy = 0;
};

std::vector<RankedBase_t> RankBasesByEnergy_(const Faction& rFaction)
{
    std::vector<RankedBase_t> ranked;
    for (const BaseManager& rBase : rFaction.Bases())
    {
        ranked.push_back(RankedBase_t{&rBase, rBase.GetEnergyProduction()});
    }
    std::sort(ranked.begin(), ranked.end(), [](const RankedBase_t& a, const RankedBase_t& b) {
        if (a.energy != b.energy)
        {
            return a.energy > b.energy;
        }
        return a.pBase->GetBaseId() < b.pBase->GetBaseId();
    });
    return ranked;
}

} // namespace

std::unordered_map<BaseId_t, int> CommerceCalculator::ComputeForFaction(
    const Faction& rOwner, const GameState& rGameState) const
{
    const CommerceConfig_t* pConfig = rOwner.GetDataContext().commerceConfig.get();
    if (pConfig == nullptr)
    {
        throw std::runtime_error("CommerceCalculator: commerceConfig is null");
    }
    const CommerceConfig_t& rConfig = *pConfig;

    // TODO: zero commerce when sanctions are in effect against either faction.

    // Planet-wide tech points only (discovered techs' commerce_rating Adds). Owner numerator
    // uses full CommerceRating resolve (techs + Economy SE + faction bonuses).
    const int techDenominator = TotalTechCommerceRating_(rGameState) + 1;

    const std::vector<RankedBase_t> ownerBases = RankBasesByEnergy_(rOwner);
    std::unordered_map<BaseId_t, int> result;

    const DiplomacyLedger& rDiplomacy = rGameState.GetDiplomacyLedger();
    const FactionId_t ownerId = rOwner.GetFactionId();

    for (const Faction& rPartner : rGameState.Factions())
    {
        if (rPartner.GetFactionId() == ownerId)
        {
            continue;
        }

        const DiplomaticStatus_t status =
            rDiplomacy.GetStatus(ownerId, rPartner.GetFactionId());
        if (status != DiplomaticStatus_t::Friendship && status != DiplomaticStatus_t::Pact)
        {
            continue;
        }

        const std::vector<RankedBase_t> partnerBases = RankBasesByEnergy_(rPartner);
        const std::size_t pairCount = std::min(ownerBases.size(), partnerBases.size());
        for (std::size_t i = 0; i < pairCount; ++i)
        {
            const BaseManager& rOwnerBase = *ownerBases[i].pBase;
            const double combined =
                static_cast<double>(ownerBases[i].energy + partnerBases[i].energy);
            int value = static_cast<int>(std::ceil(combined * rConfig.pairMultiplier));

            value = FinalizeResolvedStat(ResolveFactionStat(
                rOwner.GetActiveEffects(), StatId_t::CommerceRate, static_cast<double>(value)));

            const int commerceTech = FinalizeResolvedStat(ResolveBaseStat(
                rOwnerBase.GetBaseEffects(), StatId_t::CommerceRating,
                SeedFor(StatId_t::CommerceRating)));
            value = (value * (commerceTech + 1)) / techDenominator;

            if (status == DiplomaticStatus_t::Friendship)
            {
                value = static_cast<int>(
                    std::floor(static_cast<double>(value) * rConfig.treatyMultiplier));
            }

            value += FinalizeResolvedStat(ResolveBaseStat(
                rOwnerBase.GetBaseEffects(), StatId_t::CommerceEnergyBonus,
                SeedFor(StatId_t::CommerceEnergyBonus)));

            if (value > 0)
            {
                result[rOwnerBase.GetBaseId()] += value;
            }
        }
    }

    return result;
}

} // namespace ac
