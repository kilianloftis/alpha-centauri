#pragma once

#include <vector>

namespace ac
{

struct DiplomacyConfig_t;

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

// Next stored friction and blemishes, clamped to diplomacy.json maxima, and the base value.
class DiplomacyCalculator
{
public:
    explicit DiplomacyCalculator(const DiplomacyConfig_t& rConfig);

    int NextFriction(int current, int delta) const;
    int NextBlemishes(int current, int delta, int floor) const;
    int BaseValue(const BaseValueInputs_t& rInputs) const;

private:
    const DiplomacyConfig_t& m_rConfig;
};

} // namespace ac
