#include "game/faction/base/production/ProductionManager.h"
#include "game/faction/base/production/ProductionCostCalculator.h"

namespace ac
{

ProductionManager::ProductionManager(const ProductionConfig_t& rConfig)
    : m_rConfig(rConfig)
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

    if (pItem == m_pCurrentItem)
    {
        return;
    }

    ApplyRetoolPenalty_(pItem);
    m_pCurrentItem = pItem;
    OnProductionChanged.Emit();
}

void ProductionManager::ApplyRetoolPenalty_(const IConstructable* pNewItem)
{
    // No turn original yet
    if (!m_pTurnOriginalItem)
    {
        return;
    }
    // Free to go back to what the base was building when the turn handed over.
    if (pNewItem == m_pTurnOriginalItem)
    {
        return;
    }
    if (m_mineralStockpile <= m_rConfig.retoolPenaltyThreshold)
    {
        return;
    }

    // Integer division rounds the loss down, so the remainder favours the player.
    const int forfeited = m_mineralStockpile * m_rConfig.retoolPenaltyPercent / 100;
    m_mineralStockpile -= forfeited;
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
        m_pTurnOriginalItem = nullptr;
        return std::string();
    }

    m_mineralStockpile += minerals;

    std::string completed;
    if (m_mineralStockpile >= GetMineralCost(rBaseEffects))
    {
        completed = CompleteProduction();
    }

    // Whatever is queued once this turn's minerals are banked is what the player sees when
    // PlayerActions hands over, so it is the item a retool this turn is measured against.
    m_pTurnOriginalItem = m_pCurrentItem;
    return completed;
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
