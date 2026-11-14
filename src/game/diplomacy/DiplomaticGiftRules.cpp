#include "game/diplomacy/DiplomaticGiftRules.h"

#include "game/CurrentGameState.h"
#include "game/GameDataContext.h"
#include "game/GameState.h"
#include "game/base/BaseManager.h"
#include "game/diplomacy/BaseValueRules.h"
#include "game/diplomacy/DiplomacyCalculator.h"
#include "game/diplomacy/DiplomacyLedger.h"
#include "game/diplomacy/DiplomaticPermissionRules.h"
#include "game/economy/EconomyManager.h"
#include "game/effects/ActiveEffect.h"
#include "game/faction/Faction.h"
#include "game/research/ResearchManager.h"
#include "game/research/ResearchRules.h"
#include "game/research/TechRegistry.h"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <variant>

namespace ac
{

namespace
{

double ResolveGiftStat_(const Faction& rFaction, const Faction& rPartner, TradeKind_t kind,
                        StatId_t stat, double seed)
{
    EffectContext_t ctx;
    ctx.pPartnerFaction = &rPartner;
    ctx.giftKind = kind;
    return ResolveFactionStat(rFaction.GetActiveEffects(), stat, seed, &ctx);
}

int VendettaCount_(const Faction& rFaction)
{
    const GameState& rGame = CurrentGameState();
    const DiplomacyLedger& rLedger = rGame.GetDiplomacyLedger();
    const auto atVendetta = [&](const Faction& rOther)
    {
        return &rOther != &rFaction && HasDiplomacy(rOther)
            && rLedger.HasVendetta(rFaction.GetFactionId(), rOther.GetFactionId());
    };
    const auto count = std::ranges::count_if(rGame.Factions(), atVendetta);
    return static_cast<int>(count);
}

int TechOwners_(const TechId& rTechId)
{
    const auto owns = [&rTechId](const Faction& rFaction)
    { return HasDiplomacy(rFaction) && rFaction.GetResearch().HasDiscoveredTech(rTechId); };
    const auto count = std::ranges::count_if(CurrentGameState().Factions(), owns);
    return static_cast<int>(count);
}

// tech_val(tech, recipient, 1): the recipient's priorities, then less for an AI that lacks it.
int TechValue_(const Faction& rRecipient, const TechId& rTechId)
{
    const GameDataContext& rData = CurrentGameState().GetGameData();
    const DiplomacyCalculator& rCalculator = *rData.diplomacyCalculator;
    const TechConfig_t& rTech = rData.techRegistry->Get(rTechId);
    const int baseCount = static_cast<int>(rRecipient.GetBaseCount());
    const int value = rCalculator.TechWeightValue(rTech.aiWeights, rRecipient.GetDefinition().ai,
                                                  VendettaCount_(rRecipient), baseCount);
    const ResearchManager& rResearch = rRecipient.GetResearch();
    if (rRecipient.IsPlayerControlled() || rResearch.HasDiscoveredTech(rTechId))
    {
        return value;
    }
    return rCalculator.UnownedTechValue(value, TechOwners_(rTechId), TechLevel(rTechId),
                                        HighestTechLevel(rResearch));
}

// One make_gift branch per item.
class GiftStandingOf_
{
public:
    GiftStandingOf_(const Faction& rGiver, const Faction& rRecipient)
        : m_rGiver(rGiver)
        , m_rRecipient(rRecipient)
        , m_rCalculator(*CurrentGameState().GetGameData().diplomacyCalculator)
    {
    }

    std::optional<GiftStanding_t> operator()(const TradeCredits_t& rCredits) const
    {
        const int value = Weighted_(TradeKind_t::Credits, rCredits.amount);
        const GiftSizeInputs_t inputs = SizeInputs_(TradeKind_t::Credits, value);
        return Standing_(m_rCalculator.CreditGiftFriction(value),
                         m_rCalculator.CreditGiftSize(inputs));
    }

