#pragma once

#include <optional>

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

    // Every unit this manager creates borrows rMorale.
    UnitManager(Faction& rFaction, const MoraleCalculator& rMorale);
    ~UnitManager() = default;

    // Throws when rTile already holds a unit. pProducedAt is as in Unit's constructor.
    Unit& CreateUnit(UnitId_t unitId, const IDesign& rDesign, UnitPositionIndex& rPositions,
                     const Tile& rTile,
                     BaseManager* pHomeBase = nullptr,
                     std::optional<BaseManager*> pProducedAt = std::nullopt);
    void DestroyUnit(Unit& rUnit);

    // Giver side of an ownership transfer: removes rUnit without destroying it, so no carrier
    // loss and no OnUnitDestroyed. Its cargo links and position are the caller's to handle.
    std::unique_ptr<Unit> ReleaseUnit(Unit& rUnit);
    // Receiver side of an ownership transfer. Emits OnUnitAdopted, not OnUnitCreated.
    Unit& AdoptUnit(std::unique_ptr<Unit> pUnit);

    // Until the scope ends, destroyed units leave the map and Units() but are not freed, so a
    // pass may destroy the unit it is on. Scopes nest.
    [[nodiscard]] DeferredDestructionScope DeferDestruction();
    bool IsPendingDestruction(const Unit& rUnit) const;

    // Live units, skipping the slots deferred destruction leaves empty.
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

    // The next unit needing orders after pAfter, wrapping; the first when pAfter is null or
    // does not need orders. nullptr when no unit needs orders.
    Unit* GetNextAvailableUnit(const Unit* pAfter = nullptr) const;
    bool HasUnitsRequiringOrders() const;

    // Bumped on every unit creation/destruction; consumed by effect-pool caches.
    uint64_t GetRevision() const { return m_revision.Get(); }

    // Before a unit is destroyed, while observers can still read it.
    Signal<Unit&> OnUnitDestroyed;
    // After a unit is created and visibility rebuilt.
    Signal<Unit&, BaseManager*> OnUnitCreated;
    // Before a unit leaves for another faction. Observers holding it as "mine" drop it here.
    Signal<Unit&> OnUnitReleased;
    // After a unit joins from another faction. Not a birth, so not OnUnitCreated.
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
