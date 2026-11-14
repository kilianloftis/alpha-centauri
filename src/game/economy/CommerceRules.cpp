#include "game/economy/CommerceRules.h"

#include "game/CurrentGameState.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/atrocities/AtrocityLedger.h"
#include "game/base/BaseManager.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/economy/CommerceCalculator.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectConfig.h"
#include "game/effects/EffectEnums.h"
#include "game/faction/Faction.h"
#include "game/research/ResearchManager.h"
#include "game/research/TechConfigParser.h"
#include "game/research/TechRegistry.h"

#include <algorithm>
#include <cstddef>
#include <unordered_map>
#include <variant>
#include <vector>

namespace ac
{

namespace
{

// The denominator is planet-wide, so only a plain, unconditional commerce_rating Add the
// faction still holds counts. Null for anything else.
const StatModifierEffect_t* PlanetWideCommerceAdd_(const EffectConfig_t& rEffect,
                                                   const Faction& rFaction)
{
    const auto* pStat = std::get_if<StatModifierEffect_t>(&rEffect.effect);
    if (pStat == nullptr || pStat->stat != StatId_t::CommerceRating
        || pStat->op != ModifierOp_t::Add)
    {
        return nullptr;
    }
    if (rEffect.condition.has_value() || rEffect.buildingFilter.has_value()
        || rEffect.factionFilter.has_value()
        || pStat->amountSource.has_value() || pStat->selector.has_value())
    {
        return nullptr;
    }
    if (!rEffect.removedByTech.empty()
        && rFaction.GetResearch().HasDiscoveredTech(rEffect.removedByTech))
    {
        return nullptr;
    }
    return pStat;
}

// Sum of commerce_rating Add amounts on a faction's discovered techs only (no SE / faction
// bonuses). Used for totalCommerceTech across the planet.
int TechCommerceRatingContribution_(const Faction& rFaction, const TechRegistry& rTechs)
{
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
            if (const StatModifierEffect_t* pStat = PlanetWideCommerceAdd_(rEffect, rFaction))
            {
                total += FinalizeResolvedStat(pStat->amount);
            }
        }
    }
    return total;
}

