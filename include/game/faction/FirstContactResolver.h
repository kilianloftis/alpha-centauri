#pragma once

#include <memory>
#include <vector>

namespace ac
{

class DiplomacyLedger;
class Faction;
class Unit;

// World-scoped first-contact scans. Owned by GameState; hooked from visibility rebuild,
// unit create/move, and base-list changes.
class FirstContactResolver
{
public:
    FirstContactResolver(DiplomacyLedger& rLedger,
                         std::vector<std::unique_ptr<Faction>>& rFactions);

    // Establish mutual Known when a foreign unit or base sits on rObserver's visible map.
    void ConsiderObserver(Faction& rObserver);

    // Establish mutual Known when any other faction's visible map covers rSubject's tile.
    void ConsiderUnit(const Unit& rSubject);

private:
    DiplomacyLedger& m_rLedger;
    std::vector<std::unique_ptr<Faction>>& m_rFactions;
};

} // namespace ac
