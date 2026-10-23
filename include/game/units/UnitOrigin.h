#pragma once

namespace ac
{

class BaseManager;

// Where a new unit comes from: its home base, the base whose production grants it carries,
// and whether it can be a prototype.
class UnitOrigin
{
public:
    // Built by rBase's production: homed there, carries its grants, may be a prototype.
    static UnitOrigin Built(BaseManager& rBase) { return UnitOrigin(&rBase, &rBase, true); }
    // A starting unit: homed at rHome and carries its grants, but is never a prototype.
    static UnitOrigin Issued(BaseManager& rHome) { return UnitOrigin(&rHome, &rHome, false); }
    // Granted or spawned from nowhere: no production grants. pHome may be null.
    static UnitOrigin Gift(BaseManager* pHome) { return UnitOrigin(pHome, nullptr, false); }

    BaseManager* HomeBase() const { return m_pHome; }
    // Null for a gift.
    BaseManager* GrantsFrom() const { return m_pGrantsFrom; }
    bool MayBePrototype() const { return m_bMayBePrototype; }

private:
    UnitOrigin(BaseManager* pHome, BaseManager* pGrantsFrom, bool bMayBePrototype)
        : m_pHome(pHome)
        , m_pGrantsFrom(pGrantsFrom)
        , m_bMayBePrototype(bMayBePrototype)
    {
    }

    BaseManager* m_pHome;
    BaseManager* m_pGrantsFrom;
    bool m_bMayBePrototype;
};

} // namespace ac
