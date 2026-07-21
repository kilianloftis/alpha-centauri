#include "game/faction/base/production/ProductionManager.h"
#include "game/faction/base/production/ProductionCostCalculator.h"

namespace ac
{

ProductionManager::ProductionManager()
    : m_pCurrentItem(nullptr)
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
    OnProductionChanged.Emit();
}

const IConstructable* ProductionManager::GetCurrentProduction() const
{
    return m_pCurrentItem;
}

bool ProductionManager::HasProduction() const
{
    return m_pCurrentItem != nullptr;
}

int ProductionManager::GetMineralCost(const BaseEffects_t& rBaseEffects) const
{
    if (!m_pCurrentItem)
    {
        return 0;
    }
    return ProductionCostCalculator::ComputeCost(m_pCurrentItem->GetBaseCost(), rBaseEffects);
}

int ProductionManager::GetMineralStockpile() const
{
    return m_mineralStockpile;
}

void ProductionManager::SetMineralStockpile(int amount)
{
    m_mineralStockpile = amount;
}

std::string ProductionManager::ApplyProduction(int minerals, const BaseEffects_t& rBaseEffects)
{
    if (!HasProduction())
    {
        return std::string();
    }

    m_mineralStockpile += minerals;

    if (m_mineralStockpile >= GetMineralCost(rBaseEffects))
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
    OnProductionCompleted.Emit(completed);
    return completed;
}

void ProductionManager::ResetProduction_()
{
    bool bHadProduction = HasProduction();
    m_pCurrentItem = nullptr;
    if (bHadProduction)
    {
        OnProductionChanged.Emit();
    }
}

} // namespace ac
