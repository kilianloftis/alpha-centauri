#pragma once

#include "game/faction/base/HomeBaseIndex.h"
#include "game/map/WorkedTileIndex.h"
#include "game/units/MoraleCalculator.h"
#include "game/units/UnitDesign.h"
#include "game/units/UnitOrder.h"
#include <optional>
#include <string>
#include <vector>

namespace ac
{

class Tile;
class BaseManager;
class Faction;
class UnitManager;
class UnitPositionIndex;

using UnitId_t = int;

// Non-recomputable state needed to destroy a unit and rebuild it under a new owner.
// Home / produced-at are omitted — foreign home claims are invalid after transfer.
struct UnitSnapshot_t
{
    UnitId_t unitId = 0;
    const UnitDesign* pDesign = nullptr;
    const Tile* pTile = nullptr;
    int currentHp = 0;
    int currentFuel = 0;
    int xp = 0;
    int moveFragmentsRemaining = 0;
};

class Unit
{
public:
    // Registers the unit on rTile in rPositions for its whole life (unregistered in the
    // destructor). The index is the single owner of unit-position state: it keeps
    // GetTile() in sync on every move (UnitPositionIndex::MoveUnit) and there is no
    // other way to change a unit's position. Placement legality is the caller's job
    // (UnitManager::CreateUnit). unitId must be unique for the life of the game
    // (WorldMap's unit IdAllocator).
    // pProducedAt is the base that built this unit (train bonuses). Distinct from pHomeBase;
    // when null, defaults to pHomeBase so typical CreateUnit(home) keeps both aligned.
    // rMorale is the game-wide calculator owned by GameDataContext (supplied by the owning
    // UnitManager); used here only to seed intrinsic XP and clamp SetXp.
    Unit(UnitId_t unitId,
         const UnitDesign& rDesign,
         UnitPositionIndex& rPositions,
         const Tile& rTile,
         BaseManager* pHomeBase,
         Faction& rFaction,
         const MoraleCalculator& rMorale,
         BaseManager* pProducedAt = nullptr);
    ~Unit();

    UnitId_t GetUnitId() const;

    const UnitDesign& GetDesign() const;

    // Capture identity and live combat/move state for transfer.
    UnitSnapshot_t CaptureSnapshot() const;

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
    // Base that produced this unit (nullptr if unknown / destroyed). Independent of home.
    BaseManager* GetProducedAtBase() const;
    Faction& GetFaction();
    const Faction& GetFaction() const;

    int GetCurrentHp() const;
    int GetCurrentFuel() const;
    // Chassis Movement stat in move-points (not fragments).
    int GetMovementPoints() const;
    // Remaining movement in fragments (k_moveFragmentsPerPoint per Movement point).
    int GetMoveFragmentsRemaining() const;
    int GetXp() const;

    void SetCurrentHp(int hp);
    void SetCurrentFuel(int fuel);
    // Clamps to [0, MovementPoints * k_moveFragmentsPerPoint].
    void SetMoveFragmentsRemaining(int fragments);
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

    // Attack history for the disengage rule: a unit that attacked on its current or previous
    // turn may not disengage. MarkAttacked is called by UnitOrderExecutor::TryAttack;
    // AdvanceAttackHistory shifts this-turn → last-turn at TurnStart.
    bool HasAttackedThisTurn() const;
    bool HasAttackedLastTurn() const;
    void MarkAttacked();
    void AdvanceAttackHistory();

    // ThisUnit-scoped InterceptAttempt deploy cooldown (mission year when ready again).
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
    // Production base id (looked up via GetProducedAtBase); independent of home claim.
    std::optional<int> m_producedAtBaseId;
    Faction& m_rFaction;
    const MoraleCalculator& m_rMorale;

    int m_currentHp;
    int m_currentFuel;
    int m_moveFragmentsRemaining;
    int m_xp;
    std::optional<UnitOrder_t> m_order;
    // Held while supply-crawling (Pop-equivalent); releases the tile when destroyed/cleared.
    WorkedTileClaim m_tileClaim;
    bool m_bRegistered;
    bool m_bAttackedThisTurn = false;
    bool m_bAttackedLastTurn = false;
    // Available when missionYear >= this (0 = always ready at game start).
    int m_interceptReadyMissionYear = 0;
    Unit* m_pCarrier = nullptr;
    std::vector<Unit*> m_cargo;
};

} // namespace ac
