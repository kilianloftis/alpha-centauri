#include "game/units/UnitManager.h"
#include "game/units/Unit.h"
#include "game/units/IDesign.h"
#include "game/combat/MoraleCalculator.h"
#include "game/movement/MovementRules.h"
#include "game/movement/TransportRules.h"
#include "game/map/Tile.h"
#include "game/map/UnitPositionIndex.h"
#include "game/faction/Faction.h"
#include "game/units/Military.h"
#include "game/base/BaseManager.h"
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

Unit& UnitManager::CreateUnit(UnitId_t unitId, const IDesign& rDesign,
                              UnitPositionIndex& rPositions, const Tile& rTile,
                              BaseManager* pHomeBase,
                              std::optional<BaseManager*> pProducedAt)
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
    // After construction, so the unit has already latched IsPrototype.
    m_rFaction.GetMilitary().RecordBuiltComponents(rDesign);
    // Same three-valued production base as Unit's stamp path: omit → home; explicit null → skip.
    BaseManager* pBuiltAt = pProducedAt.has_value() ? *pProducedAt : pHomeBase;
    m_revision.Bump();
    m_rFaction.RebuildVisibility();
    OnUnitCreated.Emit(rUnit, pBuiltAt);
    return rUnit;
}

void UnitManager::DestroyUnit(Unit& rUnit)
{
    // One rebuild for the carrier and everything aboard, not one per hull.
    Faction::VisibilityRebuildScope visibilityScope = m_rFaction.DeferVisibilityRebuild();

    // A copy, since both branches below unlink the passenger.
    const std::vector<Unit*> cargo = rUnit.GetCargo();
    const Tile& rCarrierTile = rUnit.GetTile();
    for (Unit* pPassenger : cargo)
    {
        // A ship sunk in port sets its passengers down; at sea they go down with it.
        if (SurvivesCarrierLoss(*pPassenger, rCarrierTile))
        {
            pPassenger->Disembark();
        }
        else
        {
            DestroyUnit(*pPassenger);
        }
    }

    const auto it = FindOwned_(rUnit);
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

std::unique_ptr<Unit> UnitManager::ReleaseUnit(Unit& rUnit)
{
    const auto it = FindOwned_(rUnit);
    // While the unit is still owned, as DestroyUnit does.
    OnUnitReleased.Emit(rUnit);

    std::unique_ptr<Unit> pReleased = std::move(*it);
    m_units.erase(it);
    m_revision.Bump();
    m_rFaction.RebuildVisibility();
    return pReleased;
}

Unit& UnitManager::AdoptUnit(std::unique_ptr<Unit> pUnit)
{
    if (!pUnit)
    {
        throw std::invalid_argument("UnitManager::AdoptUnit: pUnit is null");
    }
    pUnit->RebindFaction(m_rFaction);
    Unit& rUnit = *pUnit;
    m_units.push_back(std::move(pUnit));
    m_revision.Bump();
    m_rFaction.RebuildVisibility();
    OnUnitAdopted.Emit(rUnit);
    return rUnit;
}

UnitManager::OwnedUnits_t::iterator UnitManager::FindOwned_(const Unit& rUnit)
{
    const auto it = std::ranges::find(m_units, &rUnit, &std::unique_ptr<Unit>::get);
    if (it == m_units.end())
    {
        throw std::runtime_error("UnitManager: unit is not owned by this manager");
    }
    return it;
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
    Unit* pFirst = nullptr;
    bool bSeenAfter = (pAfter == nullptr);

    for (const std::unique_ptr<Unit>& pUnit : m_units)
    {
        // Null slots exist while a DeferredDestructionScope is open (DestroyUnit moves the
        // owning pointer out but defers compaction); a destroyed unit must not be offered.
        if (!pUnit || !pUnit->RequiresOrders())
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
    return GetNextAvailableUnit(nullptr) != nullptr;
}

} // namespace ac
