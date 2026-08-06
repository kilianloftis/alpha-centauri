#pragma once

#include "game/units/Unit.h"
#include "lib/Revision.h"
#include "lib/Signal.h"
#include <cstddef>
#include <memory>
#include <ranges>
#include <vector>

namespace ac
{

class UnitDesign;
class Tile;
class BaseManager;
class Faction;
class UnitPositionIndex;
class MoraleCalculator;

class UnitManager
{
public:
    class DeferredDestructionScope
    {
    public:
        DeferredDestructionScope(const DeferredDestructionScope&) = delete;
        DeferredDestructionScope& operator=(const DeferredDestructionScope&) = delete;
        DeferredDestructionScope(DeferredDestructionScope&& rOther) noexcept;
        DeferredDestructionScope& operator=(DeferredDestructionScope&&) = delete;
        ~DeferredDestructionScope();

    private:
        friend class UnitManager;
        explicit DeferredDestructionScope(UnitManager& rManager);

        UnitManager* m_pManager;
    };

    // rMorale is the game-wide calculator owned by GameDataContext; every unit this manager
    // creates borrows it, so morale rules need not be threaded through create/transfer calls.
    UnitManager(Faction& rFaction, const MoraleCalculator& rMorale);
    ~UnitManager() = default;

    // The unit registers itself on rTile in rPositions for its lifetime (see Unit's
    // constructor). Rejects the tile under the stacking rule (MovementRules) before
    // constructing. unitId must be unique across the game (caller: GameState::AllocateUnitId).
    // pProducedAt defaults to pHomeBase when null (see Unit ctor).
    Unit& CreateUnit(UnitId_t unitId, const UnitDesign& rDesign, UnitPositionIndex& rPositions,
                     const Tile& rTile,
                     BaseManager* pHomeBase = nullptr, BaseManager* pProducedAt = nullptr);
    void DestroyUnit(Unit& rUnit);

    // Ownership transfer, giver side (Faction::TransferUnitTo): removes rUnit from this
    // manager and returns it, WITHOUT applying DestroyUnit's combat carrier-loss rules and
    // WITHOUT emitting OnUnitDestroyed — the unit is not dying, it is leaving. Cargo / carrier
    // links, position registration, and world occupancy are untouched; the caller decides what
    // to do with them (see docs/architecture/high-level.md, "Object lifetime"). Throws if
    // rUnit is not owned by this manager.
    std::unique_ptr<Unit> ReleaseUnit(Unit& rUnit);
    // Ownership transfer, receiver side: takes a unit released from another manager (or newly
    // built off-manager), rebinds it to this manager's faction, and adds it as live. Does not
    // re-check tile placement (the unit never left the map) and does not emit OnUnitCreated —
    // observers that must react to "a unit now exists under this faction" without treating it
    // as a birth should use OnUnitAdopted. Throws if pUnit is null.
    Unit& AdoptUnit(std::unique_ptr<Unit> pUnit);

    // Keep destroyed objects alive until the scope ends so a pass may safely destroy its
    // current unit. DestroyUnit still removes the unit immediately from occupancy and all
    // Units() views; only memory reclamation is deferred. Scopes may be nested.
    [[nodiscard]] DeferredDestructionScope DeferDestruction();
    bool IsPendingDestruction(const Unit& rUnit) const;

    // Iterate live units by reference without exposing the owning unique_ptrs. Null slots
    // left by deferred destruction are skipped.
    auto Units()
    {
        return m_units
            | std::views::filter([](const std::unique_ptr<Unit>& pUnit) {
                  return pUnit != nullptr;
              })
            | std::views::transform([](const std::unique_ptr<Unit>& pUnit) -> Unit& {
                  return *pUnit;
              });
    }
    auto Units() const
    {
        return m_units
            | std::views::filter([](const std::unique_ptr<Unit>& pUnit) {
                  return pUnit != nullptr;
              })
            | std::views::transform([](const std::unique_ptr<Unit>& pUnit) -> const Unit& {
                  return *pUnit;
              });
    }

    // First unit with moves remaining and no order, or nullptr.
    // If pAfter is set, returns the next such unit after it (wrapping), or the first if
    // pAfter is not among units that require orders.
    Unit* GetNextAvailableUnit(const Unit* pAfter = nullptr) const;
    bool HasUnitsRequiringOrders() const;

    // Bumped on every unit creation/destruction; consumed by effect-pool caches.
    uint64_t GetRevision() const { return m_revision.Get(); }

    // Fired from DestroyUnit before the unit is removed, so observers (e.g. WorldView's
    // unit selection) can drop any reference to it while it is still valid.
    Signal<Unit&> OnUnitDestroyed;
    // Fired after CreateUnit finishes (unit is live and visibility has been rebuilt).
    Signal<Unit&> OnUnitCreated;
    // Fired from ReleaseUnit before the unit is removed from this manager (giver side of a
    // transfer). Distinct from OnUnitDestroyed: the unit is not dying. Observers holding a
    // raw Unit* for "my faction's unit" (e.g. WorldView selection) should drop it here even
    // though the pointer itself stays valid — see docs/architecture/high-level.md.
    Signal<Unit&> OnUnitReleased;
    // Fired after AdoptUnit finishes (receiver side of a transfer): unit is live under the
    // new faction and visibility has been rebuilt. Distinct from OnUnitCreated so mods do not
    // see a fake birth for a unit that already existed under another faction.
    Signal<Unit&> OnUnitAdopted;

private:
    void BeginDestructionDeferral_();
    void EndDestructionDeferral_();
    void FlushPendingDestructions_();

    Faction& m_rFaction;
    const MoraleCalculator& m_rMorale;
    std::vector<std::unique_ptr<Unit>> m_units;
    std::vector<std::unique_ptr<Unit>> m_pendingDestructions;
    std::size_t m_destructionDeferralDepth = 0;
    Revision m_revision;
};

} // namespace ac
