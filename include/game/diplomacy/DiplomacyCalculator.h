#pragma once

namespace ac
{

struct DiplomacyConfig_t;

// Next stored friction and blemishes, clamped to diplomacy.json maxima.
class DiplomacyCalculator
{
public:
    explicit DiplomacyCalculator(const DiplomacyConfig_t& rConfig);

    int NextFriction(int current, int delta) const;
    int NextBlemishes(int current, int delta, int floor) const;

private:
    const DiplomacyConfig_t& m_rConfig;
};

} // namespace ac
