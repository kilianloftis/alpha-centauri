#include "game/units/Unit.h"

#include "game/Faction.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/effects/ActiveEffect.h"

#include <algorithm>

namespace ac
{

namespace
{

// A live unit's full effect list: its design's own component effects (ThisUnit lane)
// plus the owning faction's FactionUnits-scoped effects.
std::vector<ActiveEffect_t> CollectLiveUnitEffects_(const UnitDesign& rDesign, const Faction& rFaction)
{
    std::vector<ActiveEffect_t> effects = rDesign.CollectEffects();
    // Lazy view, not a copy: the source is Faction's cached pool (long-lived), and this is
    // its only use, right below.
    auto factionEffects =
        FilterByScope(rFaction.GetActiveEffects().effects, EffectScope_t::FactionUnits);
    effects.insert(effects.end(), factionEffects.begin(), factionEffects.end());
    return effects;
}

} // namespace

Unit::Unit(const UnitDesign& rDesign,
           UnitPositionIndex& rPositions,
           const Tile& rTile,
           BaseManager* pHomeBase,
           Faction& rFaction)
    : m_rDesign(rDesign)
    , m_rPositions(rPositions)
    , m_pTile(&rTile)
    , m_pHomeBase(pHomeBase)
    , m_rFaction(rFaction)
    // Seed current stats from the *live* resolved maxima (design effects + the faction's
    // active FactionUnits effects), not the design's context-free values: GetHitPoints()
    // and friends read only m_rDesign / m_rFaction, both already initialised above. A fresh
    // unit therefore starts at its true max, so current-vs-max is well defined from spawn.
    , m_currentHp(GetHitPoints())
    , m_currentFuel(GetFuel())
    , m_movesRemaining(GetMovement())
    , m_xp(0)
{
    // Throws if the stacking rule forbids rTile; the destructor is not run for a unit
    // whose constructor throws, so no unregistration is needed on that path.
    m_rPositions.Register_(*this, rTile);
}

Unit::~Unit()
{
    m_rPositions.Unregister_(*this);
}

const UnitDesign& Unit::GetDesign() const                              { return m_rDesign; }

int Unit::ResolveStat_(StatId_t statId) const
{
    const std::vector<ActiveEffect_t> effects = CollectLiveUnitEffects_(m_rDesign, m_rFaction);
    return static_cast<int>(ResolveStatModifiers(FilterByStatId(effects, statId), SeedFor(statId)).total);
}

bool Unit::ResolveFlag_(RuleFlagId_t flagId) const
{
    for (const ActiveEffect_t& rEffect : CollectLiveUnitEffects_(m_rDesign, m_rFaction))
    {
        const RuleFlagEffect_t* pFlag = std::get_if<RuleFlagEffect_t>(&rEffect.config->effect);
        if (pFlag && pFlag->flag == flagId)
        {
            return true;
        }
    }
    return false;
}

int Unit::GetBaseCost() const                                          { return m_rDesign.GetBaseCost(); }
int Unit::GetAttack() const                                            { return ResolveStat_(StatId_t::Attack); }
int Unit::GetAttackAgainst(const Unit& rDefender) const
{
    EffectContext_t ctx;
    ctx.targetTile = &rDefender.GetTile();
    const std::vector<ActiveEffect_t> effects = CollectLiveUnitEffects_(m_rDesign, m_rFaction);
    return static_cast<int>(
        ResolveStatModifiers(FilterByStatIdInContext(effects, StatId_t::Attack, ctx), SeedFor(StatId_t::Attack)).total);
}
int Unit::GetDefense() const                                           { return ResolveStat_(StatId_t::Defense); }
int Unit::GetMovement() const                                          { return ResolveStat_(StatId_t::Movement); }
int Unit::GetHitPoints() const                                         { return ResolveStat_(StatId_t::HitPoints); }
int Unit::GetDisengageChance() const                                   { return ResolveStat_(StatId_t::DisengageChance); }
int Unit::GetFuel() const                                              { return ResolveStat_(StatId_t::Fuel); }
int Unit::GetDamageFromOutOfFuel() const                               { return ResolveStat_(StatId_t::DamageFromOutOfFuel); }
bool Unit::IsFlight() const                                            { return ResolveFlag_(RuleFlagId_t::Flight); }
int Unit::GetCargoCapacity() const                                     { return ResolveStat_(StatId_t::CargoCapacity); }
int Unit::GetDifficultTerrainCost() const                              { return ResolveStat_(StatId_t::DifficultTerrainCost); }
bool Unit::IsSingleUse() const                                         { return ResolveFlag_(RuleFlagId_t::SingleUse); }
const Tile& Unit::GetTile() const                                      { return *m_pTile; }
BaseManager* Unit::GetHomeBase() const      { return m_pHomeBase; }
Faction& Unit::GetFaction() const           { return m_rFaction; }

int Unit::GetCurrentHp() const              { return m_currentHp; }
int Unit::GetCurrentFuel() const            { return m_currentFuel; }
int Unit::GetMovesRemaining() const         { return m_movesRemaining; }
int Unit::GetXp() const                     { return m_xp; }

// Current stats are clamped to [0, live max] so the invariant 0 <= current <= max holds
// regardless of caller arithmetic (overkill damage, refuel past capacity, ...). The maxima
// are the live resolved values, so they track faction-effect changes made after spawn.
void Unit::SetCurrentHp(int hp)             { m_currentHp = std::clamp(hp, 0, GetHitPoints()); }
void Unit::SetCurrentFuel(int fuel)         { m_currentFuel = std::clamp(fuel, 0, GetFuel()); }
void Unit::SetMovesRemaining(int moves)     { m_movesRemaining = std::clamp(moves, 0, GetMovement()); }
void Unit::SetXp(int xp)                    { m_xp = std::max(xp, 0); }
void Unit::SetHomeBase(BaseManager* pHomeBase) { m_pHomeBase = pHomeBase; }

std::optional<UnitOrder_t>& Unit::GetOrder()             { return m_order; }
const std::optional<UnitOrder_t>& Unit::GetOrder() const { return m_order; }
void Unit::SetOrder(const UnitOrder_t& rOrder)            { m_order = rOrder; }
void Unit::ClearOrder()                                   { m_order.reset(); }

} // namespace ac
