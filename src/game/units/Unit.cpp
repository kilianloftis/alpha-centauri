#include "game/units/Unit.h"

#include "game/Faction.h"
#include "game/map/Tile.h"
#include "lib/effects/ActiveEffect.h"

namespace ac
{

namespace
{

// A live unit's full effect list: its design's own component effects (ThisUnit lane)
// plus the owning faction's FactionUnits-scoped effects.
std::vector<ActiveEffect_t> CollectLiveUnitEffects_(const UnitDesign& rDesign, const Faction& rFaction)
{
    std::vector<ActiveEffect_t> effects = rDesign.CollectEffects();
    const std::vector<ActiveEffect_t> factionEffects =
        FilterByScope(CollectActiveEffects(rFaction).effects, EffectScope_t::FactionUnits);
    effects.insert(effects.end(), factionEffects.begin(), factionEffects.end());
    return effects;
}

} // namespace

Unit::Unit(const UnitDesign& rDesign,
           const Tile& rTile,
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

int Unit::ResolveStat_(StatId statId) const
{
    const std::vector<ActiveEffect_t> effects = CollectLiveUnitEffects_(m_rDesign, m_rFaction);
    return static_cast<int>(ResolveStatModifiers(FilterByStatId(effects, statId), SeedFor(statId)).total);
}

bool Unit::ResolveFlag_(RuleFlagId flagId) const
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
int Unit::GetAttack() const                                            { return ResolveStat_(StatId::Attack); }
int Unit::GetAttackAgainst(const Unit& rDefender) const
{
    EffectContext_t ctx;
    ctx.targetTile = &rDefender.GetTile();
    const std::vector<ActiveEffect_t> effects = CollectLiveUnitEffects_(m_rDesign, m_rFaction);
    return static_cast<int>(
        ResolveStatModifiers(FilterByStatIdInContext(effects, StatId::Attack, ctx), SeedFor(StatId::Attack)).total);
}
int Unit::GetDefense() const                                           { return ResolveStat_(StatId::Defense); }
int Unit::GetMovement() const                                          { return ResolveStat_(StatId::Movement); }
int Unit::GetHitPoints() const                                         { return ResolveStat_(StatId::HitPoints); }
int Unit::GetDisengageChance() const                                   { return ResolveStat_(StatId::DisengageChance); }
int Unit::GetFuel() const                                              { return ResolveStat_(StatId::Fuel); }
int Unit::GetDamageFromOutOfFuel() const                               { return ResolveStat_(StatId::DamageFromOutOfFuel); }
bool Unit::IsFlight() const                                            { return ResolveFlag_(RuleFlagId::Flight); }
int Unit::GetCargoCapacity() const                                     { return ResolveStat_(StatId::CargoCapacity); }
int Unit::GetDifficultTerrainCost() const                              { return ResolveStat_(StatId::DifficultTerrainCost); }
bool Unit::IsSingleUse() const                                         { return ResolveFlag_(RuleFlagId::SingleUse); }
const Tile& Unit::GetTile() const                                      { return *m_pTile; }
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
void Unit::SetTile(const Tile& rTile)       { m_pTile = &rTile; }
void Unit::SetHomeBase(BaseManager* pHomeBase) { m_pHomeBase = pHomeBase; }

std::optional<UnitOrder_t>& Unit::GetOrder()             { return m_order; }
const std::optional<UnitOrder_t>& Unit::GetOrder() const { return m_order; }
void Unit::SetOrder(const UnitOrder_t& rOrder)            { m_order = rOrder; }
void Unit::ClearOrder()                                   { m_order.reset(); }

} // namespace ac
