#pragma once

#include <set>

#include "game/effects/ActiveEffect.h"
#include "game/base/HomeBaseIndex.h"
#include "game/map/WorkedTileIndex.h"
#include "game/combat/MoraleCalculator.h"
#include "game/units/IDesign.h"
#include "game/units/UnitOrder.h"
#include "game/units/UnitOrigin.h"
#include "game/units/UnitTransport.h"
#include "game/units/UnitTurnLatches.h"
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace ac
{

// What a supply crawler may harvest. The rule and the UI both read this list.
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
    // Registers on rTile in rPositions for its whole life; the caller checks placement.
    Unit(UnitId_t unitId,
         const IDesign& rDesign,
         UnitPositionIndex& rPositions,
         const Tile& rTile,
         const UnitOrigin& rOrigin,
         Faction& rFaction,
         const MoraleCalculator& rMorale);
    ~Unit();

    UnitId_t GetUnitId() const;

    const IDesign& GetDesign() const;
    UnitDomain_t GetDomain() const;

    // Map position (UnitPositionIndex). Distinct from GetWorkedTile().
    const Tile& GetTile() const;
    // Home base from the held HomeBaseClaim (nullptr when unset or the base was destroyed).
    BaseManager* GetHomeBase() const;
    // Permanent ProducedAtThisBase grants stamped at construction (empty for a gift). Survives
    // rehome and ownership transfer.
    const std::vector<ActiveEffect_t>& GetProductionGrants() const;
    Faction& GetFaction();
    const Faction& GetFaction() const;
    // For ownership transfer. Home base, cargo and position are the caller's to update.
    void RebindFaction(Faction& rFaction);
    // Swap the design pointer; clamps HP, fuel, and move fragments to the new maxima.
    // Does not touch prototype latch, XP, or the built-component ledger.
    void RebindDesign(const IDesign& rDesign);

    int GetCurrentHp() const;
    int GetCurrentFuel() const;
    // IDesign max fuel pool (TurnsOfFuel × Movement); 0 when the design does not use fuel.
    int GetMaxFuel() const;
    // Chassis Movement stat in move-points (not fragments).
    int GetMovementPoints() const;

    // Live mineral support cost (design + FactionUnits), floored at 0.
    int GetMineralUpkeep() const;
    // Remaining movement in fragments (k_moveFragmentsPerPoint per Movement point).
    int GetMoveFragmentsRemaining() const;
    // oncePer keys this unit has used. Keys are authored, so every Monolith shares one.
    std::set<std::string>& ConsumedTriggerKeys() { return m_consumedTriggerKeys; }
    const std::set<std::string>& ConsumedTriggerKeys() const { return m_consumedTriggerKeys; }

    int GetXp() const;
    // Whether its design had a component the faction had never fielded when it was built.
    bool IsPrototype() const;
    bool IsCombatUnit() const;

    void SetCurrentHp(int hp);
    void SetCurrentFuel(int fuel);
    // Clamps to [0, MovementPoints * k_moveFragmentsPerPoint]. Does not burn fuel
    // (TurnStart refresh, captures, test setup). Gameplay spend uses SpendMoveFragments.
    void SetMoveFragmentsRemaining(int fragments);
    // Refills moves and rolls the per-turn latches. Orders are UnitOrderExecutor's.
    void BeginTurn();
    // Subtract remaining fragments and, when the design uses fuel, burn the matching
    // move-points of fuel (attacks, steps, and other intentional move spends).
    void SpendMoveFragments(int fragments);
    // Spend whatever move fragments remain (end-turn actions, AttackingEndsTurn, …).
    void SpendRemainingMoveFragments();
    void SetXp(int xp);
    // Claims pHomeBase's HomeBaseIndex, releasing any previous claim. nullptr clears it.
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

    // Holds claim and sets a SupplyCrawl order for resource; SupplyCrawlMutations decides.
    void AdoptSupplyCrawl(WorkedTileClaim claim, StatId_t resource);

    // True while a SupplyCrawl order is active and the unit holds a worked-tile claim.
    bool IsSupplyCrawling() const;

    // True when the unit still needs an order this turn (moves left and no active order).
    bool RequiresOrders() const;

    UnitTurnLatches& Latches() { return m_latches; }
    const UnitTurnLatches& Latches() const { return m_latches; }

    UnitTransport& Transport() { return m_transport; }
    const UnitTransport& Transport() const { return m_transport; }

private:
    // The index maintains m_pTile alongside its occupancy lists (MoveUnit).
    friend class UnitPositionIndex;
    friend class UnitManager;

    // Removes this unit from world occupancy before deferred object reclamation. Safe to
    // call once; the destructor skips unregistering an already-detached unit.
    void DetachFromWorld_();
    void ReleaseWorkedTile_();
    int MaxMoveFragments_() const;

    UnitId_t m_unitId;
    const IDesign* m_pDesign;
    UnitPositionIndex& m_rPositions;
    const Tile* m_pTile;
    // Holding the claim is the home-base link.
    HomeBaseClaim m_homeBaseClaim;
    // Copied at construction. Their configs live in load-time registries, so they outlive
    // faction effect-pool rebuilds.
    std::vector<ActiveEffect_t> m_productionGrants;
    // Rebindable owner (RebindFaction): never null while the unit lives in a UnitManager.
    Faction* m_pFaction;
    const MoraleCalculator& m_rMorale;

    std::set<std::string> m_consumedTriggerKeys;
    int m_currentHp;
    int m_currentFuel;
    int m_moveFragmentsRemaining;
    int m_xp;
    // Latched at construction; see docs/game-rules-decisions.md ("first one you built").
    bool m_bPrototype;
    std::optional<UnitOrder_t> m_order;
    // Held while supply-crawling (Pop-equivalent); releases the tile when destroyed/cleared.
    WorkedTileClaim m_tileClaim;
    bool m_bRegistered;
    UnitTurnLatches m_latches;
    UnitTransport m_transport;
};

} // namespace ac
