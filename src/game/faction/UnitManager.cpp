#include "game/faction/UnitManager.h"
#include "game/units/Unit.h"
#include "game/units/UnitDesign.h"
#include "game/units/MoraleCalculator.h"
#include "game/units/MovementRules.h"
#include "game/units/TransportRules.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/Faction.h"
#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ac
{

UnitManager::UnitManager(Faction& rFaction, const MoraleCalculator& rMorale)
    : m_rFaction(rFaction)
    , m_rMorale(rMorale)
{
}

UnitManager::DeferredDestructionScope::DeferredDestructionScope(UnitManager& rManager)
    : m_pManager(&rManager)
{
    m_pManager->BeginDestructionDeferral_();
}

UnitManager::DeferredDestructionScope::DeferredDestructionScope(
    DeferredDestructionScope&& rOther) noexcept
    : m_pManager(std::exchange(rOther.m_pManager, nullptr))
{
}

UnitManager::DeferredDestructionScope::~DeferredDestructionScope()
{
    if (m_pManager)
    {
        m_pManager->EndDestructionDeferral_();
    }
}

UnitManager::DeferredDestructionScope UnitManager::DeferDestruction()
{
    return DeferredDestructionScope(*this);
}

Unit& UnitManager::CreateUnit(UnitId_t unitId, const UnitDesign& rDesign,
                              UnitPositionIndex& rPositions, const Tile& rTile,
                              BaseManager* pHomeBase, BaseManager* pProducedAt)
{
    if (!CanPlaceUnitOnTile(rTile, rPositions))
    {
        throw std::runtime_error("UnitManager: tile (" + std::to_string(rTile.GetX())
                                 + ", " + std::to_string(rTile.GetY())
                                 + ") already holds a unit (single-unit-per-tile rule)");
    }

    auto pUnit = std::make_unique<Unit>(unitId, rDesign, rPositions, rTile, pHomeBase, m_rFaction,
                                        m_rMorale, pProducedAt);
    Unit& rUnit = *pUnit;
    m_units.push_back(std::move(pUnit));
    m_revision.Bump();
    m_rFaction.RebuildVisibility();
    OnUnitCreated.Emit(rUnit);
    return rUnit;
}

void UnitManager::DestroyUnit(Unit& rUnit)
{
    // Cargo that can hold the carrier's tile by itself is set down there — a ship sunk in
    // port does not drown the garrison; anything over open water goes down with it. Snapshot
    // first, since both branches clear the cargo links. Only passengers this manager owns are
    // ours to destroy; a carrier never holds another faction's units (CanCarryPassenger).
    const std::vector<Unit*> cargo = rUnit.GetCargo();
    const Tile& rCarrierTile = rUnit.GetTile();
    for (Unit* pPassenger : cargo)
    {
        if (!pPassenger || pPassenger == &rUnit)
        {
            continue;
        }
        if (SurvivesCarrierLoss(*pPassenger, rCarrierTile))
        {
            pPassenger->Disembark();
        }
        else if (&pPassenger->GetFaction() == &m_rFaction)
        {
            DestroyUnit(*pPassenger);
        }
    }

    auto it = std::find_if(m_units.begin(), m_units.end(),
        [&rUnit](const std::unique_ptr<Unit>& pUnit)
        {
            return pUnit.get() == &rUnit;
        });

    if (it == m_units.end())
    {
        throw std::runtime_error("Unit not found in UnitManager");
    }

    OnUnitDestroyed.Emit(rUnit);
    if (m_destructionDeferralDepth > 0)
    {
        m_pendingDestructions.push_back(std::move(*it));
        rUnit.DetachFromWorld_();
    }
    else
    {
        m_units.erase(it);
    }
    m_revision.Bump();
    m_rFaction.RebuildVisibility();
}

bool UnitManager::IsPendingDestruction(const Unit& rUnit) const
{
    return std::ranges::any_of(m_pendingDestructions,
        [&rUnit](const std::unique_ptr<Unit>& pUnit) {
            return pUnit.get() == &rUnit;
        });
}

void UnitManager::BeginDestructionDeferral_()
{
    ++m_destructionDeferralDepth;
}

void UnitManager::EndDestructionDeferral_()
{
    if (--m_destructionDeferralDepth == 0)
    {
        FlushPendingDestructions_();
    }
}

void UnitManager::FlushPendingDestructions_()
{
    m_pendingDestructions.clear();
    std::erase(m_units, nullptr);
}

Unit* UnitManager::GetNextAvailableUnit(const Unit* pAfter) const
{
    auto requiresOrders = [](const Unit& rUnit)
    {
        return !rUnit.GetOrder().has_value() && rUnit.GetMoveFragmentsRemaining() > 0;
    };

    Unit* pFirst = nullptr;
    bool bSeenAfter = (pAfter == nullptr);

    for (const std::unique_ptr<Unit>& pUnit : m_units)
    {
        // Null slots exist while a DeferredDestructionScope is open (DestroyUnit moves the
        // owning pointer out but defers compaction); a destroyed unit must not be offered.
        if (!pUnit || !requiresOrders(*pUnit))
        {
            continue;
        }

        if (!pFirst)
        {
            pFirst = pUnit.get();
        }

        if (bSeenAfter)
        {
            return pUnit.get();
        }

        if (pUnit.get() == pAfter)
        {
            bSeenAfter = true;
        }
    }

    return pFirst;
}

bool UnitManager::HasUnitsRequiringOrders() const
{
    return GetNextAvailableUnit() != nullptr;
}

} // namespace ac
