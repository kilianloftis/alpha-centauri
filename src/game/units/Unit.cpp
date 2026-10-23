#include "game/units/Unit.h"

#include "game/production/ProductionRules.h"
#include "game/faction/Faction.h"
#include "game/units/Military.h"
#include "game/base/BaseManager.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/map/WorkedTileIndex.h"
#include "game/map/WorldMap.h"
#include "game/effects/ActiveEffect.h"
#include "game/effects/EffectEnums.h"
#include "game/effects/TileEffectsContext.h"
#include "game/movement/MovementConstants.h"
#include "game/movement/TransportRules.h"

#include <algorithm>
#include <stdexcept>
#include <variant>

namespace ac
{

namespace
{

// ProducedAtThisBase grants from the base that built rUnit, with no origin, so a later transfer
// or razed base is never consulted.
std::vector<ActiveEffect_t> ProductionGrants_(const Unit& rUnit, const Faction& rFaction,
                                              const BaseManager& rBuiltAt)
{
    EffectContext_t context;
    context.pUnit = &rUnit;
    std::vector<ActiveEffect_t> grants;
    for (const ActiveEffect_t& rEffect : rFaction.GetActiveEffects().effects)
    {
        if (rEffect.config->scope == EffectScope_t::ProducedAtThisBase
            && rEffect.originBase == &rBuiltAt
            && ConditionSatisfied(*rEffect.config, context, rEffect.originBase))
        {
            grants.emplace_back(*rEffect.config, rEffect.sourceId, nullptr);
        }
    }
    return grants;
}

} // namespace

bool IsCrawlResource(StatId_t resource)
{
    return std::ranges::contains(k_CrawlResources, resource);
}

Unit::Unit(UnitId_t unitId,
           const IDesign& rDesign,
           UnitPositionIndex& rPositions,
           const Tile& rTile,
           const UnitOrigin& rOrigin,
           Faction& rFaction,
           const MoraleCalculator& rMorale)
    : m_unitId(unitId)
    , m_rDesign(rDesign)
    , m_rPositions(rPositions)
    , m_pTile(&rTile)
    , m_pFaction(&rFaction)
    , m_rMorale(rMorale)
    , m_currentHp(0)
    , m_currentFuel(0)
    , m_moveFragmentsRemaining(0)
    , m_xp(0)
    // Latch before RecordBuiltComponents so later IsPrototype() agrees with construction.
    , m_bPrototype(rOrigin.MayBePrototype() && ac::IsPrototype(rFaction, rDesign))
    , m_bRegistered(false)
{
    if (BaseManager* pHome = rOrigin.HomeBase())
    {
        m_homeBaseClaim = pHome->GetHomeUnits().Claim(*this);
    }
    if (BaseManager* pGrantsFrom = rOrigin.GrantsFrom())
    {
        m_productionGrants = ProductionGrants_(*this, rFaction, *pGrantsFrom);
    }

    // Here, once ResolveStat can read the members, so a new unit starts at its live maxima,
    // faction effects included.
    m_currentHp = ResolveStat(*this, StatId_t::HitPoints);
    m_currentFuel = GetMaxFuel();
    m_moveFragmentsRemaining = MaxMoveFragments_();
    m_xp = m_rMorale.StartingIntrinsicXp();
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
        std::erase(m_pCarrier->m_cargo, this);
        m_pCarrier = nullptr;
        // Still listed as cargo in the index until Unregister_ / RemoveFromTile_.
    }
    for (Unit* pPassenger : m_cargo)
    {
        pPassenger->m_pCarrier = nullptr;
        // Orphaned passenger becomes an independent occupant on the same tile.
        m_rPositions.NoteDisembarked_(*pPassenger);
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
    m_rPositions.NoteEmbarked_(*this);
}

void Unit::Disembark()
{
    if (!m_pCarrier)
    {
        return;
    }
    std::erase(m_pCarrier->m_cargo, this);
    m_pCarrier = nullptr;
    m_rPositions.NoteDisembarked_(*this);
}

UnitId_t Unit::GetUnitId() const { return m_unitId; }

const IDesign& Unit::GetDesign() const { return m_rDesign; }

UnitDomain_t Unit::GetDomain() const
{
    return m_rDesign.GetDomain();
}

const Tile& Unit::GetTile() const                                      { return *m_pTile; }
BaseManager* Unit::GetHomeBase() const      { return m_homeBaseClaim.GetBase(); }
const std::vector<ActiveEffect_t>& Unit::GetProductionGrants() const
{
    return m_productionGrants;
}
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

// Clamped to [0, live max], which follows faction effects gained after spawn.
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
    m_moveFragmentsRemaining = std::clamp(fragments, 0, MaxMoveFragments_());
}

int Unit::MaxMoveFragments_() const
{
    return GetMovementPoints() * MovementConstants_t::k_moveFragmentsPerPoint;
}

void Unit::BeginTurn()
{
    SetMoveFragmentsRemaining(MaxMoveFragments_());
    AdvanceAttackHistory();
    ClearAirdroppedThisTurn();
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
    if (!ResolveFlag(*this, RuleFlagId_t::SupplyCrawl) || !pHomeBase)
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

bool Unit::HasAirdroppedThisTurn() const { return m_bAirdroppedThisTurn; }
void Unit::MarkAirdropped()              { m_bAirdroppedThisTurn = true; }
void Unit::ClearAirdroppedThisTurn()     { m_bAirdroppedThisTurn = false; }

bool Unit::IsInterceptReady(int missionYear) const
{
    return missionYear >= m_interceptReadyMissionYear;
}

void Unit::DeployIntercept(int readyMissionYear)
{
    m_interceptReadyMissionYear = readyMissionYear;
}

} // namespace ac