    std::optional<GiftStanding_t> operator()(const TradeTechnology_t& rTechnology) const
    {
        const int rawValue = TechValue_(m_rRecipient, rTechnology.techId);
        const int value = Weighted_(TradeKind_t::Technology, rawValue);
        const GiftSizeInputs_t inputs = SizeInputs_(TradeKind_t::Technology, value);
        return Standing_(m_rCalculator.TechGiftFriction(value),
                         m_rCalculator.TechGiftSize(inputs));
    }

    std::optional<GiftStanding_t> operator()(const TradeBase_t& rBase) const
    {
        const BaseManager& rGiven = RequireGiverBase_(rBase.baseId);
        const int rawValue = BaseValue(rGiven, m_rRecipient, m_rGiver);
        const int value = Weighted_(TradeKind_t::Base, rawValue);
        const GiftSizeInputs_t inputs = SizeInputs_(TradeKind_t::Base, value);
        return Standing_(m_rCalculator.BaseGiftFriction(value),
                         m_rCalculator.BaseGiftSize(inputs));
    }

    std::optional<GiftStanding_t> operator()(const TradeCommFrequency_t&) const
    {
        return std::nullopt;
    }

    std::optional<GiftStanding_t> operator()(const TradeWorldMap_t&) const
    {
        return std::nullopt;
    }

    std::optional<GiftStanding_t> operator()(const TradeDeclareVendetta_t&) const
    {
        return std::nullopt;
    }

private:
    // The giver's weight, then the recipient's, each truncated.
    int Weighted_(TradeKind_t kind, int rawValue) const
    {
        const double given = ResolveGiftStat_(m_rGiver, m_rRecipient, kind,
                                              StatId_t::GiftGivenValue, rawValue);
        const int givenValue = static_cast<int>(given);
        const double received = ResolveGiftStat_(m_rRecipient, m_rGiver, kind,
                                                 StatId_t::GiftReceivedValue, givenValue);
        return static_cast<int>(received);
    }

    double ThresholdScale_(TradeKind_t kind) const
    {
        const double seed = SeedFor(StatId_t::GiftSizeThreshold);
        return ResolveGiftStat_(m_rGiver, m_rRecipient, kind, StatId_t::GiftSizeThreshold, seed);
    }

    GiftSizeInputs_t SizeInputs_(TradeKind_t kind, int value) const
    {
        GiftSizeInputs_t inputs;
        inputs.value = value;
        inputs.recipientCredits = m_rRecipient.GetEconomy().GetEnergy();
        inputs.giverCredits = m_rGiver.GetEconomy().GetEnergy();
        inputs.turn = CurrentGameState().GetTurnNumber();
        inputs.thresholdScale = ThresholdScale_(kind);
        return inputs;
    }

    GiftStanding_t Standing_(int frictionDelta, GiftSize_t size) const
    {
        const DiplomacyLedger& rLedger = CurrentGameState().GetDiplomacyLedger();
        const FactionId_t giver = m_rGiver.GetFactionId();
        const int current = rLedger.GetGifts(giver, m_rRecipient.GetFactionId());
        const int blemishes = rLedger.GetBlemishes(giver);
        return GiftStanding_t{frictionDelta, m_rCalculator.NextGifts(current, blemishes, size)};
    }

    const BaseManager& RequireGiverBase_(BaseId_t baseId) const
    {
        const BaseManager* pBase = m_rGiver.FindBase(baseId);
        if (!pBase)
        {
            throw std::logic_error("GiftStanding: base " + std::to_string(baseId)
                                   + " is not the giver's");
        }
        return *pBase;
    }

    const Faction& m_rGiver;
    const Faction& m_rRecipient;
    const DiplomacyCalculator& m_rCalculator;
};

} // namespace

bool IsGift(const DiplomaticProposal_t& rProposal)
{
    return !rProposal.requestedStatus && rProposal.demand.empty() && !rProposal.give.empty();
}

std::optional<GiftStanding_t> GiftStanding(FactionId_t giver, FactionId_t recipient,
                                           const TradeItem_t& rItem)
{
    const GameState& rGame = CurrentGameState();
    const GiftStandingOf_ standingOf(rGame.RequireFaction(giver), rGame.RequireFaction(recipient));
    return std::visit(standingOf, rItem);
}

} // namespace ac
