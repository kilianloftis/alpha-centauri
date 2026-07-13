#include "game/units/Unit.h"

#include "game/Faction.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/effects/ActiveEffect.h"
#include "game/units/MovementConstants.h"

#include <algorithm>

namespace ac
{

Unit::Unit(UnitId_t unitId,
           const UnitDesign& rDesign,
           UnitPositionIndex& rPositions,
           const Tile& rTile,
           BaseManager* pHomeBase,
           Faction& rFaction)
    : m_unitId(unitId)
    , m_rDesign(rDesign)
    , m_rPositions(rPositions)
    , m_pTile(&rTile)
    , m_pHomeBase(pHomeBase)
    , m_rFaction(rFaction)
    // Seed current stats from the *live* resolved maxima (design effects + the faction's
    // active FactionUnits effects), not the design's context-free values. A fresh unit
    // therefore starts at its true max, so current-vs-max is well defined from spawn.
    , m_currentHp(0)
    , m_currentFuel(0)
    , m_moveFragmentsRemaining(0)
    , m_xp(0)
{
    // Members used by ResolveStat are initialised above; seed after the mem-init list.
    m_currentHp = ResolveStat(*this, StatId_t::HitPoints);
    m_currentFuel = ResolveStat(*this, StatId_t::Fuel);
    m_moveFragmentsRemaining =
        GetMovementPoints() * MovementConstants_t::k_moveFragmentsPerPoint;

    // Throws if the stacking rule forbids rTile; the destructor is not run for a unit
    // whose constructor throws, so no unregistration is needed on that path.
    m_rPositions.Register_(*this, rTile);
}

Unit::~Unit()
{
    m_rPositions.Unregister_(*this);
}

UnitId_t Unit::GetUnitId() const { return m_unitId; }

const UnitDesign& Unit::GetDesign() const { return m_rDesign; }

int Unit::GetStat(StatId_t statId) const
{
    return ResolveStat(*this, statId);
}

int Unit::GetStat(StatId_t statId, const EffectContext_t& rCtx) const
{
    return ResolveStat(*this, statId, rCtx);
}

bool Unit::GetFlag(RuleFlagId_t flagId) const
{
    return ResolveFlag(*this, flagId);
}

UnitDomain_t Unit::GetDomain() const
{
    return m_rDesign.GetDomain();
}

const Tile& Unit::GetTile() const                                      { return *m_pTile; }
BaseManager* Unit::GetHomeBase() const      { return m_pHomeBase; }
Faction& Unit::GetFaction() const           { return m_rFaction; }

int Unit::GetCurrentHp() const              { return m_currentHp; }
int Unit::GetCurrentFuel() const            { return m_currentFuel; }
int Unit::GetMovementPoints() const         { return ResolveStat(*this, StatId_t::Movement); }
int Unit::GetMoveFragmentsRemaining() const { return m_moveFragmentsRemaining; }
int Unit::GetXp() const                     { return m_xp; }

// Current stats are clamped to [0, live max] so the invariant 0 <= current <= max holds
// regardless of caller arithmetic (overkill damage, refuel past capacity, ...). The maxima
// are the live resolved values, so they track faction-effect changes made after spawn.
void Unit::SetCurrentHp(int hp)
{
    m_currentHp = std::clamp(hp, 0, ResolveStat(*this, StatId_t::HitPoints));
}
void Unit::SetCurrentFuel(int fuel)
{
    m_currentFuel = std::clamp(fuel, 0, ResolveStat(*this, StatId_t::Fuel));
}
void Unit::SetMoveFragmentsRemaining(int fragments)
{
    const int maxFragments =
        GetMovementPoints() * MovementConstants_t::k_moveFragmentsPerPoint;
    m_moveFragmentsRemaining = std::clamp(fragments, 0, maxFragments);
}
void Unit::SetXp(int xp)                    { m_xp = std::max(xp, 0); }
void Unit::SetHomeBase(BaseManager* pHomeBase) { m_pHomeBase = pHomeBase; }

std::optional<UnitOrder_t>& Unit::GetOrder()             { return m_order; }
const std::optional<UnitOrder_t>& Unit::GetOrder() const { return m_order; }
void Unit::SetOrder(const UnitOrder_t& rOrder)            { m_order = rOrder; }
void Unit::ClearOrder()                                   { m_order.reset(); }

} // namespace ac
