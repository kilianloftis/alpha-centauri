#include "game/faction/base/production/ProductionManager.h"
#include <stdexcept>

namespace ac
{

ProductionManager::ProductionManager(const BuildingRegistry* pBuildingRegistry)
    : m_pBuildingRegistry(pBuildingRegistry)
    , m_currentItemId()
    , m_accumulatedMinerals(0)
{
}

ProductionManager::~ProductionManager() = default;

void ProductionManager::SetProduction(const std::string& itemId)
{
    if (itemId.empty())
    {
        ResetProduction_();
        return;
    }

    if (!m_pBuildingRegistry || !m_pBuildingRegistry->Find(itemId))
    {
        throw std::runtime_error("Unknown production item: " + itemId);
    }

    m_currentItemId = itemId;
    m_accumulatedMinerals = 0;
    on_production_changed.emit();
}

const std::string& ProductionManager::GetProduction() const
{
    return m_currentItemId;
}

bool ProductionManager::HasProduction() const
{
    return !m_currentItemId.empty();
}

int ProductionManager::GetAccumulatedMinerals() const
{
    return m_accumulatedMinerals;
}

int ProductionManager::GetMineralCost() const
{
    const BuildingConfig_t* pConfig = FindConfig_();
    return pConfig ? pConfig->mineralCost : 0;
}

bool ProductionManager::CanComplete() const
{
    return HasProduction() && m_accumulatedMinerals >= GetMineralCost();
}

std::string ProductionManager::ProgressProduction(int minerals)
{
    if (!HasProduction() || minerals <= 0)
    {
        return std::string();
    }

    m_accumulatedMinerals += minerals;
    if (m_accumulatedMinerals >= GetMineralCost())
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

    std::string completed = m_currentItemId;
    ResetProduction_();
    on_production_completed.emit(completed);
    return completed;
}

void ProductionManager::ResetProduction_()
{
    bool bHadProduction = HasProduction();
    m_currentItemId.clear();
    m_accumulatedMinerals = 0;
    if (bHadProduction)
    {
        on_production_changed.emit();
    }
}

const BuildingConfig_t* ProductionManager::FindConfig_() const
{
    if (!m_pBuildingRegistry || m_currentItemId.empty())
    {
        return nullptr;
    }
    return m_pBuildingRegistry->Find(m_currentItemId);
}

} // namespace ac
