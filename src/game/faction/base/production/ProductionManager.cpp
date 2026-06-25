#include "game/faction/base/production/ProductionManager.h"

#include <algorithm>

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
    return m_pCurrentItem ? m_pCurrentItem->GetMineralCost() : 0;
}

int ProductionManager::GetMineralStockpile() const
{
    return m_mineralStockpile;
}

void ProductionManager::AddMinerals(int amount)
{
    m_mineralStockpile += amount;
}

int ProductionManager::ConsumeMinerals(int amount)
{
    int consumed = std::min(amount, m_mineralStockpile);
    m_mineralStockpile -= consumed;
    return consumed;
}

void ProductionManager::CollectMinerals(int amount)
{
    AddMinerals(amount);
}

std::string ProductionManager::CompleteProduction()
{
    if (!HasProduction())
    {
        return std::string();
    }

    std::string completed = m_pCurrentItem->GetId();
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