// The SMAC +1 baseline, clamped since it divides: negative mod Adds could reach zero or below.
int TechDenominator_(const GameState& rGameState)
{
    const TechRegistry& rTechs = *rGameState.GetGameData().techRegistry;
    int total = 0;
    for (const Faction& rFaction : rGameState.Factions())
    {
        total += TechCommerceRatingContribution_(rFaction, rTechs);
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

// CommerceRate as rFaction resolves it toward rPartner: its own effects plus the FactionPair
// effects of their diplomatic status.
double CommerceRateToward_(const Faction& rFaction, const Faction& rPartner, double seed)
{
    EffectContext_t ctx;
    ctx.pFaction = &rFaction;
    ctx.pPartnerFaction = &rPartner;
    return ResolveFactionStat(rFaction.GetActiveEffects(), StatId_t::CommerceRate, seed, &ctx);
}

int CommercePairValue_(const Faction& rBeneficiary,
                       const BaseManager& rBeneficiaryBase,
                       const Faction& rPartner,
                       int beneficiaryEnergy,
                       int partnerEnergy,
                       const CommerceCalculator& rCalculator,
                       int techDenominator)
{
    const double commerceRating = ResolveBaseStat(
        rBeneficiaryBase.GetBaseEffects(), StatId_t::CommerceRating,
        SeedFor(StatId_t::CommerceRating), nullptr);

    CommercePairInputs_t inputs;
    inputs.energyOurs = beneficiaryEnergy;
    inputs.energyTheirs = partnerEnergy;
    inputs.commerceTech = FinalizeResolvedStat(commerceRating);
    inputs.techDenominator = techDenominator;
    const int formulaValue = rCalculator.PairValue(inputs);

    // Same seam as scrap: formula first, then PureMultiplier effects scale the result
    // (Global Trade Pact AddPercent 100 → ×2, a Treaty's MultiplyGeometric 0.5). Flat bonus last.
    const double rated =
        CommerceRateToward_(rBeneficiary, rPartner, static_cast<double>(formulaValue));
    const double bonus = ResolveBaseStat(
        rBeneficiaryBase.GetBaseEffects(), StatId_t::CommerceEnergyBonus,
        SeedFor(StatId_t::CommerceEnergyBonus), nullptr);
    return FinalizeResolvedStat(rated) + FinalizeResolvedStat(bonus);
}

bool TradesWith_(const Faction& rOwner, const Faction& rPartner, const GameState& rGameState)
{
    if (rPartner.GetFactionId() == rOwner.GetFactionId())
    {
        return false;
    }
    // A rate of zero (the diplomatic status's own factor) means the pair does not trade.
    if (CommerceRateToward_(rOwner, rPartner, 1.0) <= 0.0)
    {
        return false;
    }
    const AtrocityLedger& rAtrocities = rGameState.GetAtrocityLedger();
    return !rAtrocities.IsSanctioned(rPartner.GetFactionId(), rGameState.GetMissionYear());
}

} // namespace

std::unordered_map<BaseId_t, std::vector<CommercePartnerLine_t>> ComputeCommerceLines(
    const Faction& rOwner)
{
    const GameState& rGameState = CurrentGameState();
    const CommerceCalculator& rCalculator = *rGameState.GetGameData().commerceCalculator;

    const AtrocityLedger& rAtrocities = rGameState.GetAtrocityLedger();
    // Step 10: sanctions zero the pair, applied before pairing so the partner drops off the base
    // screen.
    if (rAtrocities.IsSanctioned(rOwner.GetFactionId(), rGameState.GetMissionYear()))
    {
        return {};
    }

    const int techDenominator = TechDenominator_(rGameState);
    // Ranking a faction prices every one of its bases (WorkedYield per base), so each
    // side is ranked once here and reused for every pair rather than per owner base.
    const std::vector<RankedBase_t> ownerBases = RankBasesByEnergy_(rOwner);

    std::unordered_map<BaseId_t, std::vector<CommercePartnerLine_t>> lines;
    const DiplomacyLedger& rDiplomacy = rGameState.GetDiplomacyLedger();
    const FactionId_t ownerId = rOwner.GetFactionId();

    for (const Faction& rPartner : rGameState.Factions())
    {
        if (!TradesWith_(rOwner, rPartner, rGameState))
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
            line.pPartnerFaction = &rPartner;
            line.status = rDiplomacy.GetStatus(ownerId, rPartner.GetFactionId());
            line.ourEnergy = CommercePairValue_(rOwner, *rOurs.pBase, rPartner, rOurs.energy,
                                                rTheirs.energy, rCalculator, techDenominator);
            line.theirEnergy = CommercePairValue_(rPartner, *rTheirs.pBase, rOwner, rTheirs.energy,
                                                  rOurs.energy, rCalculator, techDenominator);
            lines[rOurs.pBase->GetBaseId()].push_back(line);
        }
    }

    return lines;
}

std::unordered_map<BaseId_t, int> ComputeCommerceForFaction(const Faction& rOwner)
{
    const std::unordered_map<BaseId_t, std::vector<CommercePartnerLine_t>> lines =
        ComputeCommerceLines(rOwner);
    std::unordered_map<BaseId_t, int> result;
    for (const auto& [baseId, rLines] : lines)
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

std::vector<CommercePartnerLine_t> ComputeCommerceForBase(const BaseManager& rBase)
{
    const std::unordered_map<BaseId_t, std::vector<CommercePartnerLine_t>> lines =
        ComputeCommerceLines(rBase.GetFaction());
    const auto it = lines.find(rBase.GetBaseId());
    if (it == lines.end())
    {
        return {};
    }
    return it->second;
}

} // namespace ac
