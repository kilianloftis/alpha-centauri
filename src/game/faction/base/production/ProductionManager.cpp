#include "game/faction/base/production/ProductionManager.h"

namespace ac
{

ProductionManager::ProductionManager(const ProductionCostCalculator& rCostCalculator)
    : m_pCostCalculator(&rCostCalculator)
    , m_pCurrentItem(nullptr)
    , m_mineralStockpile(0)
{
}

ProductionManager::~ProductionManager() = default;

void ProductionManager::SetProduction(const IConstructable* pItem)
{
    if (!pItem)
    {
        ResetProduction_();
        return;
    }

    m_pCurrentItem = pItem;
    on_production_changed.emit();
}

const IConstructable* ProductionManager::GetCurrentProduction() const
{
    return m_pCurrentItem;
}

bool ProductionManager::HasProduction() const
{
    return m_pCurrentItem != nullptr;
}

int ProductionManager::GetMineralCost() const
{
    if (!m_pCurrentItem || !m_pCostCalculator)
    {
        return 0;
    }
    // TODO: pass real industry_rating once base industry modifiers exist
    return m_pCostCalculator->ComputeCost(m_pCurrentItem->GetBaseCost(), 0);
}

int ProductionManager::GetMineralStockpile() const
{
    return m_mineralStockpile;
}

std::string ProductionManager::ApplyProduction(int minerals)
{
    if (!HasProduction())
    {
        return std::string();
    }

    m_mineralStockpile += minerals;

    if (m_mineralStockpile >= GetMineralCost())
    {
        return CompleteProduction();
    }

    return std::string();
}

std::string ProductionManager::CompleteProduction()
{
    if (!HasProduction())
    {
        return std::string();
    }

    std::string completed = m_pCurrentItem->GetId();
    m_mineralStockpile = 0;
    ResetProduction_();
    on_production_completed.emit(completed);
    return completed;
}

void ProductionManager::ResetProduction_()
{
    bool bHadProduction = HasProduction();
    m_pCurrentItem = nullptr;
    if (bHadProduction)
    {
        on_production_changed.emit();
    }
}

} // namespace ac
