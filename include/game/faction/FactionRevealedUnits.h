#pragma once

#include "game/units/Unit.h"
#include "lib/Revision.h"

#include <unordered_set>

namespace ac
{

// Per-faction contact memory: concealed enemy units this faction has bumped into
// (occupied tile or ZOC block). Lives on Faction (not Unit), alongside
// FactionExploredMap / FactionVisibleMap. Tracked by stable UnitId_t so entries
// survive address reuse; Forget on destroy keeps the set bounded.
class FactionRevealedUnits
{
public:
    FactionRevealedUnits() = default;

    void Clear()
    {
        if (!m_unitIds.empty())
        {
            m_unitIds.clear();
            m_revision.Bump();
        }
    }

    void Reveal(UnitId_t unitId)
    {
        if (m_unitIds.insert(unitId).second)
        {
            m_revision.Bump();
        }
    }

    void Reveal(const Unit& rUnit) { Reveal(rUnit.GetUnitId()); }

    void Forget(UnitId_t unitId)
    {
        if (m_unitIds.erase(unitId) > 0)
        {
            m_revision.Bump();
        }
    }

    void Forget(const Unit& rUnit) { Forget(rUnit.GetUnitId()); }

    bool IsRevealed(UnitId_t unitId) const { return m_unitIds.contains(unitId); }
    bool IsRevealed(const Unit& rUnit) const { return IsRevealed(rUnit.GetUnitId()); }

    uint64_t GetRevision() const { return m_revision.Get(); }

private:
    std::unordered_set<UnitId_t> m_unitIds;
    Revision m_revision;
};

} // namespace ac
