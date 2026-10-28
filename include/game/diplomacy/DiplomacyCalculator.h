#pragma once

namespace ac
{

struct DiplomacyConfig_t;

// Clamps pairwise friction to diplomacy.json friction_max. Writers call this before
// storing a total.
class DiplomacyCalculator
{
public:
    explicit DiplomacyCalculator(const DiplomacyConfig_t& rConfig);

    int ClampFriction(int friction) const;

private:
    const DiplomacyConfig_t& m_rConfig;
};

} // namespace ac
