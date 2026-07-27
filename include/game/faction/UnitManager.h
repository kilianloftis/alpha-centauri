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
struct MoraleConfig_t;

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

    explicit UnitManager(Faction& rFaction);
    ~UnitManager() = default;

    // The unit registers itself on rTile in rPositions for its lifetime (see Unit's
    // constructor). Rejects the tile under the stacking rule (MovementRules) before
    // constructing. unitId must be unique across the game (caller: GameState::AllocateUnitId).
    // pProducedAt defaults to pHomeBase when null (see Unit ctor).
    // rMorale is game-wide static data from GameDataContext.
    Unit& CreateUnit(UnitId_t unitId, const UnitDesign& rDesign, UnitPositionIndex& rPositions,
                     const Tile& rTile, const MoraleConfig_t& rMorale,
                     BaseManager* pHomeBase = nullptr, BaseManager* pProducedAt = nullptr);
    void DestroyUnit(Unit& rUnit);

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

private:
    void BeginDestructionDeferral_();
    void EndDestructionDeferral_();
    void FlushPendingDestructions_();

    Faction& m_rFaction;
    std::vector<std::unique_ptr<Unit>> m_units;
    std::vector<std::unique_ptr<Unit>> m_pendingDestructions;
    std::size_t m_destructionDeferralDepth = 0;
    Revision m_revision;
};

} // namespace ac
