#pragma once

#include "game/diplomacy/GiftSize.h"

#include <vector>

namespace ac
{

struct AITendenciesConfig;
struct DiplomacyConfig_t;
struct TechAiWeights_t;

// What BaseValueRules gathers for value_of_base (docs/thinker/terranx-map.md "Base value").
struct BaseValueInputs_t
{
    // Headquarters distance after the neighbour scan.
    int distance = 0;
    // The larger of the two treasuries.
    int credits = 0;
    int population = 0;
    bool bGarrisoned = false;
    int evaluatorRegionBases = 0;
    int counterpartyRegionBases = 0;
    // Built facilities in registry order; the k-th weighs k times.
    std::vector<int> facilityMinerals;
    int projectMinerals = 0;
    int evaluatorUnitMinerals = 0;
};

// What a credit-tested gift's size reads, credits taken before the gift moves.
struct GiftSizeInputs_t
{
    int value = 0;
    int recipientCredits = 0;
    int giverCredits = 0;
    int turn = 0;
    // The giver's GiftSizeThreshold.
    double thresholdScale = 1.0;
};

// Next stored friction and blemishes, clamped to diplomacy.json maxima, the base value, and
// make_gift's sizes, friction and gift count.
class DiplomacyCalculator
{
public:
    explicit DiplomacyCalculator(const DiplomacyConfig_t& rConfig);

    int NextFriction(int current, int delta) const;
    int NextBlemishes(int current, int delta, int floor) const;
    int BaseValue(const BaseValueInputs_t& rInputs) const;

    // Each gift kind's size ladder and the recipient's friction change toward the giver (<= 0).
    GiftSize_t CreditGiftSize(const GiftSizeInputs_t& rInputs) const;
    int CreditGiftFriction(int value) const;
    GiftSize_t TechGiftSize(const GiftSizeInputs_t& rInputs) const;
    int TechGiftFriction(int value) const;
    GiftSize_t BaseGiftSize(const GiftSizeInputs_t& rInputs) const;
    int BaseGiftFriction(int value) const;
    // tech_val's weighted sum, scaled down unless the tech matches a priority.
    int TechWeightValue(const TechAiWeights_t& rWeights, const AITendenciesConfig& rPriorities,
                        int vendettaCount, int baseCount) const;
    // tech_val's adjustment for an AI that lacks the tech.
    int UnownedTechValue(int value, int owners, int techLevel, int highestLevel) const;
    // The giver's count toward the recipient after a gift, capped at the giver's blemishes.
    int NextGifts(int current, int giverBlemishes, GiftSize_t size) const;
    int AlliedTerritoryGifts(int current) const;

private:
    const DiplomacyConfig_t& m_rConfig;
};

} // namespace ac
