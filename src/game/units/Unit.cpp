#include "game/units/Unit.h"

#include "game/map/Tile.h"
#include "lib/effects/ActiveEffect.h"

namespace ac
{

Unit::Unit(const UnitDesign& rDesign,
           Tile& rTile,
           BaseManager* pHomeBase,
           Faction& rFaction)
    : m_rDesign(rDesign)
    , m_pTile(&rTile)
    , m_pHomeBase(pHomeBase)
    , m_rFaction(rFaction)
    , m_currentHp(rDesign.GetHitPoints())
    , m_currentFuel(rDesign.GetFuel())
    , m_movesRemaining(rDesign.GetMovement())
    , m_xp(0)
{
}

const UnitDesign& Unit::GetDesign() const                              { return m_rDesign; }

int Unit::GetBaseCost() const                                          { return m_rDesign.GetBaseCost(); }
int Unit::GetAttack() const                                            { return m_rDesign.GetAttack(); }
int Unit::GetAttackAgainst(const Unit& rDefender) const
{
    EffectContext_t ctx;
    ctx.targetTile = &rDefender.GetTile();
    return m_rDesign.GetAttackAgainst(ctx);
}
int Unit::GetDefense() const                                           { return m_rDesign.GetDefense(); }
int Unit::GetMovement() const                                          { return m_rDesign.GetMovement(); }
int Unit::GetHitPoints() const                                         { return m_rDesign.GetHitPoints(); }
int Unit::GetDisengageChance() const                                   { return m_rDesign.GetDisengageChance(); }
int Unit::GetFuel() const                                              { return m_rDesign.GetFuel(); }
int Unit::GetDamageFromOutOfFuel() const                               { return m_rDesign.GetDamageFromOutOfFuel(); }
bool Unit::IsFlight() const                                            { return m_rDesign.IsFlight(); }
int Unit::GetCargoCapacity() const                                     { return m_rDesign.GetCargoCapacity(); }
int Unit::GetDifficultTerrainCost() const                              { return m_rDesign.GetDifficultTerrainCost(); }
bool Unit::IsSingleUse() const                                         { return m_rDesign.IsSingleUse(); }
Tile& Unit::GetTile() const                 { return *m_pTile; }
BaseManager* Unit::GetHomeBase() const      { return m_pHomeBase; }
Faction& Unit::GetFaction() const           { return m_rFaction; }

int Unit::GetCurrentHp() const              { return m_currentHp; }
int Unit::GetCurrentFuel() const            { return m_currentFuel; }
int Unit::GetMovesRemaining() const         { return m_movesRemaining; }
int Unit::GetXp() const                     { return m_xp; }

void Unit::SetCurrentHp(int hp)             { m_currentHp = hp; }
void Unit::SetCurrentFuel(int fuel)         { m_currentFuel = fuel; }
void Unit::SetMovesRemaining(int moves)     { m_movesRemaining = moves; }
void Unit::SetXp(int xp)                    { m_xp = xp; }
void Unit::SetTile(Tile& rTile)             { m_pTile = &rTile; }
void Unit::SetHomeBase(BaseManager* pHomeBase) { m_pHomeBase = pHomeBase; }

std::optional<UnitOrder_t>& Unit::GetOrder()             { return m_order; }
const std::optional<UnitOrder_t>& Unit::GetOrder() const { return m_order; }
void Unit::SetOrder(const UnitOrder_t& rOrder)            { m_order = rOrder; }
void Unit::ClearOrder()                                   { m_order.reset(); }

} // namespace ac
