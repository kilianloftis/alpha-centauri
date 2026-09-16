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
            // The denominator is planet-wide, so one gated or non-literal entry here would
            // quietly lower commerce for every faction. Count only what resolves the same
            // way for everyone: no runtime condition, no filter, no amount source, no
            // per-tile selector, and not already retired by a later tech.
            if (rEffect.condition.has_value() || rEffect.buildingFilter.has_value()
                || rEffect.unitFilter.has_value() || rEffect.factionFilter.has_value()
                || pStat->amountSource.has_value() || pStat->selector.has_value())
            {
                continue;
            }
            if (!rEffect.removedByTech.empty()
                && rFaction.GetResearch().HasDiscoveredTech(rEffect.removedByTech))
            {
                continue;
            }
            total += FinalizeResolvedStat(pStat->amount);
        }
    }
    return total;
}

// The +1 is the SMAC baseline (a planet with no economic techs still trades). Clamped
// because it divides: a mod with negative commerce_rating Adds could otherwise sum to -1
// and divide by zero, or below that and invert the ratio.
int TechDenominator_(const GameState& rGameState)
{
    int total = 0;
    for (const Faction& rFaction : rGameState.Factions())
    {
        total += TechCommerceRatingContribution_(rFaction);
    }
    return std::max(1, total + 1);
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

std::unordered_map<BaseId_t, std::vector<CommercePartnerLine_t>>
CommerceCalculator::ComputeAllLines(const Faction& rOwner, const GameState& rGameState) const
{
    const CommerceConfig_t* pConfig = rOwner.GetDataContext().commerceConfig.get();
    if (pConfig == nullptr)
    {
        throw std::runtime_error("CommerceCalculator: commerceConfig is null");
    }
    const CommerceConfig_t& rConfig = *pConfig;

    // TODO: zero commerce when sanctions are in effect against either faction.

    const int techDenominator = TechDenominator_(rGameState);
    // Ranking a faction prices every one of its bases (ComputeWorked_ per base), so each
    // side is ranked once here and reused for every pair rather than per owner base.
    const std::vector<RankedBase_t> ownerBases = RankBasesByEnergy_(rOwner);

    std::unordered_map<BaseId_t, std::vector<CommercePartnerLine_t>> lines;
    const DiplomacyLedger& rDiplomacy = rGameState.GetDiplomacyLedger();
    const FactionId_t ownerId = rOwner.GetFactionId();

    for (const Faction& rPartner : rGameState.Factions())
    {
        if (rPartner.GetFactionId() == ownerId)
        {
            continue;
        }

        const DiplomaticStatus_t status = rDiplomacy.GetStatus(ownerId, rPartner.GetFactionId());
        if (status != DiplomaticStatus_t::Friendship && status != DiplomaticStatus_t::Pact)
        {
            continue;
        }

        const std::vector<RankedBase_t> partnerBases = RankBasesByEnergy_(rPartner);
        // Top-to-top by rank; whichever side has more bases leaves its tail unpaired.
        const std::size_t pairCount = std::min(ownerBases.size(), partnerBases.size());
        for (std::size_t i = 0; i < pairCount; ++i)
        {
            const RankedBase_t& rOurs = ownerBases[i];
            const RankedBase_t& rTheirs = partnerBases[i];

            CommercePartnerLine_t line;
            line.pPartner = &rPartner;
            line.status = status;
            line.ourEnergy = CommercePairValue_(rOwner, *rOurs.pBase, rOurs.energy,
                                                rTheirs.energy, status, rConfig, techDenominator);
            line.theirEnergy = CommercePairValue_(rPartner, *rTheirs.pBase, rTheirs.energy,
                                                  rOurs.energy, status, rConfig, techDenominator);
            lines[rOurs.pBase->GetBaseId()].push_back(line);
        }
    }

    return lines;
}

std::unordered_map<BaseId_t, int> CommerceCalculator::ComputeForFaction(
    const Faction& rOwner, const GameState& rGameState) const
{
    std::unordered_map<BaseId_t, int> result;
    for (const auto& [baseId, rLines] : ComputeAllLines(rOwner, rGameState))
    {
        int total = 0;
        for (const CommercePartnerLine_t& rLine : rLines)
        {
            if (rLine.ourEnergy > 0)
            {
                total += rLine.ourEnergy;
            }
        }
        if (total > 0)
        {
            result[baseId] = total;
        }
    }
    return result;
}

std::vector<CommercePartnerLine_t> CommerceCalculator::ComputeForBase(
    const BaseManager& rBase, const GameState& rGameState) const
{
    const std::unordered_map<BaseId_t, std::vector<CommercePartnerLine_t>> lines =
        ComputeAllLines(rBase.GetFaction(), rGameState);
    const auto it = lines.find(rBase.GetBaseId());
    if (it == lines.end())
    {
        return {};
    }
    return it->second;
}

} // namespace ac
