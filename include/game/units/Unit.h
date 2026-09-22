#pragma once

#include <set>

#include "game/effects/ActiveEffect.h"
#include "game/faction/base/HomeBaseIndex.h"
#include "game/map/WorkedTileIndex.h"
#include "game/units/MoraleCalculator.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitOrder.h"
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace ac
{

// What a supply crawler may carry. The rule and the UI that offers the choice read the same
// list, so a menu entry cannot exist that TryStartSupplyCrawl then rejects.
inline constexpr std::array<StatId_t, 3> k_CrawlResources = {
    StatId_t::Nutrients,
    StatId_t::Minerals,
    StatId_t::Energy,
};

bool IsCrawlResource(StatId_t resource);

class Tile;
class BaseManager;
class Faction;
class UnitManager;
class UnitPositionIndex;

using UnitId_t = int;

class Unit
{
public:
    // Registers the unit on rTile in rPositions for its whole life (unregistered in the
    // destructor). The index is the single owner of unit-position state: it keeps
    // GetTile() in sync on every move (UnitPositionIndex::MoveUnit) and there is no
    // other way to change a unit's position. Placement legality is the caller's job
    // (UnitManager::CreateUnit). unitId must be unique for the life of the game
    // (WorldMap's unit IdAllocator).
    // pProducedAt is ephemeral: stamp ProducedAtThisBase grants + prototype latch. Distinct
    // from pHomeBase, and three-valued so "homed but built nowhere" is sayable:
    //   nullopt (default) — unspecified; stamp from pHomeBase when set; fires on_unit_produced
    //                       when that base is set (but does not latch prototype)
    //   a base           — stamp / on_unit_produced from that base; only form that latches prototype
    //   nullptr          — explicitly produced nowhere: no stamps, no on_unit_produced (gift)
    // Production passes the same base as both. The production base is not stored.
    // rMorale is the game-wide calculator owned by GameDataContext (supplied by the owning
    // UnitManager); used here only to seed intrinsic XP and clamp SetXp.
    Unit(UnitId_t unitId,
         const UnitDesign& rDesign,
         UnitPositionIndex& rPositions,
         const Tile& rTile,
         BaseManager* pHomeBase,
         Faction& rFaction,
         const MoraleCalculator& rMorale,
         std::optional<BaseManager*> pProducedAt = std::nullopt);
    ~Unit();

    UnitId_t GetUnitId() const;

    const UnitDesign& GetDesign() const;

    // Live-unit stat / flag resolution (design effects + FactionUnits). Prefer the free
    // ResolveStat / ResolveFlag overloads; these forward to them.
    int GetStat(StatId_t statId) const;
    int GetStat(StatId_t statId, const EffectContext_t& rCtx) const;
    bool GetFlag(RuleFlagId_t flagId) const;
    UnitDomain_t GetDomain() const;

    // Map position (UnitPositionIndex). Distinct from GetWorkedTile().
    const Tile& GetTile() const;
    // Home base from the held HomeBaseClaim (nullptr when unset or the base was destroyed).
    BaseManager* GetHomeBase() const;
    // Permanent ProducedAtThisBase grants stamped at construction (empty for gifts / free spawns
    // with an explicit null production base). Survives rehome and ownership transfer.
    const std::vector<ActiveEffect_t>& GetProductionGrants() const;
    Faction& GetFaction();
    const Faction& GetFaction() const;
    // Ownership transfer (Faction::TransferUnitTo): rebind to the new owner without
    // destroying/recreating the unit. Does not touch home base, cargo, or position —
    // callers apply those rules (see docs/architecture/high-level.md, "Object lifetime").
    void RebindFaction(Faction& rFaction);

    int GetCurrentHp() const;
    int GetCurrentFuel() const;
    // Design max fuel pool (TurnsOfFuel × Movement); 0 when the design does not use fuel.
    int GetMaxFuel() const;
    // Chassis Movement stat in move-points (not fragments).
    int GetMovementPoints() const;

    // Live mineral support cost (design + FactionUnits), floored at 0.
    int GetMineralUpkeep() const;
    // Remaining movement in fragments (k_moveFragmentsPerPoint per Movement point).
    int GetMoveFragmentsRemaining() const;
    // Keys of triggered effects carrying `oncePer` that this unit has already consumed
    // (see TriggeredEffectConfig_t::oncePer). Authored keys, so the rule spans instances:
    // every Monolith shares "monolith_xp", and a second visit grants nothing.
    std::set<std::string>& ConsumedTriggerKeys() { return m_consumedTriggerKeys; }
    const std::set<std::string>& ConsumedTriggerKeys() const { return m_consumedTriggerKeys; }

    int GetXp() const;
    // True when this unit's design carried a component its faction had never fielded at the
    // moment the unit was created. Fixed for life: the faction's build ledger keeps moving,
    // but what a given unit was when it rolled off the line does not.
    bool IsPrototype() const;
    // Forwards to UnitDesign::IsCombatUnit (component Attack / ForcesPsiCombat).
    bool IsCombatUnit() const;

    void SetCurrentHp(int hp);
    void SetCurrentFuel(int fuel);
    // Clamps to [0, MovementPoints * k_moveFragmentsPerPoint]. Does not burn fuel
    // (TurnStart refresh, test setup). Gameplay spend uses SpendMoveFragments.
    void SetMoveFragmentsRemaining(int fragments);
    // Refill moves and roll Unit-owned turn latches (attack history, airdropped). Called from
    // TurnStart; order policy (e.g. clearing SkipTurn) stays on UnitOrderExecutor::OnTurnStart.
    void BeginTurn();
    // Subtract remaining fragments and, when the design uses fuel, burn the matching
    // move-points of fuel (attacks, steps, and other intentional move spends).
    void SpendMoveFragments(int fragments);
    // Spend whatever move fragments remain (end-turn actions, AttackingEndsTurn, …).
    void SpendRemainingMoveFragments();
    void SetXp(int xp);
    // Claims rHomeBase's HomeBaseIndex (or clears). Replaces any previous home claim.
    void SetHomeBase(BaseManager* pHomeBase);

    std::optional<UnitOrder_t>& GetOrder();
    const std::optional<UnitOrder_t>& GetOrder() const;
    void SetOrder(const UnitOrder_t& rOrder);
    void ClearOrder();

    // Worked-tile claim for supply crawl (same WorkedTileClaim shape as Pop). Minted via
    // WorkedTileIndex in TryStartSupplyCrawl; yield is collected from HomeBaseIndex units.
    void SetTileClaim(WorkedTileClaim claim);
    // The claimed harvest tile, or nullptr when unassigned. Not the unit's map position.
    const Tile* GetWorkedTile() const;

    // Begin a supply crawl on the unit's current tile. Claims via the world WorkedTileIndex
    // (any free tile). Requires SupplyCrawl flag, a home base, and a free tile.
    // resource must be Nutrients, Minerals, or Energy.
    bool TryStartSupplyCrawl(StatId_t resource);

    // True while a SupplyCrawl order is active and the unit holds a worked-tile claim.
    bool IsSupplyCrawling() const;

    // True when the unit still needs an order this turn (moves left and no active order).
    bool RequiresOrders() const;

    // Attack history for the disengage rule: a unit that attacked on its current or previous
    // turn may not disengage. MarkAttacked is called by UnitOrderExecutor::TryAttack;
    // BeginTurn shifts this-turn → last-turn.
    bool HasAttackedThisTurn() const;
    bool HasAttackedLastTurn() const;
    void MarkAttacked();
    void AdvanceAttackHistory();

    // Latched for the turn after a successful airdrop (cleared in BeginTurn). Gates post-drop
    // attack StatModifiers via HasAirdroppedThisTurn, and blocks chaining a second drop onto
    // a friendly pad while moves remain.
    bool HasAirdroppedThisTurn() const;
    void MarkAirdropped();
    void ClearAirdroppedThisTurn();

    // ThisUnit-scoped Intercept deploy cooldown (mission year when ready again).
    bool IsInterceptReady(int missionYear) const;
    void DeployIntercept(int readyMissionYear);

    // Cargo / transport. Embarked units share the carrier's tile. Outside a base they are
    // ignored for combat targeting, ZOC, and tile occupancy; in a base they may defend and
    // block (carrier preferred as the combat target when both are present).
    bool IsEmbarked() const;
    Unit* GetCarrier() const;
    const std::vector<Unit*>& GetCargo() const;
    // Link passenger into rCarrier (same tile). Caller enforces capacity / domain / load site.
    void EmbarkInto(Unit& rCarrier);
    // Clear carrier link; passenger remains on its current tile.
    void Disembark();

private:
    // The index maintains m_pTile alongside its occupancy lists (MoveUnit).
    friend class UnitPositionIndex;
    friend class UnitManager;

    // Removes this unit from world occupancy before deferred object reclamation. Safe to
    // call once; the destructor skips unregistering an already-detached unit.
    void DetachFromWorld_();
    void ReleaseWorkedTile_();
    void ClearCargoLinks_();

    UnitId_t m_unitId;
    const UnitDesign& m_rDesign;
    UnitPositionIndex& m_rPositions;
    const Tile* m_pTile;
    // Holding the claim IS the home-base link (see HomeBaseIndex).
    HomeBaseClaim m_homeBaseClaim;
    // ProducedAtThisBase effects copied at construction; originBase cleared on each copy.
    // config pointers remain valid for the unit's life: every ProducedAtThisBase EffectConfig_t
    // is owned by a load-time registry (buildings, ratings, …), not by the ephemeral faction
    // effects pool — the stamp deliberately outlives pool rebuilds.
    std::vector<ActiveEffect_t> m_productionGrants;
    // Rebindable owner (RebindFaction): never null while the unit lives in a UnitManager.
    Faction* m_pFaction;
    const MoraleCalculator& m_rMorale;

    std::set<std::string> m_consumedTriggerKeys;
    int m_currentHp;
    int m_currentFuel;
    int m_moveFragmentsRemaining;
    int m_xp;
    // Latched at construction: true only when this unit was produced (a non-null pProducedAt)
    // and Military::IsPrototype was true before the ledger recorded the design. Free spawns
    // never latch; see docs/game-rules-decisions.md ("first one you built").
    bool m_bPrototype;
    std::optional<UnitOrder_t> m_order;
    // Held while supply-crawling (Pop-equivalent); releases the tile when destroyed/cleared.
    WorkedTileClaim m_tileClaim;
    bool m_bRegistered;
    bool m_bAttackedThisTurn = false;
    bool m_bAttackedLastTurn = false;
    bool m_bAirdroppedThisTurn = false;
    // Available when missionYear >= this (0 = always ready at game start).
    int m_interceptReadyMissionYear = 0;
    Unit* m_pCarrier = nullptr;
    std::vector<Unit*> m_cargo;
};

} // namespace ac
