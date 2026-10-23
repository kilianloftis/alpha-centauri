#pragma once

#include <vector>

namespace ac
{

class Unit;
class UnitPositionIndex;

// The carrier a unit rides and the passengers it carries. Embarked units share the carrier's
// tile. Outside a base they are ignored for targeting, ZOC and occupancy; in a base they may
// defend.
class UnitTransport
{
public:
    UnitTransport(Unit& rOwner, UnitPositionIndex& rPositions);
    UnitTransport(const UnitTransport&) = delete;
    UnitTransport& operator=(const UnitTransport&) = delete;

    bool IsEmbarked() const { return m_pCarrier != nullptr; }
    Unit* GetCarrier() const { return m_pCarrier; }
    const std::vector<Unit*>& GetCargo() const { return m_cargo; }

    // Throws when rCarrier cannot take this unit.
    void EmbarkInto(Unit& rCarrier);
    // Clear carrier link; passenger remains on its current tile.
    void Disembark();
    // Drops both links before the unit leaves the world; its passengers become independent
    // occupants of the same tile.
    void ClearLinks();

private:
    Unit& m_rOwner;
    UnitPositionIndex& m_rPositions;
    Unit* m_pCarrier = nullptr;
    std::vector<Unit*> m_cargo;
};

} // namespace ac
