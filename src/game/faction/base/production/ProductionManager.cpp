#include "game/faction/base/production/ProductionManager.h"

namespace ac
{

static const std::string k_EmptyString;

ProductionManager::ProductionManager()
    : m_pCurrentBuilding(nullptr)
{
}

ProductionManager::~ProductionManager() = default;

void ProductionManager::SetProduction(const Building* pBuilding)
{
    if (!pBuilding)
    {
        ResetProduction_();
        return;
    }

    m_pCurrentBuilding = pBuilding;
    on_production_changed.emit();
}

const Building* ProductionManager::GetCurrentBuilding() const
{
    return m_pCurrentBuilding;
}

const std::string& ProductionManager::GetProduction() const
{
    if (!m_pCurrentBuilding)
    {
        return k_EmptyString;
    }
    return m_pCurrentBuilding->GetName();
}

bool ProductionManager::HasProduction() const
{
    return m_pCurrentBuilding != nullptr;
}

int ProductionManager::GetMineralCost() const
{
    return m_pCurrentBuilding ? m_pCurrentBuilding->GetMineralCost() : 0;
}

std::string ProductionManager::CompleteProduction()
{
    if (!HasProduction())
    {
        return std::string();
    }

    std::string completed = m_pCurrentBuilding->GetBuildingId();
    ResetProduction_();
    on_production_completed.emit(completed);
    return completed;
}

void ProductionManager::ResetProduction_()
{
    bool bHadProduction = HasProduction();
    m_pCurrentBuilding = nullptr;
    if (bHadProduction)
    {
        on_production_changed.emit();
    }
}

} // namespace ac
