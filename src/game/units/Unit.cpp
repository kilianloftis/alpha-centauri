#include "game/units/Unit.h"

#include "game/Faction.h"
#include "game/faction/Military.h"
#include "game/faction/base/BaseManager.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorkedTileIndex.h"
#include "game/map/WorldMap.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/TileEffectsContext.h"
#include "game/units/MovementConstants.h"
#include "game/units/TransportRules.h"

#include <algorithm>
#include <stdexcept>
#include <variant>

namespace ac
{

bool IsCrawlResource(StatId_t resource)
{
    return std::find(k_CrawlResources.begin(), k_CrawlResources.end(), resource)
           != k_CrawlResources.end();
}

Unit::Unit(UnitId_t unitId,
           const UnitDesign& rDesign,
           UnitPositionIndex& rPositions,
           const Tile& rTile,
           BaseManager* pHomeBase,
           Faction& rFaction,
           const MoraleCalculator& rMorale,
           BaseManager* pProducedAt)
    : m_unitId(unitId)
    , m_rDesign(rDesign)
    , m_rPositions(rPositions)
    , m_pTile(&rTile)
    , m_pFaction(&rFaction)
    , m_rMorale(rMorale)
    // Seed current stats from the *live* resolved maxima (design effects + the faction's
    // active FactionUnits effects), not the design's context-free values. A fresh unit
    // therefore starts at its true max, so current-vs-max is well defined from spawn.
    , m_currentHp(0)
    , m_currentFuel(0)
    , m_moveFragmentsRemaining(0)
    , m_xp(0)
    // Prototype XP is "first one you built" (docs/game-rules-decisions.md): only an explicit
    // production base latches true. Free spawns (Engine starting units, escape pods) still
    // unlock the ledger via UnitManager::RecordBuiltComponents, but do not collect the bonus.
    // Latch before that record so StartingExperience below and later IsPrototype() agree.
    , m_bPrototype(pProducedAt != nullptr && rFaction.GetMilitary().IsPrototype(rDesign))
    , m_bRegistered(false)
{
    if (pHomeBase)
    {
        m_homeBaseClaim = pHomeBase->GetHomeUnits().Claim(*this);
    }
    // Production base is independent of home; default to home when the caller omits it
    // (train-bonus bookkeeping only — not a prototype signal; see m_bPrototype above).
    if (BaseManager* pBuiltAt = pProducedAt ? pProducedAt : pHomeBase)
    {
        m_producedAtBaseId = pBuiltAt->GetBaseId();
    }

    // Members used by ResolveStat are initialised above; seed after the mem-init list.
    // ProducedAtThisBase StartingExperience needs m_producedAtBaseId before this resolve.
    m_currentHp = ResolveStat(*this, StatId_t::HitPoints);
    m_currentFuel = GetMaxFuel();
    m_moveFragmentsRemaining =
        GetMovementPoints() * MovementConstants_t::k_moveFragmentsPerPoint;
    m_xp = m_rMorale.BaseIntrinsicXp(*this)
        + ResolveStat(*this, StatId_t::StartingExperience);

    m_rPositions.Register_(*this, rTile);
    m_bRegistered = true;
}

Unit::~Unit()
{
    DetachFromWorld_();
}

void Unit::ClearCargoLinks_()
{
    if (m_pCarrier)
    {
        auto& rCargo = m_pCarrier->m_cargo;
        rCargo.erase(std::remove(rCargo.begin(), rCargo.end(), this), rCargo.end());
        m_pCarrier = nullptr;
    }
    for (Unit* pPassenger : m_cargo)
    {
        if (pPassenger && pPassenger->m_pCarrier == this)
        {
            pPassenger->m_pCarrier = nullptr;
        }
    }
    m_cargo.clear();
}

void Unit::DetachFromWorld_()
{
    ClearCargoLinks_();
    ReleaseWorkedTile_();
    if (m_bRegistered)
    {
        m_rPositions.Unregister_(*this);
        m_bRegistered = false;
    }
}

bool Unit::IsEmbarked() const
{
    return m_pCarrier != nullptr;
}

Unit* Unit::GetCarrier() const
{
    return m_pCarrier;
}

const std::vector<Unit*>& Unit::GetCargo() const
{
    return m_cargo;
}

void Unit::EmbarkInto(Unit& rCarrier)
{
    if (m_pCarrier == &rCarrier)
    {
        return;
    }
    // The invariants used to be documented as caller duties, which meant one missed call site
    // could overfill m_cargo (FreeCargoSlots then goes negative) or link a passenger to a
    // carrier on another tile, which MoveUnit would still tow. Enforced here so the cargo graph
    // cannot be put into a state the rules say is impossible.
    if (&rCarrier == this)
    {
        throw std::invalid_argument("Unit::EmbarkInto: a unit cannot carry itself");
    }
    if (&rCarrier.GetTile() != &GetTile())
    {
        throw std::invalid_argument("Unit::EmbarkInto: carrier and passenger are not on the "
                                    "same tile");
    }
    if (!CanCarryPassenger(rCarrier, *this))
    {
        throw std::invalid_argument("Unit::EmbarkInto: carrier cannot take this passenger "
                                    "(capacity, domain, faction, or the carrier is itself cargo)");
    }
    if (m_pCarrier)
    {
        Disembark();
    }
    m_pCarrier = &rCarrier;
    rCarrier.m_cargo.push_back(this);
}

void Unit::Disembark()
{
    if (!m_pCarrier)
    {
        return;
    }
    auto& rCargo = m_pCarrier->m_cargo;
    rCargo.erase(std::remove(rCargo.begin(), rCargo.end(), this), rCargo.end());
    m_pCarrier = nullptr;
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
BaseManager* Unit::GetHomeBase() const      { return m_homeBaseClaim.GetBase(); }
BaseManager* Unit::GetProducedAtBase() const
{
    if (!m_producedAtBaseId.has_value())
    {
        return nullptr;
    }
    for (BaseManager& rBase : m_pFaction->Bases())
    {
        if (rBase.GetBaseId() == *m_producedAtBaseId)
        {
            return &rBase;
        }
    }
    return nullptr;
}
void Unit::ClearProducedAtBase()            { m_producedAtBaseId.reset(); }
Faction& Unit::GetFaction()                 { return *m_pFaction; }
const Faction& Unit::GetFaction() const     { return *m_pFaction; }

void Unit::RebindFaction(Faction& rFaction)
{
    m_pFaction = &rFaction;
}

int Unit::GetCurrentHp() const              { return m_currentHp; }
int Unit::GetCurrentFuel() const            { return m_currentFuel; }
int Unit::GetMaxFuel() const                { return m_rDesign.MaxFuel(); }
int Unit::GetMovementPoints() const         { return ResolveStat(*this, StatId_t::Movement); }
int Unit::GetMineralUpkeep() const
{
    return std::max(0, ResolveStat(*this, StatId_t::MineralUpkeep));
}
int Unit::GetMoveFragmentsRemaining() const { return m_moveFragmentsRemaining; }
int Unit::GetXp() const                     { return m_xp; }
bool Unit::IsPrototype() const              { return m_bPrototype; }
bool Unit::IsCombatUnit() const             { return m_rDesign.IsCombatUnit(); }

// Current stats are clamped to [0, live max] so the invariant 0 <= current <= max holds
// regardless of caller arithmetic (overkill damage, refuel past capacity, ...). The maxima
// are the live resolved values, so they track faction-effect changes made after spawn.
void Unit::SetCurrentHp(int hp)
{
    m_currentHp = std::clamp(hp, 0, ResolveStat(*this, StatId_t::HitPoints));
}
void Unit::SetCurrentFuel(int fuel)
{
    m_currentFuel = std::clamp(fuel, 0, GetMaxFuel());
}
void Unit::SetMoveFragmentsRemaining(int fragments)
{
    const int maxFragments =
        GetMovementPoints() * MovementConstants_t::k_moveFragmentsPerPoint;
    m_moveFragmentsRemaining = std::clamp(fragments, 0, maxFragments);
}
void Unit::SpendMoveFragments(int fragments)
{
    if (fragments <= 0)
    {
        return;
    }
    SetMoveFragmentsRemaining(m_moveFragmentsRemaining - fragments);
    if (!m_rDesign.UsesFuel())
    {
        return;
    }
    const int pointsSpent = fragments / MovementConstants_t::k_moveFragmentsPerPoint;
    if (pointsSpent > 0)
    {
        SetCurrentFuel(m_currentFuel - pointsSpent);
    }
}
void Unit::SpendRemainingMoveFragments()
{
    SpendMoveFragments(m_moveFragmentsRemaining);
}
void Unit::SetXp(int xp)
{
    m_xp = std::clamp(xp, 0, m_rMorale.GetConfig().MaxLevel());
}

void Unit::SetHomeBase(BaseManager* pHomeBase)
{
    if (GetHomeBase() == pHomeBase)
    {
        return;
    }
    if (!pHomeBase)
    {
        m_homeBaseClaim = HomeBaseClaim{};
        return;
    }
    m_homeBaseClaim = pHomeBase->GetHomeUnits().Claim(*this);
}

std::optional<UnitOrder_t>& Unit::GetOrder()             { return m_order; }
const std::optional<UnitOrder_t>& Unit::GetOrder() const { return m_order; }

void Unit::SetTileClaim(WorkedTileClaim claim)
{
    m_tileClaim = std::move(claim);
}

const Tile* Unit::GetWorkedTile() const
{
    return m_tileClaim.GetTile();
}

void Unit::ReleaseWorkedTile_()
{
    m_tileClaim = WorkedTileClaim{};
}

void Unit::SetOrder(const UnitOrder_t& rOrder)
{
    ReleaseWorkedTile_();
    m_order = rOrder;
}

void Unit::ClearOrder()
{
    ReleaseWorkedTile_();
    m_order.reset();
}

bool Unit::TryStartSupplyCrawl(StatId_t resource)
{
    if (!IsCrawlResource(resource))
    {
        throw std::runtime_error("TryStartSupplyCrawl: resource must be nutrients, minerals, or energy");
    }
    BaseManager* pHomeBase = GetHomeBase();
    if (!GetFlag(RuleFlagId_t::SupplyCrawl) || !pHomeBase)
    {
        return false;
    }

    ClearOrder();

    // Claim the current tile directly (same WorkedTileIndex as workers; any free tile).
    WorkedTileClaim claim = pHomeBase->GetTileEffects().GetWorldMap().GetWorkedTiles().TryClaim(
        GetTile(),
        /*bUserAssigned*/ true,
        [this]() { ClearOrder(); });
    if (!claim.GetTile())
    {
        return false;
    }

    m_tileClaim = std::move(claim);
    // Claim is held; set the order without SetOrder (which would release it).
    m_order = SupplyCrawlOrder_t{resource};
    return true;
}

bool Unit::IsSupplyCrawling() const
{
    return m_tileClaim.GetTile() != nullptr
        && m_order.has_value()
        && std::holds_alternative<SupplyCrawlOrder_t>(*m_order);
}

bool Unit::RequiresOrders() const
{
    return !m_order.has_value() && m_moveFragmentsRemaining > 0;
}

bool Unit::HasAttackedThisTurn() const { return m_bAttackedThisTurn; }
bool Unit::HasAttackedLastTurn() const { return m_bAttackedLastTurn; }
void Unit::MarkAttacked()              { m_bAttackedThisTurn = true; }
void Unit::AdvanceAttackHistory()
{
    m_bAttackedLastTurn = m_bAttackedThisTurn;
    m_bAttackedThisTurn = false;
}

bool Unit::IsInterceptReady(int missionYear) const
{
    return missionYear >= m_interceptReadyMissionYear;
}

void Unit::DeployIntercept(int readyMissionYear)
{
    m_interceptReadyMissionYear = readyMissionYear;
}

} // namespace ac
