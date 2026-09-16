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

int CommercePairValue_(const Faction& rBeneficiary,
                       const BaseManager& rBeneficiaryBase,
                       int beneficiaryEnergy,
                       int partnerEnergy,
                       DiplomaticStatus_t status,
                       const CommerceConfig_t& rConfig,
                       int techDenominator)
{
    const double combined = static_cast<double>(beneficiaryEnergy + partnerEnergy);
    int value = static_cast<int>(std::ceil(combined * rConfig.pairMultiplier));

    value = FinalizeResolvedStat(ResolveFactionStat(
        rBeneficiary.GetActiveEffects(), StatId_t::CommerceRate, static_cast<double>(value)));

    const int commerceTech = FinalizeResolvedStat(ResolveBaseStat(
        rBeneficiaryBase.GetBaseEffects(), StatId_t::CommerceRating,
        SeedFor(StatId_t::CommerceRating)));
    value = (value * (commerceTech + 1)) / techDenominator;

    if (status == DiplomaticStatus_t::Friendship)
    {
        value = static_cast<int>(std::floor(static_cast<double>(value) * rConfig.treatyMultiplier));
    }

    value += FinalizeResolvedStat(ResolveBaseStat(
        rBeneficiaryBase.GetBaseEffects(), StatId_t::CommerceEnergyBonus,
        SeedFor(StatId_t::CommerceEnergyBonus)));

    return value;
}

} // namespace

std::unordered_map<BaseId_t, int> CommerceCalculator::ComputeForFaction(
    const Faction& rOwner, const GameState& rGameState) const
{
    std::unordered_map<BaseId_t, int> result;
    for (const BaseManager& rBase : rOwner.Bases())
    {
        for (const CommercePartnerLine_t& rLine : ComputeForBase(rBase, rGameState))
        {
            if (rLine.ourEnergy > 0)
            {
                result[rBase.GetBaseId()] += rLine.ourEnergy;
            }
        }
    }
    return result;
}

std::vector<CommercePartnerLine_t> CommerceCalculator::ComputeForBase(
    const BaseManager& rBase, const GameState& rGameState) const
{
    const Faction& rOwner = rBase.GetFaction();
    const CommerceConfig_t* pConfig = rOwner.GetDataContext().commerceConfig.get();
    if (pConfig == nullptr)
    {
        throw std::runtime_error("CommerceCalculator: commerceConfig is null");
    }
    const CommerceConfig_t& rConfig = *pConfig;

    // TODO: zero commerce when sanctions are in effect against either faction.

    const int techDenominator = TotalTechCommerceRating_(rGameState) + 1;
    const std::vector<RankedBase_t> ownerBases = RankBasesByEnergy_(rOwner);

    std::size_t ownerIndex = ownerBases.size();
    for (std::size_t i = 0; i < ownerBases.size(); ++i)
    {
        if (ownerBases[i].pBase == &rBase)
        {
            ownerIndex = i;
            break;
        }
    }
    if (ownerIndex >= ownerBases.size())
    {
        throw std::runtime_error("CommerceCalculator: base is not owned by its faction");
    }

    std::vector<CommercePartnerLine_t> lines;
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
        if (ownerIndex >= partnerBases.size())
        {
            continue;
        }

        const RankedBase_t& rOwnerRank = ownerBases[ownerIndex];
        const RankedBase_t& rPartnerRank = partnerBases[ownerIndex];

        CommercePartnerLine_t line;
        line.pPartner = &rPartner;
        line.status = status;
        line.ourEnergy = CommercePairValue_(
            rOwner, rBase, rOwnerRank.energy, rPartnerRank.energy, status, rConfig,
            techDenominator);
        line.theirEnergy = CommercePairValue_(
            rPartner, *rPartnerRank.pBase, rPartnerRank.energy, rOwnerRank.energy, status, rConfig,
            techDenominator);
        lines.push_back(line);
    }

    return lines;
}

} // namespace ac
